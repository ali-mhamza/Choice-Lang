#include "../include/vm.h"
#include "../include/disasm.h"
#include "../include/error.h"
#include "../include/natives.h"
#include "../include/opcodes.h"
#include <cmath>
#include <cstring>

VM::VM() :
    ip(nullptr), end(nullptr),
    registers(std::make_unique<Object[]>(regSize))
{
    #ifdef WATCH_REG
    regSlot = 0;
    #endif

    #ifdef WATCH_EXEC
    dis = nullptr;
    #endif
}

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

inline bool VM::isTruthy(const Object& obj)
{
    switch (obj.type)
    {
        case OBJ_INT:   return (AS_INT(obj) != 0);
        case OBJ_DEC:   return (AS_DEC(obj) != 0.0);
        case OBJ_BOOL:  return AS_BOOL(obj);
        case OBJ_NULL:  return false;
        default:
        {
            if (IS_HEAP_OBJ(obj))
            {
                HeapObj* temp = AS_HEAP_PTR(obj);
                switch (temp->type)
                {
                    case OBJ_STRING:   return (AS_STRING(temp).str.size() != 0);
                    default:           return true;
                }
            }
            return true;
        }
    }
}

inline Object VM::loadOper(const vObj& pool)
{
    switch (ui8 oper = readByte())
    {
        case OP_NEG_TWO:    case OP_NEG_ONE:    case OP_ZERO:
        case OP_ONE:        case OP_TWO:
            return i64(oper) - 2;
        case OP_TRUE:       return true;
        case OP_FALSE:      return false;
        case OP_NULL:       return nullptr;
        case OP_BYTE_OPER:  return pool[readByte()];
        case OP_SHORT_OPER: return pool[readShort()];
        case OP_LONG_OPER:  return pool[readLong()];
        default: UNREACHABLE();
    }
}

inline Object VM::concatStrings(const Object& str1, const Object& str2)
{
    HeapObj* temp1 = AS_HEAP_PTR(str1);
    HeapObj* temp2 = AS_HEAP_PTR(str2);

    std::string concat = AS_STRING(temp1).str + AS_STRING(temp2).str;
    HeapObj* newStr = new String(concat);
    return newStr;
}

Object VM::arithOper(Opcode oper)
{
    const Object& a = registers[readByte()];
    const Object& b = registers[readByte()];

    if (IS_INT(a) && IS_INT(b))
    {
        i64 aVal = a.as.intVal;
        i64 bVal = b.as.intVal;
        switch (oper)
        {
            case OP_ADD:    return aVal + bVal;
            case OP_SUB:    return aVal - bVal;
            case OP_MULT:   return aVal * bVal;
            case OP_DIV:    return (double) aVal / bVal;
            case OP_MOD:    return aVal % bVal;
            case OP_POWER:  return i64(pow(aVal, bVal));
            default: UNREACHABLE();
        }
    }
    else if (IS_NUM(a) && IS_NUM(b))
    {
        double aVal = (double) AS_NUM(a);
        double bVal = (double) AS_NUM(b);
        switch (oper)
        {
            case OP_ADD:    return aVal + bVal;
            case OP_SUB:    return aVal - bVal;
            case OP_MULT:   return aVal * bVal;
            case OP_DIV:    return aVal / bVal;
            case OP_POWER:  return pow(aVal, bVal);
            // Cannot do modulus for non-integers.
            // Maybe raise an error?
            default: UNREACHABLE();
        }
    }
    else if (IS_STRING(&a) && IS_STRING(&b) && (oper == OP_ADD))
        return concatStrings(a, b);
    else
        throw TypeMismatch(
            "Cannot apply arithmetic operator to non-numeric values.",
            OBJ_NUM,
            (IS_NUM(a) ? b.type : a.type)
        );
}

inline Object VM::compareOper(Opcode op)
{
    const Object& a = registers[readByte()];
    const Object& b = registers[readByte()];
    
    switch (op)
    {
        case OP_EQUAL:  return (a == b);
        case OP_GT:     return (a > b);
        case OP_LT:     return (a < b);
        case OP_IN:
        {
            if (IS_STRING(&a) && IS_STRING(&b)) // Temporarily.
            {
                const String& s1 = AS_CONST_STRING(AS_HEAP_PTR(a));
                const String& s2 = AS_CONST_STRING(AS_HEAP_PTR(b));
                return (strstr(s2.str.c_str(), s1.str.c_str()) != nullptr);
            }
            else
                throw TypeMismatch(
                    "Left operand not matching member type of iterable object.",
                    OBJ_STRING,
                    (IS_STRING(&a) ? b.type : a.type)
                );
        }
        default: UNREACHABLE();
    }
}

static inline i64 fromUnsigned(ui64 num)
{
    i64 i;
    std::memcpy(&i, &num, sizeof(ui64));
    return i;
}

