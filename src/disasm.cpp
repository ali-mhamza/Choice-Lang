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
	if (func.lambda)
		FORMAT_PRINT("===== [start] <lambda> =====\n");
	else
		FORMAT_PRINT("===== [start] func {} =====\n", func.name);

	FORMAT_PRINT("-- locals: {}\n", func.argCount);
	Disassembler miniDis(func.code);
	miniDis.topLevel = false;
	miniDis.disassembleCode();

	if (func.lambda)
		FORMAT_PRINT("====== [end] <lambda> ======\n");
	else
		FORMAT_PRINT("====== [end] func {} ======\n", func.name);
}

void Disassembler::printOperValue(const Object& oper)
{
	FORMAT_PRINT("'{}' {}\n",
		oper.printVal(), oper.printType());
	if (IS_(FUNC, oper) && !inVM)
		disFunction(*(AS_(func, oper)));
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

void Disassembler::singleOper(ui8 byte)
{
	printOpcode(opNames[byte]);
	FORMAT_PRINT("R[{}]\n", *(ip + 1));
	
	ip += 2;
}

void Disassembler::doubleOper(ui8 byte)
{
	printOpcode(opNames[byte]);

	if (CAN_MODIFY(byte))
		FORMAT_PRINT("D[{}] R[{}]", *(ip + 1), *(ip + 2));
	else
		FORMAT_PRINT("R[{}] R[{}]", *(ip + 1), *(ip + 2));
	FORMAT_PRINT("\n");

	ip += 3;
}

void Disassembler::tripleOper(ui8 byte)
{
	printOpcode(opNames[byte]);

	FORMAT_PRINT("D[{}] ", *(ip + 1));
	ip++;

	for (int i = 0; i < 2; i++)
		FORMAT_PRINT("R[{}] ", *(ip + i + 1));
	FORMAT_PRINT("\n");
	
	ip += 3;
}

void Disassembler::loadOper()
{
	printOpcode("OP_LOAD_R");
	FORMAT_PRINT("D[{}] R[{}] ", *(ip + 1), *(ip + 2));

	ip += 3;
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

void Disassembler::callOper(ui8 byte)
{
	printOpcode(opNames[byte]);

	ui8 offset = 0;
	if (byte == OP_CALL_DEF)
	{
		offset = restoreByte();
		ip++;
	}
	ui8 callee = restoreByte();
	ip++;
	ui8 start = restoreByte();
	ip++;
	ui8 count = restoreByte();
	ip += 2;

	if (byte == OP_CALL_NAT)
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
		FORMAT_PRINT("D[{}] F[{}] ({}) R[{}]\n", offset, callee, count, start);
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

void Disassembler::listOper(ui8 byte)
{
	printOpcode(opNames[byte]);

	ui8 listReg = restoreByte();
	ip++;

	if (static_cast<Opcode>(byte) == OP_EXT_LIST)
	{
		ui8 startReg = restoreByte();
		ip++;

		ui8 count = restoreByte();
		ip += 2;

		FORMAT_PRINT("R[{}] R[{}] ({})\n", listReg, startReg, count);
	}
	else
	{
		FORMAT_PRINT("R[{}]\n", listReg);
		ip += 1;
	}
}

void Disassembler::disassembleOp(ui8 byte)
{
	switch (byte)
	{
		case OP_ADD:		case OP_SUB:		case OP_MULT:			case OP_DIV:
		case OP_MOD:		case OP_POWER:		case OP_AND:			case OP_OR:
		case OP_XOR:		case OP_SHIFT_R:	case OP_SHIFT_L:		case OP_GET_VAR:
		case OP_SET_VAR:
			tripleOper(byte);
			break;
		case OP_EQUAL:		case OP_GT:			case OP_LT:			case OP_IN:
		case OP_INCR:		case OP_DECR:		case OP_COMP:		case OP_MOVE_R:
			doubleOper(byte);
			break;
		case OP_JUMP:		case OP_JUMP_TRUE:	case OP_JUMP_FALSE:		case OP_LOOP:
			jumpOper(byte, byte == OP_LOOP ? -1 : 1);
			break;
		case OP_MAKE_ITER:	case OP_UPDATE_ITER:
			iterOper(byte);
			break;
		case OP_CALL_NAT:	case OP_CALL_DEF:
			callOper(byte);
			break;
		case OP_NEGATE:			case OP_NOT:		case OP_RETURN:		case OP_VOID:
		case OP_PRINT_VALID:
			singleOper(byte);
			break;
		case OP_LOAD_R:
			loadOper();
			break;
		case OP_LIST:		case OP_EXT_LIST:
			listOper(byte);
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