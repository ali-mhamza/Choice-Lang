#include "../include/vm.h"
#include "../include/disasm.h"
#include "../include/opcodes.h"
#include <cmath>
using namespace AST::Statement;
using namespace AST::Expression;

VM::VM() :
    registers(std::make_unique<Object[]>(256)), regSlot(0) {}

inline ui8 VM::readByte()
{
    ip++;
    return *(ip - 1);
}

inline ui16 VM::readShort()
{
    ip += 2;
    return static_cast<ui16>((*(ip - 2) << 8) | *(ip - 1));
}

inline ui32 VM::readLong()
{
    ip += 4;
    return static_cast<ui32>((*(ip - 4) << 24)
        | (*(ip - 3) << 16)
        | (*(ip - 2) << 8)
        | *(ip - 1));
}

bool VM::isTruthy(const Object& obj)
{
    switch (obj.type)
    {
        case OBJ_INT:   return (AS_INT(obj) != 0);
        case OBJ_DEC:   return (AS_DEC(obj) != 0);
        case OBJ_BOOL:  return AS_BOOL(obj);
        case OBJ_NULL:  return false;
        case OBJ_HEAP:
        {
            HeapObj* temp = AS_HEAP_PTR(obj);
            switch (temp->type)
            {
                case HEAP_STRING:   return (AS_STRING(temp).str.size() != 0);
                default:            return true;
            }
        }
        default: return true;
    }
}

void VM::loadOper(const vObj& pool)
{
    ui8 dest = readByte();
    regSlot = dest;
    ui8 oper = readByte();

    switch (oper)
    {
        case OP_NEG_TWO:    case OP_NEG_ONE:    case OP_ZERO:
        case OP_ONE:        case OP_TWO:
            registers[dest] = i64(oper - 2);
            break;
        case OP_TRUE:       registers[dest] = true;    break;
        case OP_FALSE:      registers[dest] = false;   break;
        case OP_NULL:       registers[dest] = nullptr; break;
        
        case OP_BYTE_OPER:
        {
            registers[dest] = pool[readByte()]; // Temporarily as integer.
            break;
        }
        case OP_SHORT_OPER:
        {
            registers[dest] = pool[readShort()];
            break;
        }
        case OP_LONG_OPER:
        {
            registers[dest] = pool[readLong()];
            break;
        }
    }
}

Object VM::arithOper(Opcode oper)
{
    ui8 op1 = readByte();
    ui8 op2 = readByte();
    Object obj;

    Object a = registers[op1];
    Object b = registers[op2];

    if (IS_NUM(a) && IS_NUM(b))
    {
        if (IS_INT(a) && IS_INT(b))
        {
            i64 aVal = a.as.intVal;
            i64 bVal = b.as.intVal;
            switch (oper)
            {
                case OP_ADD:    obj = aVal + bVal;          break;
                case OP_SUB:    obj = aVal - bVal;          break;
                case OP_MULT:   obj = aVal * bVal;          break;
                case OP_DIV:    obj = (double) aVal / bVal; break;
                case OP_MOD:    obj = aVal % bVal;          break;
                case OP_POWER:  obj = i64(pow(aVal, bVal)); break;
                default: break;
            }
        }
        else
        {
            double aVal = (double) AS_NUM(a);
            double bVal = (double) AS_NUM(b);
            switch (oper)
            {
                case OP_ADD:    obj = aVal + bVal;      break;
                case OP_SUB:    obj = aVal - bVal;      break;
                case OP_MULT:   obj = aVal * bVal;      break;
                case OP_DIV:    obj = aVal / bVal;      break;
                case OP_POWER:  obj = pow(aVal, bVal);  break;
                // Cannot do modulus for non-integers.
                // Maybe raise an error?
                default: break;
            }
        }
    }
    else
        throw TypeMismatch(
            "Cannot apply arithmetic operator to non-numeric values.",
            OBJ_NUM,
            (IS_NUM(a) ? b.type : a.type)
        );

    return obj;
}

inline Object VM::compareOper(Opcode op)
{
    const Object a = registers[readByte()];
    const Object b = registers[readByte()];
    
    switch (op)
    {
        case OP_EQUAL:  return (a == b);
        case OP_GT:     return (a > b);
        case OP_LT:     return (a < b);
        default: UNREACHABLE();
    }
}

