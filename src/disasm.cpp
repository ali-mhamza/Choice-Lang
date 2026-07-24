/*
 * All code for the disassembler to display a ByteCode chunk/script
 * in an intelligible manner for inspection, debugging, etc.
 */

#include "../include/disasm.h"
#include "../include/bytecode.h"
#include "../include/common.h"
#include "../include/config.h"
#include "../include/diagnostic.h"
#include "../include/natives.h"
#include "../include/object.h"
#include "../include/opcodes.h"
#include <string_view>

#if OPCODE_STRIP_PREFIX
    #define OP_STRING "{:<15} "
#else
    #define OP_STRING "{:<18} "
#endif

#define PRINT_FULL_OFFSET 1

Disassembler::Disassembler(const Function* function) :
	func{function}, ip{func->code.block.begin()},
	start{func->code.block.begin()} {}

void Disassembler::printOpcode(std::string_view opName)
{
	#if PRINT_FULL_OFFSET
		CH_PRINT("{:0>4} " OP_STRING, ip - start, opName);
	#else
		// Prints leading spaces, not zeros.
		CH_PRINT("{:>4} " OP_STRING, ip - start, opName);
	#endif

	ip++;
}

void Disassembler::printBareOpcode(std::string_view opName)
{
	#if PRINT_FULL_OFFSET
		CH_PRINT("{:0>4} {}\n", ip - start, opName);
	#else
		CH_PRINT("{:>4} {}\n", ip - start, opName);
	#endif

	ip++;
}

#undef PRINT_FULL_OFFSET

void Disassembler::disFunction(const Function* func) const
{
	if (func->name == nullptr)
		CH_PRINT("\n===== [start] <lambda> =====\n\n");
	else
		CH_PRINT("\n===== [start] func {} =====\n\n", func->name);

	Disassembler miniDis{func};
	miniDis.topLevel = false;
	miniDis.disassembleCode();

	if (func->name == nullptr)
		CH_PRINT("\n====== [end] <lambda> ======\n\n");
	else
		CH_PRINT("\n====== [end] func {} ======\n\n", func->name);
}

void Disassembler::printOperValue(const Object& oper) const
{
	CH_PRINT("'{}' {}\n", oper.printVal(), oper.printType());

	// We only disassemble functions when requested, and not
	// with concurrent disassembler output during VM execution.
	if (DIS_FUNCTION_OBJS && IS_FUNCOBJ(oper) && !inVM)
		disFunction(AS_USER_FUNC(oper));
}

u8 Disassembler::readByte()
{
	u8 byteVal{*ip};
	ip += sizeof(u8);

	return byteVal;
}

u16 Disassembler::readShort()
{
	u16 shortVal{static_cast<u16>(
		(ip[0] << 8) | (ip[1])
	)};

	ip += sizeof(u16);
	return shortVal;
}

u32 Disassembler::readLong()
{
	u32 longVal{static_cast<u32>(
	      (ip[0] << 24)
		| (ip[1] << 16)
		| (ip[2] << 8)
		| ip[3]
	)};

	ip += sizeof(u32);
	return longVal;
}

void Disassembler::singleOper(u8 byte)
{
	printOpcode(opNames[byte]);
	CH_PRINT("R[{}]\n", readByte());
}

void Disassembler::doubleOper(u8 byte)
{
	printOpcode(opNames[byte]);
	u8 first{readByte()}, second{readByte()};

	Opcode op{static_cast<Opcode>(byte)};
	if ((op == OP_VAR) || (op == OP_FIX))
        CH_PRINT("R[{}] ({})\n", first, second);
	else if (op == OP_GET_CELL)
		CH_PRINT("R[{}] C[{}]\n", first, second);
	else if (op == OP_SET_CELL)
		CH_PRINT("C[{}] R[{}]\n", first, second);
	else
		CH_PRINT("R[{}] R[{}]\n", first, second);
}

void Disassembler::loadOp()
{
	printOpcode(opNames[OP_LOAD_REG]);
	CH_PRINT("R[{}] ", readByte());

	switch (readByte())
	{
		case OP_BYTE_OPER:
		{
			u8 operand{readByte()};
			CH_PRINT("C[{}] ", operand);
			printOperValue(func->code.pool[operand]);
			break;
		}
		case OP_SHORT_OPER:
		{
			u16 operand{readShort()};
			CH_PRINT("C[{}] ", operand);
			printOperValue(func->code.pool[operand]);
			break;
		}
		case OP_LONG_OPER:
		{
			u32 operand{readLong()};
			CH_PRINT("C[{}] ", operand);
			printOperValue(func->code.pool[operand]);
			break;
		}
		default: // Direct constant loading instruction.
			CH_PRINT("{}\n", opNames[ip[-1]]);
	}
}