Object VM::bitOper(Opcode op)
{
    const Object& a = registers[readByte()];
    const Object& b = registers[readByte()];
    
    if (!IS_INT(a) || !IS_INT(b))
        throw TypeMismatch(
            "Cannot apply bitwise operator to non-integer values.",
            OBJ_INT,
            (IS_INT(a) ? b.type : a.type)
        );
    
    ui64 aVal = AS_UINT(a);
    ui64 bVal = AS_UINT(b);

    switch (op)
    {
        case OP_BIT_AND:        return fromUnsigned(aVal & bVal);
        case OP_BIT_OR:         return fromUnsigned(aVal | bVal);
        case OP_BIT_XOR:        return fromUnsigned(aVal ^ bVal);
        case OP_BIT_SHIFT_L:    return fromUnsigned(aVal << bVal);
        case OP_BIT_SHIFT_R:    return fromUnsigned(aVal >> bVal);
        default: UNREACHABLE();
    }
}

Object VM::unaryOper(Opcode op)
{
    const Object& obj = registers[readByte()];
    
    switch (op)
    {
        case OP_INCREMENT:
        case OP_DECREMENT:
        {
            if (!IS_NUM(obj))
                throw TypeMismatch(
                    "Cannot increment or decrement a non-numeric value.",
                    OBJ_NUM,
                    obj.type
                );
            if (IS_INT(obj))
                return AS_INT(obj) + i64(op == OP_INCREMENT ? 1 : -1);
            else
                return AS_DEC(obj) + double(op == OP_INCREMENT ? 1 : -1);
        }
        case OP_NEGATE:
        {
            if (!IS_NUM(obj))
                throw TypeMismatch(
                    "Cannot negate a non-numeric value.",
                    OBJ_NUM,
                    obj.type
                );
            if (IS_INT(obj))
                return i64(AS_NUM(obj) * -1);
            else
                return (AS_NUM(obj) * -1);
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
            return i64(~AS_UINT(obj));
        }
        default: UNREACHABLE();
    }
}

// Handle regSlot.
void VM::handleIter(Opcode oper)
{
    if (oper == OP_MAKE_ITER)
    {
        Object& var = registers[readByte()];
        Object& iterable = registers[readByte()];

        ObjIter* iter;
        if ((iter = iterable.makeIter()) == nullptr)
            throw TypeMismatch(
                "Given object is not iterable.",
                OBJ_ITER,
                iterable.type
            );

        if (iter->start(var))
        {
            iterable = Object(iter);
            ip += 3; // Skip our fail-case jump.
            #ifdef WATCH_EXEC
                this->dis->ip += 3;
            #endif
        }
    }
    else if (oper == OP_UPDATE_ITER)
    {
        Object& var = registers[readByte()];
        Object& iter = registers[readByte()];
        ui16 jump = readShort();

        if (AS_ITER(iter)->next(var))
        {
            ip -= jump;
            #ifdef WATCH_EXEC
                this->dis->ip -= jump;
            #endif
        }
    }
}

#ifdef WATCH_REG
#include "../include/common.h"

void VM::printRegister()
{
    for (int i = 0; i <= regSlot; i++)
    {
        if (!IS_VALID(registers[i]))
            break;
        FORMAT_PRINT("[{}]", registers[i].printVal());
    }
    FORMAT_PRINT("\n");
}

#endif

