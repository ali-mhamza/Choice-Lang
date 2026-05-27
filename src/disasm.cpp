#include "../include/disasm.h"
#include "../include/bytecode.h"
#include "../include/common.h"
#include "../include/config.h"
#include "../include/diagnostic.h"
#include "../include/natives.h"
#include "../include/object.h"
#include "../include/opcodes.h"
#include <string_view>

#define PRINT_FULL_OFFSET 1

Disassembler::Disassembler(const Function* function) :
	func{function}, ip{func->code.block.begin()},
	start{func->code.block.begin()} {}

void Disassembler::printOpcode(std::string_view opName) const
{
	#if PRINT_FULL_OFFSET
		CH_PRINT("{:0>4} {:<15} ", ip - start, opName);
	#else
		// Prints leading spaces, not zeros.
		CH_PRINT("{:>4} {:<15} ", ip - start, opName);
	#endif
}

void Disassembler::disFunction(const Function* func) const
{
	if (func->lambda)
		CH_PRINT("\n===== [start] <lambda> =====\n\n");
	else
		CH_PRINT("\n===== [start] func {} =====\n\n", func->name);

	Disassembler miniDis{func};
	miniDis.topLevel = false;
	miniDis.disassembleCode();

	if (func->lambda)
		CH_PRINT("\n====== [end] <lambda> ======\n\n");
	else
		CH_PRINT("\n====== [end] func {} ======\n\n", func->name);
}

void Disassembler::printOperValue(const Object& oper) const
{
	CH_PRINT("'{}' {}\n",
		oper.printVal(), oper.printType());
	// We only disassemble functions when requested, and not
	// with concurrent disassembler output during VM execution.
	if (DIS_FUNCTION_OBJS && IS_FUNCOBJ(oper) && !inVM)
		disFunction(AS_FUNC(oper));
}

u8 Disassembler::restoreByte() const
{
	return ip[1];
}

u16 Disassembler::restoreShort() const
{
	u16 value{static_cast<u16>(
		(ip[1] << 8) | (ip[2])
	)};

	return value;
}

u32 Disassembler::restoreLong() const
{
	u32 value{static_cast<u32>(
		(ip[1] << 24)
		| (ip[2] << 16)
		| (ip[3] << 8)
		| ip[4]
	)};

	return value;
}

void Disassembler::singleOper(u8 byte)
{
	printOpcode(opNames[byte]);
	CH_PRINT("R[{}]\n", ip[1]);

	ip += 2;
}

void Disassembler::doubleOper(u8 byte)
{
	printOpcode(opNames[byte]);

	Opcode op{static_cast<Opcode>(byte)};
	if (op == OP_GET_CELL)
		CH_PRINT("R[{}] C[{}]\n", ip[1], ip[2]);
	else if (op == OP_SET_CELL)
		CH_PRINT("C[{}] R[{}]\n", ip[1], ip[2]);
	else
		CH_PRINT("R[{}] R[{}]\n", ip[1], ip[2]);

	ip += 3;
}

void Disassembler::loadOp()
{
	printOpcode("OP_LOAD_R");
	CH_PRINT("R[{}] ", ip[1]);

	ip += 2;
	switch (*ip)
	{
		case OP_BYTE_OPER:
		{
			u8 operand{restoreByte()};
			CH_PRINT("C[{}] ", operand);
			printOperValue(func->code.pool[operand]);
			ip += 2;
			break;
		}
		case OP_SHORT_OPER:
		{
			u16 operand{restoreShort()};
			CH_PRINT("C[{}] ", operand);
			printOperValue(func->code.pool[operand]);
			ip += 3;
			break;
		}
		case OP_LONG_OPER:
		{
			u32 operand{restoreLong()};
			CH_PRINT("C[{}] ", operand);
			printOperValue(func->code.pool[operand]);
			ip += 5;
			break;
		}
		default: // Direct constant loading instruction.
			CH_PRINT("{}\n", opNames[*ip]);
			ip++;
	}
}

void Disassembler::jumpOp(u8 byte, int sign)
{
	printOpcode(opNames[byte]);
	if ((byte == OP_JUMP_TRUE) || (byte == OP_JUMP_FALSE))
	{
		u8 reg{restoreByte()};
		ip++;
		CH_PRINT("R[{}] ", reg);
	}

	u16 jump{restoreShort()};
	ip += 3;
	CH_PRINT("-> {}\n", ip - start + (sign * jump));
}