inline Object VM::bitOper(Opcode op)
{
    Object a = registers[readByte()];
    Object b = registers[readByte()];
    
    if (!IS_INT(a) || !IS_INT(b))
        throw TypeMismatch(
            "Cannot apply bitwise operator to non-integer values.",
            OBJ_INT,
            (IS_INT(a) ? b.type : a.type)
        );

    switch (op)
    {
        case OP_BIT_AND:        return AS_INT(a) & AS_INT(b);
        case OP_BIT_OR:         return AS_INT(a) | AS_INT(b);
        case OP_BIT_XOR:        return AS_INT(a) ^ AS_INT(b);
        case OP_BIT_SHIFT_L:    return AS_INT(a) << AS_INT(b);
        case OP_BIT_SHIFT_R:    return AS_INT(a) >> AS_INT(b);
        default: UNREACHABLE();
    }
}

inline Object VM::unaryOper(Opcode op)
{
    Object obj = registers[readByte()];
    
    switch (op)
    {
        case OP_NEGATE:
        {
            if (!IS_NUM(obj))
                throw TypeMismatch(
                    "Cannot negate a non-numeric value.",
                    OBJ_NUM,
                    obj.type
                );
            return i64(AS_NUM(obj) * -1);
        }
        case OP_NOT: return !isTruthy(obj);
        case OP_BIT_COMP:
        {
            if (!IS_INT(obj))
                throw TypeMismatch(
                    "Cannot apply bitwise operator to non-integer values.",
                    OBJ_INT,
                    obj.type
                );
            return i64(~AS_INT(obj));
        }
        default: UNREACHABLE();
    }
}

#ifdef WATCH_REG
#include <iostream>

void VM::printRegister()
{
    for (int i = 0; i <= regSlot; i++)
    {
        if (!IS_VALID(registers[i]))
            break;
        std::cout << "[ " << registers[i].printVal()
            << " ]";
    }
    std::cout << '\n';
}

#endif

void VM::executeOp(Opcode op, const vObj& pool)
{    
    switch (op)
    {
        case OP_LOAD_R:
            loadOper(pool); // Could re-write this to be like the operators below.
            break;
        case OP_GET_VAR:
        case OP_SET_VAR:
        {
            ui8 dest = readByte();
            regSlot = (op == OP_GET_VAR ? dest : regSlot);
            ui8 src = readByte();
            registers[dest] = registers[src];
            break;
        }
        
        // Arithmetic operators.

        case OP_ADD:    case OP_SUB:    case OP_MULT:
        case OP_DIV:    case OP_MOD:    case OP_POWER:
        {
            ui8 dest = readByte();
            registers[dest] = arithOper(op);
            regSlot--;
            break;
        }

        // Comparison operators.
        case OP_GT:     case OP_LT:     case OP_EQUAL:
        {
            ui8 dest = readByte();
            registers[dest] = compareOper(op);
            regSlot--;
            break;
        }

        // Bit-wise operators.
        case OP_BIT_AND:        case OP_BIT_OR:         case OP_BIT_XOR:
        case OP_BIT_SHIFT_L:    case OP_BIT_SHIFT_R:
        {
            ui8 dest = readByte();
            registers[dest] = bitOper(op);
            regSlot--;
            break;
        }

        // Unary operators.
        case OP_NEGATE:     case OP_NOT:    case OP_BIT_COMP:
        {
            ui8 dest = readByte();
            registers[dest] = unaryOper(op);
            break;
        }

        default: break;
    }
}

void VM::executeCode(const ByteCode& code)
{
    ip = code.block.begin();
    auto end = code.block.end();
    const vObj& pool = code.pool;

    #ifdef WATCH_EXEC
        Disassembler dis(code);
    #endif
    
    while (ip < end)
    {
        try
        {
            #ifdef WATCH_EXEC
                dis.disassembleOp(*ip);
            #endif

            executeOp(static_cast<Opcode>(readByte()), pool);

            #ifdef WATCH_REG
                printRegister();
            #endif
        }
        catch (TypeMismatch& error)
        {
            error.report();
        }
    }
}