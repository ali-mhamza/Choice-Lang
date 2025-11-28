#include "../include/altvm.h"
#include "../include/opcodes.h"
#include <cstring>

AltVM::AltVM() :
    regSlot(0) {}

bool AltVM::checkNumOper(uint8_t slot)
{
    if (registers[slot].index() == 0) // BaseUP.
    {
        ObjType type = std::get<BaseUP>(registers[slot])->type;
        if (IS_NUM(type))
            return true;
    }
    else if (registers[slot].index() == 1) // Integer.
        return true;

    return false;
}

bool AltVM::checkNumOpers(uint8_t slot1, uint8_t slot2)
{
    return (checkNumOper(slot1) && checkNumOper(slot2));
}

void AltVM::arithOper(Opcode oper)
{
    uint8_t dest = *(++ip);
    uint8_t oper1 = *(++ip);
    uint8_t oper2 = *(++ip);

    if (checkNumOpers(oper1, oper2))
    {

    }
}

void AltVM::executeOp(uint8_t op)
{
    switch (op)
    {
        // Basic values.

        case OP_ZERO:       registers[regSlot++] = 0;       ip++;   break;
        case OP_ONE:        registers[regSlot++] = 1;       ip++;   break;
        case OP_TWO:        registers[regSlot++] = 2;       ip++;   break;
        case OP_NEG_ONE:    registers[regSlot++] = -1;      ip++;   break;
        case OP_NEG_TWO:    registers[regSlot++] = -2;      ip++;   break;
        case OP_TRUE:       registers[regSlot++] = true;    ip++;   break;
        case OP_FALSE:      registers[regSlot++] = false;   ip++;   break;
        case OP_NULL:       registers[regSlot++] = Null();  ip++;   break;
        case OP_CONST:
        {

        }
        
        // Arithmetic operations.

        case OP_ADD:    arithOper(OP_ADD);  break;
        case OP_SUB:    arithOper(OP_SUB);  break;
        case OP_MULT:   arithOper(OP_MULT); break;
        case OP_DIV:    arithOper(OP_DIV);  break;
        case OP_NEGATE: ip++;   break;
        case OP_POWER:  ip++;   break;
        case OP_MOD:    arithOper(OP_MOD);  break;
    }
}

void AltVM::executeCode(const ByteCode& code)
{
    ip = code.block.begin();
    auto end = code.block.end();
    
    while (ip < end)
    {
        uint8_t op = *ip;
        executeOp(op);
    }
}