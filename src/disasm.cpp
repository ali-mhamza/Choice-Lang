#include "../include/disasm.h"
#include "../include/bytecode.h"
#include "../include/common.h"
#include "../include/object.h"
#include "../include/opcodes.h"
#include <iostream>
#include <iomanip>
using namespace Object;

Disassembler::Disassembler(const ByteCode& code) :
    code(code), ip(code.block.begin()),
    start(code.block.begin()) {}

void Disassembler::printOperValue(const BaseUP& oper)
{
    std::cout << "'" << oper->print() << "' ";
    std::cout << oper->printType() << '\n';
}

void Disassembler::noOper(std::string_view opName)
{
    std::cout << std::setw(4) << std::setfill('0') 
        << static_cast<int>(ip - start) << ' ';
    std::cout << opName << '\n';
    ip++;
}

void Disassembler::byteOper(std::string_view opName)
{
    uint8_t operand = *(ip + 1);
    std::cout << std::setw(4) << std::setfill('0') << 
        static_cast<int>(ip - 1 - start) << ' '; // ip - 1 to skip back across the length opcode.
    std::cout << opName << ' ';
    std::cout << static_cast<int>(operand) << ' ';

    printOperValue(code.pool[operand]);
    ip += 2;
}

void Disassembler::shortOper(std::string_view opName)
{
    uint16_t operand = static_cast<uint16_t>(
        (*(ip + 1) << 8) | *(ip + 2)
    );
    std::cout << std::setw(4) << std::setfill('0') << 
        static_cast<int>(ip - 1 - start) << ' ';
    std::cout << opName << ' ' << operand << ' ';

    printOperValue(code.pool[operand]);
    ip += 3;
}

void Disassembler::longOper(std::string_view opName)
{
    uint32_t operand = static_cast<uint32_t>(
        (*(ip + 1) << 24) | (*(ip + 2) << 16)
        | (*(ip + 3) << 8) | *(ip + 4)
    );
    std::cout << std::setw(4) << std::setfill('0') << 
        static_cast<int>(ip - 1 - start) << ' ';
    std::cout << opName << ' ' << operand << ' ';

    printOperValue(code.pool[operand]);
    ip += 5;
}

void Disassembler::constOper(std::string_view opName)
{
    switch (*(++ip)) // ++ to skip the length opcode.
    {
        case OP_BYTE_OPER:  byteOper(opName);   break;
        case OP_SHORT_OPER: shortOper(opName);  break;
        case OP_LONG_OPER:  longOper(opName);   break;
        default: break;
    }
}

void Disassembler::disassembleOp(uint8_t byte)
{
    switch (byte)
    {
        case OP_ADD:        noOper("OP_ADD");           break;
        case OP_SUB:        noOper("OP_SUB");           break;
        case OP_MULT:       noOper("OP_MULT");          break;
        case OP_DIV:        noOper("OP_DIV");           break;
        case OP_MOD:        noOper("OP_MOD");           break;
        case OP_ZERO:       noOper("OP_ZERO");          break;
        case OP_ONE:        noOper("OP_ONE");           break;
        case OP_TWO:        noOper("OP_TWO");           break;
        case OP_NEG_ONE:    noOper("OP_NEG_ONE");       break;
        case OP_NEG_TWO:    noOper("OP_NEG_TWO");       break;
        case OP_TRUE:       noOper("OP_TRUE");          break;
        case OP_FALSE:      noOper("OP_FALSE");         break;
        case OP_NULL:       noOper("OP_NULL");          break;
        case OP_CONST:      constOper("OP_CONST");      break;
        case OP_RETURN:     noOper("OP_RETURN");        break;
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
    while (ip < end)
        disassembleOp(*ip);
}