void Disassembler::jumpOp(u8 byte, int sign)
{
	printOpcode(opNames[byte]);
	if ((byte == OP_JUMP_TRUE) || (byte == OP_JUMP_FALSE))
	{
		u8 reg{readByte()};
		CH_PRINT("R[{}] ", reg);
	}

	u16 jump{readShort()};
	CH_PRINT("-> {}\n", ip - start + (sign * jump));
}

void Disassembler::callOp(u8 byte)
{
	printOpcode(opNames[byte]);
	u8 callee{readByte()};
	u8 start{readByte()};
	u8 count{readByte()};

	if (byte == OP_CALL_NAT)
	{
		std::string_view func{Natives::funcNames[callee]};
		CH_PRINT("'{}' R[{}] ({})\n", func, start, count);
	}
	else
	{
		// We only save the register that the function object
		// will be in by the time it is called.
		// Since registers and their contents are only available
		// at runtime, we cannot display any information about the
		// function besides its expected location when only
		// disassembling bytecode.
		CH_PRINT("F[{}] R[{}] ({})\n", callee, start, count);
	}
}

void Disassembler::iterOp(u8 byte)
{
	printOpcode(opNames[byte]);
	u8 varReg{readByte()}, iterReg{readByte()};

	if (static_cast<Opcode>(byte) == OP_MAKE_ITER)
		CH_PRINT("R[{}] R[{}]\n", varReg, iterReg);
	else if (static_cast<Opcode>(byte) == OP_UPDATE_ITER)
	{
		u16 jump{readShort()};
		CH_PRINT("R[{}] R[{}] -> {}\n", varReg, iterReg,
			ip - start - jump);
	}
}

void Disassembler::indexOp(u8 byte)
{
	printOpcode(opNames[byte]);

	u8 objReg{readByte()};
	u8 indexReg{readByte()};
	u8 tempReg{readByte()};

	CH_PRINT("R[{}] R[{}] R[{}]\n", objReg, indexReg, tempReg);
}

void Disassembler::collectionOp(u8 byte)
{
	printOpcode(opNames[byte]);
	u8 reg{readByte()};

	if ((static_cast<Opcode>(byte) == OP_EXT_LIST)
		|| (static_cast<Opcode>(byte) == OP_EXT_TABLE))
	{
		u8 startReg{readByte()};
		u8 count{readByte()};
		CH_PRINT("R[{}] R[{}] ({})\n", reg, startReg, count);
	}
	else
		CH_PRINT("R[{}]\n", reg);
}

void Disassembler::typeOp(u8 byte)
{
    printOpcode(opNames[byte]);
    u8 first{readByte()}, second{readByte()}, third{readByte()};
    CH_PRINT("R[{}] R[{}] R[{}]\n", first, second, third);
}

void Disassembler::moduleOp(u8 byte)
{
    printOpcode(opNames[byte]);
    if (static_cast<Opcode>(byte) == OP_GET_ENTRY)
    {
        u8 first{readByte()}, second{readByte()}, third{readByte()};
        CH_PRINT("R[{}] R[{}] R[{}]\n", first, second, third);
    }
}

void Disassembler::captureOp(u8 byte)
{
	printOpcode(opNames[byte]);
	u8 funcReg{readByte()};

	if ((static_cast<Opcode>(byte) == OP_CAPTURE_GLOBAL)
	    || (static_cast<Opcode>(byte) == OP_CAPTURE_LOCAL))
	{
	    CH_PRINT("F[{}] R[{}]\n", funcReg, readByte());
	}
	else
		CH_PRINT("F[{}] C[{}]\n", funcReg, readByte());
}

void Disassembler::unpackOp(u8 byte)
{
    printOpcode(opNames[byte]);
    u8 reg{readByte()}, count{readByte()};
    bool first{static_cast<bool>(readByte())};
    bool second{static_cast<bool>(readByte())};

    CH_PRINT("R[{}] ({}) ({}) ({})\n", reg, count, first, second);
}

void Disassembler::referenceOp(u8 byte)
{
    // Mimicking the compiler.
    enum class VarType : u8 { Global, Cell, Local };

	printOpcode(opNames[byte]);
	u8 reg{readByte()};
	CH_PRINT("R[{}] ", reg);

	if ((static_cast<Opcode>(byte) == OP_INDEX_REF)
	    || (static_cast<Opcode>(byte) == OP_FIELD_REF))
	{
	    u8 field{readByte()};
		CH_PRINT("R[{}]\n", field);
	}
	else if (static_cast<Opcode>(byte) == OP_VAR_REF)
	{
    	VarType type{static_cast<VarType>(readByte())};
    	u8 target{readByte()};

    	switch (type)
    	{
    		case VarType::Global:	CH_PRINT("GLOBAL ");	break;
    		case VarType::Cell:		CH_PRINT("CELL ");		break;
    		case VarType::Local:	CH_PRINT("LOCAL ");		break;
    	}

    	CH_PRINT("R[{}]\n", target);
	}
}

