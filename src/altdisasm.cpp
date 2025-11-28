#include "../include/altdisasm.h"
#include "../include/opcodes.h"
#include <iostream>
#include <iomanip>

AltDisassembler::AltDisassembler(const ByteCode& code) :
	code(code), ip(code.block.begin()),
	start(code.block.begin()) {}

void AltDisassembler::printOperValue(const BaseUP& oper)
{
	std::cout << "'" << oper->print() << "' ";
	std::cout << oper->printType() << '\n';
}

uint8_t AltDisassembler::restoreByte()
{
	return *(ip + 1);
}

uint16_t AltDisassembler::restoreShort()
{
	uint16_t value = static_cast<uint16_t>(
		(*(ip + 1) << 8) | *(ip + 2)
	);

	return value;
}

uint32_t AltDisassembler::restoreLong()
{
	uint32_t value = static_cast<uint32_t>(
		(*(ip + 1) << 24) | (*(ip + 2) << 16)
		| (*(ip + 3) << 8) | *(ip + 4)
	);

	return value;
}

void AltDisassembler::singleOper(std::string_view opName)
{
	std::cout << std::right << std::setfill('0') 
		<< std::setw(4) << static_cast<int>(ip - start) << ' ';
	std::cout << std::left << std::setfill(' ') <<
		std::setw(15) << opName << ' ';
	std::cout << "R[" << static_cast<int>(*(ip + 1)) << "]\n";
	
	ip += 2;
}

void AltDisassembler::doubleOper(std::string_view opName)
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

void AltDisassembler::tripleOper(std::string_view opName)
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

void AltDisassembler::loadOper(std::string_view opName)
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
			uint8_t operand = restoreByte();
			std::cout << "C[" << static_cast<int>(operand) << "] ";
			printOperValue(code.pool[operand]);
			ip += 2;
			break;
		}
		case OP_SHORT_OPER:
		{	
			uint16_t operand = restoreShort();
			std::cout << "C[" << operand << "] ";
			printOperValue(code.pool[operand]);
			ip += 3;
			break;
		}
		case OP_LONG_OPER:
		{
			uint32_t operand = restoreLong();
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

void AltDisassembler::disassembleOp(uint8_t byte)
{
	switch (byte)
	{
		case OP_ADD:    tripleOper("OP_ADD");   	break;
		case OP_SUB:	tripleOper("OP_SUB");		break;
		case OP_MULT:	tripleOper("OP_MULT");		break;
		case OP_DIV:	tripleOper("OP_DIV");		break;
		case OP_MOD:	tripleOper("OP_MOD");		break;
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

void AltDisassembler::disassembleCode()
{
	auto end = code.block.end();
	if ((file != "") && (ip < end)) // ip < end -> We have some bytecode to print.
		// std::cout << "=== CODE ===\n";
		std::cout << "=== CODE [" << file << "] ===\n";
	std::cout << "Bytes: " << code.block.size() << '\n';
	while (ip < end)
		disassembleOp(*ip);
}