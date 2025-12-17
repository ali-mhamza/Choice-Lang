#include "../include/disasm.h"
#include "../include/opcodes.h"
#include <iostream>
#include <iomanip>

Disassembler::Disassembler(const ByteCode& code) :
	code(code), ip(code.block.begin()),
	start(code.block.begin()) {}

void Disassembler::printOperValue(const BaseUP& oper)
{
	std::cout << "'" << oper->print() << "' ";
	std::cout << oper->printType() << '\n';
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
	std::cout << std::right << std::setfill('0') 
		<< std::setw(4) << static_cast<int>(ip - start) << ' ';
	std::cout << std::left << std::setfill(' ') <<
		std::setw(15) << opName << ' ';
	std::cout << "R[" << static_cast<int>(*(ip + 1)) << "]\n";
	
	ip += 2;
}

void Disassembler::doubleOper(std::string_view opName)
{
	std::cout << std::right << std::setfill('0')
		<< std::setw(4) << static_cast<int>(ip - start) << ' ';
	std::cout << std::left << std::setfill(' ') <<
		std::setw(15) << opName << ' ';

	for (int i = 0; i < 2; i++)
		std::cout << "R[" << static_cast<int>(*(ip + i + 1)) << "] ";
	
	std::cout << '\n';

	ip += 3;
}

void Disassembler::tripleOper(std::string_view opName)
{
	std::cout << std::right << std::setfill('0')
		<< std::setw(4) << static_cast<int>(ip - start) << ' ';
	std::cout << std::left << std::setfill(' ') <<
		std::setw(15) << opName << ' ';

	for (int i = 0; i < 3; i++)
		std::cout << "R[" << static_cast<int>(*(ip + i + 1)) << "] ";

	std::cout << '\n';
	
	ip += 4;
}

void Disassembler::loadOper(std::string_view opName)
{
	std::cout << std::right << std::setfill('0')
		<< std::setw(4) << static_cast<int>(ip - start) << ' ';
	std::cout << std::left << std::setfill(' ') <<
		std::setw(15) << opName << ' ';
	std::cout << "R[" << static_cast<int>(*(ip + 1)) << "] ";

	ip += 2;
	switch (*ip)
	{
		case OP_BYTE_OPER:
		{
			ui8 operand = restoreByte();
			std::cout << "C[" << static_cast<int>(operand) << "] ";
			printOperValue(code.pool[operand]);
			ip += 2;
			break;
		}
		case OP_SHORT_OPER:
		{	
			ui16 operand = restoreShort();
			std::cout << "C[" << operand << "] ";
			printOperValue(code.pool[operand]);
			ip += 3;
			break;
		}
		case OP_LONG_OPER:
		{
			ui32 operand = restoreLong();
			std::cout << "C[" << operand << "] ";
			printOperValue(code.pool[operand]);
			ip += 5;
			break;
		}
		default: // Direct constant loading instruction.
			std::cout << opNames[*ip] << '\n';
			ip += 1;
	}
}

void Disassembler::disassembleOp(ui8 byte)
{
	switch (byte)
	{
		case OP_ADD:		case OP_SUB:		case OP_MULT:			case OP_DIV:
		case OP_MOD:		case OP_POWER:		case OP_AND:			case OP_OR:			
		case OP_EQUAL:		case OP_GT:			case OP_LT:				case OP_BIT_AND:
		case OP_BIT_OR:		case OP_BIT_XOR:	case OP_BIT_SHIFT_R:
		case OP_BIT_SHIFT_L:
			tripleOper(opNames[byte]);
			break;
		case OP_NEGATE:		case OP_NOT:		case OP_BIT_COMP:	case OP_MOVE_R:
		case OP_GET_VAR:	case OP_SET_VAR:
			doubleOper(opNames[byte]);
			break;
		case OP_LOAD_R: loadOper("OP_LOAD_R");  	break;
		case OP_RETURN:	singleOper("OP_RETURN");	break;
		// case OP_FREE_R:	singleOper("OP_FREE_R");	break;
		default:
		{
			std::cout << std::setw(4) << std::setfill('0')
				<< static_cast<int>(ip - start) << ' ';
			std::cout << "UNKNOWN OPCODE " << static_cast<int>(byte) << '\n';
			ip++;
			break;
		}
	}
}

void Disassembler::disassembleCode()
{
	auto end = code.block.end();
	if ((file != "") && (ip < end)) // ip < end -> We have some bytecode to print.
		// std::cout << "=== CODE ===\n";
		std::cout << "=== CODE [" << file << "] ===\n";
	std::cout << "Bytes: " << code.block.size() << '\n';
	int opers = 0;
	while (ip < end)
	{
		disassembleOp(*ip);
		opers++;
	}
	std::cout << "Instructions: " << opers << '\n';
}