void Disassembler::callOp(u8 byte)
{
	printOpcode(opNames[byte]);
	u8 callee{restoreByte()};
	ip++;
	u8 start{restoreByte()};
	ip++;
	u8 count{restoreByte()};
	ip += 2;

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

	if (static_cast<Opcode>(byte) == OP_MAKE_ITER)
	{
		CH_PRINT("R[{}] R[{}]\n", ip[1], ip[2]);
		ip += 3;
	}
	else if (static_cast<Opcode>(byte) == OP_UPDATE_ITER)
	{
		ip += 2;
		u16 jump{restoreShort()};
		ip += 3;
		CH_PRINT("R[{}] R[{}] -> {}\n", ip[-4], ip[-3],
			ip - start - jump);
	}
}

void Disassembler::indexOp(u8 byte)
{
	printOpcode(opNames[byte]);

	u8 objReg{restoreByte()};
	ip++;

	u8 indexReg{restoreByte()};
	ip++;

	u8 tempReg{restoreByte()};
	ip += 2;

	CH_PRINT("R[{}] R[{}] R[{}]\n", objReg, indexReg, tempReg);
}

void Disassembler::collectionOp(u8 byte)
{
	printOpcode(opNames[byte]);

	u8 reg{restoreByte()};
	ip++;

	if ((static_cast<Opcode>(byte) == OP_EXT_LIST)
		|| (static_cast<Opcode>(byte) == OP_EXT_TABLE))
	{
		u8 startReg{restoreByte()};
		ip++;

		u8 count{restoreByte()};
		ip += 2;

		CH_PRINT("R[{}] R[{}] ({})\n", reg, startReg, count);
	}
	else
	{
		CH_PRINT("R[{}]\n", reg);
		ip++;
	}
}

void Disassembler::captureOp(u8 byte)
{
	printOpcode(opNames[byte]);
	u8 funcReg{restoreByte()};
	ip++;

	if (static_cast<Opcode>(byte) == OP_CAPTURE_VAL)
		CH_PRINT("F[{}] R[{}]\n", funcReg, ip[1]);
	else
		CH_PRINT("F[{}] C[{}]\n", funcReg, ip[1]);
	ip += 2;
}

void Disassembler::referenceOp()
{
    // Mimicking the compiler.
    enum VarType : u8 { GLOBAL, CELL, LOCAL };

	printOpcode(opNames[OP_MAKE_REF]);

	u8 reg{restoreByte()};
	ip++;
	CH_PRINT("R[{}] ", reg);

	VarType type{static_cast<VarType>(restoreByte())};
	ip++;

	u8 target{restoreByte()};
	ip += 2;

	switch (type)
	{
		case GLOBAL:	CH_PRINT("GLOBAL ");	break;
		case CELL:		CH_PRINT("CELL ");		break;
		case LOCAL:		CH_PRINT("LOCAL ");		break;
	}

	CH_PRINT("R[{}]\n", target);
}

void Disassembler::formatOp()
{
	printOpcode(opNames[OP_FORMAT_STR]);

	u8 reg{restoreByte()};
	ip++;

	u8 count{restoreByte()};
	ip += 2;

	CH_PRINT("R[{}] ({})\n", reg, count);
}

void Disassembler::declOp()
{
	printOpcode(opNames[OP_DEF_START]);
	CH_PRINT("V[{}]\n", ip[1]);

	ip += 2;
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
		case OP_IN:			case OP_MOVE_R:		case OP_RANGE:
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
		case OP_CLOSURE:	case OP_NEG:		case OP_NOT:		case OP_INCR:
		case OP_DECR:		case OP_COMP:		case OP_RETURN:		case OP_VOID:
		case OP_VAR:        case OP_FIX:		case OP_IMMUT:		case OP_MUT:
		case OP_ENTER_SCOPE:    case OP_PRINT_VALID:
			singleOper(byte);
			break;
		case OP_LOAD_R:
			loadOp();
			break;
		case OP_LIST:		case OP_EXT_LIST:	case OP_TABLE:		case OP_EXT_TABLE:
			collectionOp(byte);
			break;
		case OP_CAPTURE_VAL:	case OP_CAPTURE_CELL:
			captureOp(byte);
			break;
		case OP_MAKE_REF:	referenceOp();	break;
		case OP_FORMAT_STR:	formatOp();		break;
		case OP_DEF_START:	declOp();		break;
		case OP_EXIT_SCOPE:		case OP_DEF_END:	case OP_HALT:
		{
			#if PRINT_FULL_OFFSET
				CH_PRINT("{:0>4} {}\n", ip - start, opNames[byte]);
			#else
				CH_PRINT("{:>4} {}\n", ip - start, opNames[byte]);
			#endif
			ip++;
			break;
		}
		default:
		{
			CH_PRINT("{:0>4} UNKNOWN OPCODE {}\n",
				ip - start, byte);
			ip++;
			break;
		}
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

	CH_PRINT("(bytes: {}, args: {}, constants: {})\n\n",
		func->code.block.size(), func->argCount, func->code.pool.size());

	while (ip < end)
		disassembleOp(*ip);
}