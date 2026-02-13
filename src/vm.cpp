#include "../include/vm.h"
#include "../include/disasm.h"
#include "../include/error.h"
#include "../include/linear_alloc.h"
#include "../include/natives.h"
#include "../include/opcodes.h"
#include "../include/object.h"
#include <cmath>
#include <cstring>

VM::VM() :
    ip(nullptr), end(nullptr),
    registers(new Object[regSize])
{
    #if WATCH_REG
    regSlot = 0;
    #endif

    #if WATCH_EXEC
    dis = nullptr;
    #endif
}

VM::~VM()
{
    delete[] registers;
}

inline ui8 VM::readByte()
{
    ip++;
    return *(ip - 1);
}

inline ui16 VM::readShort()
{
    ui16 b1 = ip[0];
    ui16 b2 = ip[1];
    ip += 2;
    return static_cast<ui16>((b1 << 8) | b2);
}

inline ui32 VM::readLong()
{
    ui32 b1 = ip[0];
    ui32 b2 = ip[1];
    ui32 b3 = ip[2];
    ui32 b4 = ip[3];
    ip += 4;
    return static_cast<ui32>((b1 << 24) | (b2 << 16) | (b3 << 8) | b4);
}

inline bool VM::isTruthy(const Object& obj)
{
    switch (obj.type)
    {
        case OBJ_INT:       return (AS_INT(obj) != 0);
        case OBJ_DEC:       return (AS_DEC(obj) != 0.0);
        case OBJ_BOOL:      return AS_BOOL(obj);
        case OBJ_NULL:      return false;
        case OBJ_STRING:    return (AS_STRING(obj).str.size() != 0);
        // Functions and ranges are always truthy.
        default:            return true;
    }
}