void VM::executeOp(Opcode op, const vObj& pool)
{
    #if COMPUTED_GOTO
        static void* dispatchTable[] = {
            #define LABEL_ENABLE(op)    &&CASE_##op
            #define LABEL_DISABLE(op)   &&CASE_NO_REACH

            #define LABEL(op, state) LABEL_##state(op),
            #include "../include/opcode_list.h"
            &&CASE_NO_REACH
            #undef LABEL
        };

        #ifdef WATCH_EXEC
            #define DEBUG_OP(op)    dis->disassembleOp(op)
        #else
            #define DEBUG_OP(op)
        #endif

        #ifdef WATCH_REG
            #define PRINT_REGS()    printRegister()
        #else
            #define PRINT_REGS()
        #endif

        #define DISPATCH_OP(op)  goto *dispatchTable[op]
        #define DISPATCH()                                                          \
            do {                                                                    \
                if (ip >= end)                                                      \
                    return;                                                         \
                op = static_cast<Opcode>(readByte());                               \
                ASSERT(IS_VALID_OP(op),                                             \
                    FORMAT_STR("Invalid opcode {}", static_cast<ui8>(op)));         \
                DEBUG_OP(op);                                                       \
                DISPATCH_OP(op);                                                    \
                PRINT_REGS();                                                       \
            } while (false)
        #define SWITCH(op)  DISPATCH();
        #define CASE(op)    CASE_##op
        #define DEFAULT     CASE_NO_REACH
    #else
        #define SWITCH(op)  switch (op)
        #define CASE(op)    case op
        #define DISPATCH()  break
        #define DEFAULT     default
    #endif
    
    SWITCH(op)
    {
        CASE(OP_LOAD_R):
        {
            ui8 dest = readByte();
            registers[dest] = loadOper(pool);
            #ifdef WATCH_REG
            regSlot = dest;
            #endif
            DISPATCH();
        }
        CASE(OP_LOOP):
        {
            ui16 jump = readShort();
            ip -= jump;
            #ifdef WATCH_EXEC
                this->dis->ip -= jump;
            #endif
            DISPATCH();
        }
        CASE(OP_JUMP):
        {
            ui16 jump = readShort();
            ip += jump;
            #ifdef WATCH_EXEC
                this->dis->ip += jump;
            #endif
            DISPATCH();
        }
        CASE(OP_JUMP_TRUE):
        {
            ui8 check = readByte();
            ui16 jump = readShort();
            if (isTruthy(registers[check]))
            {
                ip += jump;
                #ifdef WATCH_EXEC
                    this->dis->ip += jump;
                #endif
            }
            DISPATCH();
        }
        CASE(OP_JUMP_FALSE):
        {
            ui8 check = readByte();
            ui16 jump = readShort();
            if (!isTruthy(registers[check]))
            {
                ip += jump;
                #ifdef WATCH_EXEC
                    this->dis->ip += jump;
                #endif
            }
            DISPATCH();
        }
        CASE(OP_GET_VAR):
        CASE(OP_SET_VAR):
        {
            ui8 dest = readByte();
            ui8 src = readByte();
            registers[dest] = registers[src];
            #ifdef WATCH_REG
            regSlot = (op == OP_GET_VAR ? dest : regSlot);
            #endif
            DISPATCH();
        }
        CASE(OP_MAKE_ITER):
        CASE(OP_UPDATE_ITER):
        {
            handleIter(op);
            DISPATCH();
        }
        
        // Arithmetic operators.

        CASE(OP_ADD):   CASE(OP_SUB):   CASE(OP_MULT):
        CASE(OP_DIV):   CASE(OP_MOD):   CASE(OP_POWER):
        {
            ui8 dest = readByte();
            registers[dest] = arithOper(op);
            #ifdef WATCH_REG
            regSlot--;
            #endif
            DISPATCH();
        }

        // Comparison operators.
        CASE(OP_GT):    CASE(OP_LT):    CASE(OP_EQUAL):     CASE(OP_IN):
        {
            ui8 dest = readByte();
            registers[dest] = compareOper(op);
            #ifdef WATCH_REG
            regSlot--;
            #endif
            DISPATCH();
        }

        // Bit-wise operators.
        CASE(OP_BIT_AND):       CASE(OP_BIT_OR):        CASE(OP_BIT_XOR):
        CASE(OP_BIT_SHIFT_L):   CASE(OP_BIT_SHIFT_R):
        {
            ui8 dest = readByte();
            registers[dest] = bitOper(op);
            #ifdef WATCH_REG
            regSlot--;
            #endif
            DISPATCH();
        }

        // Unary operators.
        CASE(OP_INCREMENT):     CASE(OP_DECREMENT):     CASE(OP_NEGATE):
        CASE(OP_NOT):           CASE(OP_BIT_COMP):
        {
            ui8 dest = readByte();
            registers[dest] = unaryOper(op);
            DISPATCH();
        }

        // Functions.
        CASE(OP_CALL_NAT):
        {
            ui8 callee = readByte();
            ui8 start = readByte();
            ui8 argCount = readByte();

            registers[start] = Natives::functions[callee](
                &registers[start], argCount, Token() // Temporarily.
            );
            DISPATCH();
        }

        DEFAULT: DISPATCH();
    }
}

void VM::executeCode(const ByteCode& code)
{
    ip = code.block.data();
    end = ip + code.block.size();
    const vObj& pool = code.pool;

    #ifdef WATCH_EXEC
        Disassembler dis(code);
        this->dis = &dis;
    #endif

    try
    {
        #if !COMPUTED_GOTO
            while (ip < end)
            {
                #ifdef WATCH_EXEC
                    dis.disassembleOp(*ip);
                #endif

                executeOp(static_cast<Opcode>(readByte()), pool);

                #ifdef WATCH_REG
                    printRegister();
                #endif
            }
        #else
            executeOp(static_cast<Opcode>(0), pool);
        #endif
    }
    catch (TypeMismatch& error)
    {
        error.report();
    }
    catch (RuntimeError& error)
    {
        error.report();
    }

    #ifdef WATCH_EXEC
    this->dis = nullptr;
    #endif
}