void Disassembler::formatOp()
{
	printOpcode(opNames[OP_FORMAT_STR]);

	u8 reg{readByte()};
	u8 count{readByte()};
	CH_PRINT("R[{}] ({})\n", reg, count);
}

void Disassembler::declOp()
{
	printOpcode(opNames[OP_DEF_START]);
	CH_PRINT("V[{}]\n", readByte());
}

void Disassembler::disassembleOp(u8 byte)
{
	switch (byte)
	{
		case OP_ADD:		case OP_SUB:		case OP_MULT:		case OP_DIV:
		case OP_MOD:		case OP_POWER:		case OP_AND:		case OP_OR:
		case OP_XOR:		case OP_SHIFT_R:	case OP_SHIFT_L:	case OP_GET_GLOBAL:
		case OP_SET_GLOBAL:	case OP_GET_CELL:	case OP_SET_CELL:	case OP_GET_LOCAL:
		case OP_SET_LOCAL:	case OP_EQUAL:		case OP_GT:			case OP_LT:
		case OP_VAR:        case OP_FIX:		case OP_IN:			case OP_MOVE_REG:
		case OP_RANGE:      case OP_METHOD:     case OP_MODULE:
			doubleOper(byte);
			break;
		case OP_JUMP:		case OP_JUMP_TRUE:	case OP_JUMP_FALSE:		case OP_LOOP:
			jumpOp(byte, byte == OP_LOOP ? -1 : 1);
			break;
		case OP_MAKE_ITER:	case OP_UPDATE_ITER:
			iterOp(byte);
			break;
		case OP_CALL_NAT:	case OP_CALL_DEF:
			callOp(byte);
			break;
		case OP_GET_INDEX:	case OP_SET_INDEX:
			indexOp(byte);
			break;
		case OP_CLOSURE:	    case OP_NEG:		    case OP_NOT:		    case OP_INCR:
		case OP_DECR:		    case OP_COMP:		    case OP_RETURN:		    case OP_VOID:
		case OP_IMMUT:		    case OP_MUT:            case OP_INSTANCE:       case OP_FINISH_FIELDS:
		case OP_ENTER_SCOPE:    case OP_VAR_ARGS:		case OP_COMPUTE:
		case OP_PRINT_VALID:
			singleOper(byte);
			break;
		case OP_LOAD_REG:
			loadOp();
			break;
		case OP_LIST:		case OP_EXT_LIST:	case OP_TABLE:		case OP_EXT_TABLE:
			collectionOp(byte);
			break;
		case OP_INIT_FIELD:     case OP_GET_FIELD:  case OP_SET_FIELD:
            typeOp(byte);
            break;
        case OP_GET_ENTRY:
            moduleOp(byte);
            break;
		case OP_CAPTURE_GLOBAL:     case OP_CAPTURE_LOCAL:      case OP_CAPTURE_CELL:
			captureOp(byte);
			break;
		case OP_UNPACK:
		    unpackOp(byte);
			break;
		case OP_VAR_REF:    case OP_INDEX_REF:      case OP_FIELD_REF:
		    referenceOp(byte);
			break;
		case OP_FORMAT_STR:	formatOp();		break;
		case OP_DEF_START:	declOp();		break;
		case OP_EXIT_SCOPE:		case OP_DEF_END:	case OP_HALT:
			printBareOpcode(opNames[byte]);
			break;
		default:
			printBareOpcode(CH_STR("UNKNOWN OPCODE {}", byte));
			break;
	}
}

void Disassembler::disassembleCode()
{
	// We only abort completely if a compilation error occurred.
	// Bytecode for empty input is still displayed.
	if (func->code.codeSize() == 0)
		return;

	inVM = false;
	auto end{func->code.block.end()};

	// ip < end -> We have some bytecode to print.
	if (topLevel && !inRepl && (ip < end))
		CH_PRINT("=== CODE [{}] ===\n", sourceManager.getFile(func->code.id));

	CH_PRINT("(bytes: {}, ", func->code.codeSize());
	if (func->variadic)
	    CH_PRINT("args: {}-{}, ", func->arityMin, CODE_MAX);
	else
	{
		if (func->arityMin == func->arityMax)
			CH_PRINT("args: {}, ", func->arityMax);
		else
			CH_PRINT("args: {}-{}, ", func->arityMin, func->arityMax);
	}
	CH_PRINT("constants: {})\n\n", func->code.pool.size());

	while (ip < end)
		disassembleOp(*ip);
}

#undef PRINT_FULL_OFFSET
#undef OP_STRING