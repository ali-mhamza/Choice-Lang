#include "../include/disasm.h"
#include "../include/bytecode.h"
#include "../include/common.h"	// For CH_PRINT macro and 'file' global variable.
#include "../include/config.h"	// For DIS_FUNCTION_OBJS constant.
#include "../include/natives.h"	// For funcNames in callOp() method.
#include "../include/object.h"
#include "../include/opcodes.h"
#include <string_view>

#define PRINT_FULL_OFFSET 1

Disassembler::Disassembler(const ByteCode& code) :
	code(code), ip(code.block.begin()),
	start(code.block.begin()), topLevel(true),
	inVM(true) {}

void Disassembler::printOpcode(std::string_view opName)
{
	#if PRINT_FULL_OFFSET
		CH_PRINT("{:0>4} {:<15} ", ip - start, opName);
	#else
		// Prints leading spaces, not zeros.
		CH_PRINT("{:>4} {:<15} ", ip - start, opName);
	#endif
}

void Disassembler::disFunction(const Function& func)
{
	if (func.lambda)
		CH_PRINT("\n===== [start] <lambda> =====\n\n");
	else
		CH_PRINT("\n===== [start] func {} =====\n\n", func.name);

	CH_PRINT("(args: {}, ", func.argCount);
	CH_PRINT("constants: {})", func.code.pool.size());
	CH_PRINT("\n\n");
	Disassembler miniDis(func.code);
	miniDis.topLevel = false;
	miniDis.disassembleCode();

	if (func.lambda)
		CH_PRINT("\n====== [end] <lambda> ======\n\n");
	else
		CH_PRINT("\n====== [end] func {} ======\n\n", func.name);
}

void Disassembler::printOperValue(const Object& oper)
{
	CH_PRINT("'{}' {}\n",
		oper.printVal(), oper.printType());
	// We only disassemble functions when requested, and not
	// with concurrent disassembler output during VM execution.
	if (DIS_FUNCTION_OBJS && IS_FUNCOBJ(oper) && !inVM)
		disFunction(*(AS_FUNC(oper)));
}

ui8 Disassembler::restoreByte()
{
	return ip[1];
}

ui16 Disassembler::restoreShort()
{
	ui16 value = static_cast<ui16>(
		(ip[1] << 8) | (ip[2])
	);

	return value;
}

ui32 Disassembler::restoreLong()
{
	ui32 value = static_cast<ui32>(
		(ip[1] << 24)
		| (ip[2] << 16)
		| (ip[3] << 8)
		| ip[4]
	);

	return value;
}

void Disassembler::singleOper(ui8 byte)
{
	printOpcode(opNames[byte]);
	CH_PRINT("R[{}]\n", ip[1]);
	
	ip += 2;
}

void Disassembler::doubleOper(ui8 byte)
{
	printOpcode(opNames[byte]);

	Opcode op = static_cast<Opcode>(byte);
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
			ui8 operand = restoreByte();
			CH_PRINT("C[{}] ", operand);
			printOperValue(code.pool[operand]);
			ip += 2;
			break;
		}
		case OP_SHORT_OPER:
		{
			ui16 operand = restoreShort();
			CH_PRINT("C[{}] ", operand);
			printOperValue(code.pool[operand]);
			ip += 3;
			break;
		}
		case OP_LONG_OPER:
		{
			ui32 operand = restoreLong();
			CH_PRINT("C[{}] ", operand);
			printOperValue(code.pool[operand]);
			ip += 5;
			break;
		}
		default: // Direct constant loading instruction.
			CH_PRINT("{}\n", opNames[*ip]);
			ip++;
	}
}

void Disassembler::jumpOp(ui8 byte, int sign)
{
	printOpcode(opNames[byte]);
	if ((byte == OP_JUMP_TRUE) || (byte == OP_JUMP_FALSE))
	{
		ui8 reg = restoreByte();
		ip++;
		CH_PRINT("R[{}] ", reg);
	}
	ui16 jump = restoreShort();
	ip += 3;
	CH_PRINT("-> {}\n", ip - start + (sign * jump));
}

