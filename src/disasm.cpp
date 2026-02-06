#include "../include/disasm.h"
#include "../include/common.h"
#include "../include/natives.h"
#include "../include/opcodes.h"

#define PRINT_FULL_OFFSET 1

Disassembler::Disassembler(const ByteCode& code) :
	code(code), ip(code.block.begin()),
	start(code.block.begin()), topLevel(true),
	inVM(true) {}

void Disassembler::printOpcode(std::string_view opName)
{
	#if PRINT_FULL_OFFSET
		FORMAT_PRINT("{:0>4} {:<15} ", ip - start, opName);
	#else
		FORMAT_PRINT("{:>4} {:<15} ", ip - start, opName); // Prints leading spaces, not zeros.
	#endif
}

void Disassembler::disFunction(const Function& func)
{
	FORMAT_PRINT("===== [start] func {} =====\n", func.name);
	Disassembler miniDis(func.code);
	miniDis.topLevel = false;
	miniDis.disassembleCode();
	FORMAT_PRINT("====== [end] func {} ======\n", func.name);
}

void Disassembler::printOperValue(const Object& oper)
{
	FORMAT_PRINT("'{}' {}\n",
		oper.printVal(), oper.printType());
	if (IS_FUNC(oper) && !inVM)
		disFunction(AS_FUNC(oper));
}

ui8 Disassembler::restoreByte()
{
	return *(ip + 1);
}

ui16 Disassembler::restoreShort()
{
	ui16 value = static_cast<ui16>(
		(*(ip + 1) << 8) | *(ip + 2)
	);

	return value;
}

ui32 Disassembler::restoreLong()
{
	ui32 value = static_cast<ui32>(
		(*(ip + 1) << 24) | (*(ip + 2) << 16)
		| (*(ip + 3) << 8) | *(ip + 4)
	);

	return value;
}

void Disassembler::singleOper(std::string_view opName)
{
	printOpcode(opName);
	FORMAT_PRINT("R[{}]\n", *(ip + 1));
	
	ip += 2;
}

void Disassembler::doubleOper(std::string_view opName)
{
	printOpcode(opName);

	for (int i = 0; i < 2; i++)
		FORMAT_PRINT("R[{}] ", *(ip + i + 1));
	FORMAT_PRINT("\n");

	ip += 3;
}

void Disassembler::tripleOper(std::string_view opName)
{
	printOpcode(opName);

	for (int i = 0; i < 3; i++)
		FORMAT_PRINT("R[{}] ", *(ip + i + 1));
	FORMAT_PRINT("\n");
	
	ip += 4;
}

void Disassembler::loadOper(std::string_view opName)
{
	printOpcode(opName);
	FORMAT_PRINT("R[{}] ", *(ip + 1));

	ip += 2;
	switch (*ip)
	{
		case OP_BYTE_OPER:
		{
			ui8 operand = restoreByte();
			FORMAT_PRINT("C[{}] ", operand);
			printOperValue(code.pool[operand]);
			ip += 2;
			break;
		}
		case OP_SHORT_OPER:
		{
			ui16 operand = restoreShort();
			FORMAT_PRINT("C[{}] ", operand);
			printOperValue(code.pool[operand]);
			ip += 3;
			break;
		}
		case OP_LONG_OPER:
		{
			ui32 operand = restoreLong();
			FORMAT_PRINT("C[{}] ", operand);
			printOperValue(code.pool[operand]);
			ip += 5;
			break;
		}
		default: // Direct constant loading instruction.
			FORMAT_PRINT("{}\n", opNames[*ip]);
			ip += 1;
	}
}

void Disassembler::jumpOper(ui8 byte, int sign)
{
	printOpcode(opNames[byte]);
	if ((byte == OP_JUMP_TRUE) || (byte == OP_JUMP_FALSE))
	{
		ui8 reg = restoreByte();
		ip++;
		FORMAT_PRINT("R[{}] ", reg);
	}
	ui16 jump = restoreShort();
	ip += 3;
	FORMAT_PRINT("-> {}\n", ip - start + (sign * jump));
}

void Disassembler::callOper(std::string_view opName, bool builtin)
{
	printOpcode(opName);

	ui8 callee = restoreByte();
	ip++;
	ui8 start = restoreByte();
	ip++;
	ui8 count = restoreByte();
	ip += 2;

	if (builtin)
	{
		std::string_view func = Natives::funcNames[callee];
		FORMAT_PRINT("'{}' ({}) R[{}]\n", func, count, start);
	}
	else
	{
		// We only save the register that the function object
		// will be in by the time it is called.
		// Since registers and their contents are only available
		// at runtime, we cannot display any information about the
		// function besides its expected location when only
		// disassembling bytecode.
		FORMAT_PRINT("F[{}] ({}) R[{}]\n", callee, count, start);
	}
}

void Disassembler::iterOper(ui8 byte)
{
	printOpcode(opNames[byte]);
	
	if (static_cast<Opcode>(byte) == OP_MAKE_ITER)
	{
		FORMAT_PRINT("R[{}] R[{}]\n", *(ip + 1), *(ip + 2));
		ip += 3;
	}
	else if (static_cast<Opcode>(byte) == OP_UPDATE_ITER)
	{
		ip += 2;
		ui16 jump = restoreShort();
		ip += 3;
		FORMAT_PRINT("R[{}] R[{}] -> {}\n", *(ip - 4), *(ip -3),
			ip - start - jump);
	}
}

void Disassembler::disassembleOp(ui8 byte)
{
	switch (byte)
	{
		case OP_ADD:		case OP_SUB:		case OP_MULT:			case OP_DIV:
		case OP_MOD:		case OP_POWER:		case OP_EQUAL:			case OP_GT:
		case OP_LT:			case OP_IN:			case OP_BIT_AND:		case OP_BIT_OR:
		case OP_BIT_XOR:	case OP_BIT_SHIFT_R:	case OP_BIT_SHIFT_L:
			tripleOper(opNames[byte]);
			break;
		case OP_GET_VAR:	case OP_SET_VAR:	case OP_INCREMENT:	case OP_DECREMENT:
		case OP_NEGATE:		case OP_NOT:		case OP_BIT_COMP:	case OP_MOVE_R:
			doubleOper(opNames[byte]);
			break;
		case OP_JUMP:		case OP_JUMP_TRUE:	case OP_JUMP_FALSE:		case OP_LOOP:
			jumpOper(byte, byte == OP_LOOP ? -1 : 1);
			break;
		case OP_MAKE_ITER:	case OP_UPDATE_ITER:
			iterOper(byte);
			break;
		case OP_CALL_NAT:	case OP_CALL_DEF:
			callOper(opNames[byte], (byte == OP_CALL_NAT));
			break;
		case OP_RETURN:		case OP_INVALID:
			singleOper(opNames[byte]);
			break;
		case OP_LOAD_R:
			loadOper("OP_LOAD_R");
			break;
		default:
		{
			FORMAT_PRINT("{:0>4} UNKNOWN OPCODE {}\n",
				ip - start, byte);
			ip++;
			break;
		}
	}
}

void Disassembler::disassembleCode()
{
	inVM = false;
	auto end = code.block.end();
	if (topLevel)
	{
		if ((file != "") && (ip < end)) // ip < end -> We have some bytecode to print.
			FORMAT_PRINT("=== CODE [{}] ===\n", file);
		FORMAT_PRINT("Bytes: {}\n", code.block.size());
	}
	int opers = 0;
	while (ip < end)
	{
		disassembleOp(*ip);
		opers++;
	}
	if (topLevel)
		FORMAT_PRINT("Instructions: {}\n", opers);
}