inline Object VM::loadOper()
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
    std::string concat = AS_STRING(str1).str + AS_STRING(str2).str;
    return ALLOC(String, ObjDealloc<String>, concat);
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
    else if (IS_STRING(a) && IS_STRING(b) && (oper == OP_ADD))
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

    if (((op == OP_GT) || (op == OP_LT))
        && (!IS_NUM(a) || !IS_NUM(b)))
    {
        throw TypeMismatch(
            "Cannot compare non-numeric values.",
            OBJ_NUM,
            IS_NUM(a) ? b.type : a.type
        );
    }
    
    switch (op)
    {
        case OP_EQUAL:  return (a == b);
        case OP_GT:     return (a > b);
        case OP_LT:     return (a < b);
        case OP_IN:
        {
            if (IS_STRING(a) && IS_STRING(b)) // Temporarily.
            {
                const String& s1 = AS_STRING(a);
                const String& s2 = AS_STRING(b);
                return s2.contains(s1);
            }
            else if (IS_INT(a) && IS_RANGE(b))
            {
                const Range& range = AS_RANGE(b);
                return range.contains(a);
            }
            else if (!IS_STRING(b) && !IS_RANGE(b))
                throw TypeMismatch(
                    "Right operand must be an iterable object.",
                    OBJ_ITER,
                    b.type
                );
            else
                throw TypeMismatch(
                    "Left operand not matching member type of iterable object.",
                    (b.type == OBJ_STRING ? OBJ_STRING : OBJ_INT),
                    a.type
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

void VM::callFunc(ui8 callee, ui8 start, ui8 argCount)
{
    const Object& obj = registers[callee];
    if (!IS_FUNC(obj))
        throw TypeMismatch(
            "Object is not callable.",
            OBJ_FUNC,
            obj.type
        );

    const ByteCode& code = AS_FUNC(obj).code;
    registers += start;
    ip = code.block.data();
    end = ip + code.block.size();
    pool = code.pool.data();

    #if WATCH_EXEC
        this->dis = new Disassembler(code);
    #endif
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
            #if WATCH_EXEC
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
            #if WATCH_EXEC
                this->dis->ip -= jump;
            #endif
        }
    }
}

#if WATCH_REG
#include "../include/common.h"

void VM::printRegister()
{
    ui8 i;
    for (i = 0; i <= regSlot; i++)
    {
        if (!IS_VALID(registers[i]))
            break;
        FORMAT_PRINT("[{}]", registers[i].printVal());
    }
    if (i != 0) FORMAT_PRINT("\n");
}

#endif

void VM::executeOp(Opcode op)
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

        #if WATCH_EXEC
            #define DEBUG_OP(op)    dis->disassembleOp(op)
        #else
            #define DEBUG_OP(op)
        #endif

        #if WATCH_REG
            #define PRINT_REGS()    printRegister()
        #else
            #define PRINT_REGS()
        #endif

        #define DISPATCH_OP(op)  goto *dispatchTable[op]
        #define DISPATCH()                                                          \
            do {                                                                    \
                PRINT_REGS();                                                       \
                if (ip >= end)                                                      \
                    return;                                                         \
                op = static_cast<Opcode>(readByte());                               \
                ASSERT(IS_VALID_OP(op),                                             \
                    FORMAT_STR("Invalid opcode {}", static_cast<ui8>(op)));         \
                DEBUG_OP(op);                                                       \
                DISPATCH_OP(op);                                                    \
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
            registers[dest] = loadOper();
            #if WATCH_REG
            regSlot = dest;
            #endif
            DISPATCH();
        }
        CASE(OP_LOOP):
        {
            ui16 jump = readShort();
            ip -= jump;
            #if WATCH_EXEC
                this->dis->ip -= jump;
            #endif
            DISPATCH();
        }
        CASE(OP_JUMP):
        {
            ui16 jump = readShort();
            ip += jump;
            #if WATCH_EXEC
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
                #if WATCH_EXEC
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
                #if WATCH_EXEC
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

            if ((op == OP_SET_VAR) && !IS_VALID(registers[src]))
                throw RuntimeError(Token(),
                    "Type-less value cannot be assigned to a variable.");

            registers[dest] = registers[src];
            #if WATCH_REG
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
            #if WATCH_REG
            regSlot--;
            #endif
            DISPATCH();
        }

        // Comparison operators.
        CASE(OP_GT):    CASE(OP_LT):    CASE(OP_EQUAL):     CASE(OP_IN):
        {
            ui8 dest = readByte();
            registers[dest] = compareOper(op);
            #if WATCH_REG
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
            #if WATCH_REG
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

            auto func = Natives::functions[callee];
            Natives::functions[callee](&registers[start], argCount,
                Token()); // Temporarily.
            DISPATCH();
        }
        CASE(OP_CALL_DEF):
        {
            ui8 callee = readByte();
            ui8 start = readByte();
            ui8 argCount = readByte();

            contexts.emplace(FuncContext::Args{
                registers, ip, end, pool
                #if WATCH_EXEC
                , this->dis
                #endif
            });

            callFunc(callee, start, argCount);
            DISPATCH();
        }
        CASE(OP_RETURN):
        {   
            ui8 retSlot = readByte();
            registers[0] = registers[retSlot];

            // Correct regSlot after return.

            FuncContext& context = contexts.top();
            registers = context.regStart;
            ip = context.ip;
            end = context.end;
            pool = context.pool;
            #if WATCH_EXEC
                delete this->dis;
                this->dis = context.dis;
            #endif

            contexts.pop();
            DISPATCH();
        }
        CASE(OP_VOID):
        {
            ui8 dest = readByte();
            registers[dest] = Object();
            DISPATCH();
        }

        DEFAULT: UNREACHABLE();
    }
}

void VM::executeCode(const ByteCode& code)
{
    ip = code.block.data();
    end = ip + code.block.size();
    pool = code.pool.data();

    #if WATCH_EXEC
        Disassembler dis(code);
        this->dis = &dis;
    #endif

    try
    {
        #if !COMPUTED_GOTO
            while (ip < end)
            {
                #if WATCH_EXEC
                    dis.disassembleOp(*ip);
                #endif

                executeOp(static_cast<Opcode>(readByte()));

                #if WATCH_REG
                    printRegister();
                #endif
            }
        #else
            executeOp(static_cast<Opcode>(0));
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

    #if WATCH_EXEC
        this->dis = nullptr;
    #endif
}

/* FuncContext logic. */

VM::FuncContext::FuncContext(const Args& args) :
    regStart(args.regStart), ip(args.ip), end(args.end),
    pool(args.pool)
    #if WATCH_EXEC
    , dis(args.dis)
    #endif
    {}