void Disassembler::callOp(ui8 byte)
{
	printOpcode(opNames[byte]);
	ui8 callee = restoreByte();
	ip++;
	ui8 start = restoreByte();
	ip++;
	ui8 count = restoreByte();
	ip += 2;

	if (byte == OP_CALL_NAT)
	{
		std::string_view func = Natives::funcNames[callee];
		CH_PRINT("'{}' ({}) R[{}]\n", func, count, start);
	}
	else
	{
		// We only save the register that the function object
		// will be in by the time it is called.
		// Since registers and their contents are only available
		// at runtime, we cannot display any information about the
		// function besides its expected location when only
		// disassembling bytecode.
		CH_PRINT("F[{}] ({}) R[{}]\n", callee, count, start);
	}
}

void Disassembler::iterOp(ui8 byte)
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
		ui16 jump = restoreShort();
		ip += 3;
		CH_PRINT("R[{}] R[{}] -> {}\n", ip[-4], ip[-3],
			ip - start - jump);
	}
}

void Disassembler::collectionOp(ui8 byte)
{
	printOpcode(opNames[byte]);

	ui8 reg = restoreByte();
	ip++;

	if ((static_cast<Opcode>(byte) == OP_EXT_LIST)
		|| (static_cast<Opcode>(byte) == OP_EXT_TUPLE))
	{
		ui8 startReg = restoreByte();
		ip++;

		ui8 count = restoreByte();
		ip += 2;

		CH_PRINT("R[{}] R[{}] ({})\n", reg, startReg, count);
	}
	else
	{
		CH_PRINT("R[{}]\n", reg);
		ip += 1;
	}
}

void Disassembler::captureOp(ui8 byte)
{
	printOpcode(opNames[byte]);
	ui8 funcReg = restoreByte();
	ip++;

	if (static_cast<Opcode>(byte) == OP_CAPTURE_VAL)
		CH_PRINT("F[{}] R[{}]\n", funcReg, ip[1]);
	else
		CH_PRINT("F[{}] C[{}]\n", funcReg, ip[1]);
	ip += 2;
}

void Disassembler::disassembleOp(ui8 byte)
{
	switch (byte)
	{
		case OP_ADD:		case OP_SUB:		case OP_MULT:		case OP_DIV:
		case OP_MOD:		case OP_POWER:		case OP_AND:		case OP_OR:
		case OP_XOR:		case OP_SHIFT_R:	case OP_SHIFT_L:	case OP_GET_GLOBAL:
		case OP_SET_GLOBAL:	case OP_GET_CELL:	case OP_SET_CELL:	case OP_GET_LOCAL:
		case OP_SET_LOCAL:	case OP_EQUAL:		case OP_GT:			case OP_LT:
		case OP_IN:			case OP_MOVE_R:
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
		case OP_CLOSURE:	case OP_NEG:		case OP_NOT:		case OP_INCR:
		case OP_DECR:		case OP_COMP:		case OP_RETURN:		case OP_VOID:
		case OP_PRINT_VALID:	case OP_ENTER_SCOPE:
			singleOper(byte);
			break;
		case OP_LOAD_R:
			loadOp();
			break;
		case OP_LIST:		case OP_EXT_LIST:		case OP_TUPLE:		case OP_EXT_TUPLE:
			collectionOp(byte);
			break;
		case OP_CAPTURE_VAL:	case OP_CAPTURE_CELL:
			captureOp(byte);
			break;
		case OP_EXIT_SCOPE:
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
	inVM = false;
	auto end = code.block.end();
	if (topLevel)
	{
		if ((file != "") && (ip < end)) // ip < end -> We have some bytecode to print.
			CH_PRINT("=== CODE [{}] ===\n", file);
		CH_PRINT("Bytes: {}\n", code.block.size());
	}
	int opers = 0;
	while (ip < end)
	{
		disassembleOp(*ip);
		opers++;
	}
	if (topLevel)
		CH_PRINT("Instructions: {}\n", opers);
}