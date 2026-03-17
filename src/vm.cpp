#include "../include/vm.h"
#include "../include/disasm.h"
#include "../include/error.h"
#include "../include/linear_alloc.h"
#include "../include/natives.h"
#include "../include/opcodes.h"
#include "../include/object.h"
#include <algorithm>
#include <cmath>
#include <cstring>

#if COPY_INLINE
    #define COPY(a, b) copyObject((a), (b))
#else
    #define COPY(a, b) (a) = (b)
#endif

#if WATCH_REG
    #define SET_REGSLOT(slot)   \
        do {                    \
            regSlot = slot;     \
        } while (false)
    
    #undef MAX
    #define MAX(a, b) (((a) > (b)) ? (a) : (b))
    
    #define SET_REGSLOT_MAX(a, b)   \
        do {                        \
            regSlot = MAX(a, b);    \
        } while (false)
#else
    #define SET_REGSLOT(slot)
    #define SET_REGSLOT_MAX(a, b)
#endif

#undef MAX
#define MAX(a, b) (a > b ? a : b)

VM::VM() :
    registers(new Object[regSize])
{
    for (ui8 i = 0; i < Natives::FuncType::NUM_FUNCS; i++)
        registers[i] = Object(Natives::FuncType(i));
    SET_REGSLOT(Natives::NUM_FUNCS);
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
        case OBJ_INT:       return (AS_(int, obj) != 0);
        case OBJ_DEC:       return (AS_(dec, obj) != 0.0);
        case OBJ_BOOL:      return AS_(bool, obj);
        case OBJ_NULL:      return false;
        case OBJ_STRING:    return (AS_(string, obj)->str.size() != 0);
        // Functions and ranges are always truthy.
        default:            return true;
    }
}

inline Cell* VM::captureValue(ui8 depth, ui8 slot)
{
    Object* addr = depthRecords[depth].window + slot;
    for (auto it = activeCells.rbegin(); it != activeCells.rend(); it++)
    {
        Cell* cell = *it;
        if (cell->location < addr)
            break;
        if (cell->location == addr)
            return cell;
    }

    Cell* cell = ALLOC(Cell, addr);
    // Insert the cell in sorted order.
    auto it = std::lower_bound(activeCells.begin(),
        activeCells.end(),
        cell,
        [](Cell* c1, Cell* c2) -> bool {
            return c1->location < c2->location;
        }
    );
    activeCells.insert(it, cell);
    return cell;
}

inline void VM::closeCells()
{
    // Close all cells that were declared *in this scope*.
    // Do not clear or close ALL cells.
    while (!activeCells.empty())
    {
        Cell* cell = activeCells.back();
        if (cell->location < registers)
            break;
        cell->close();
        activeCells.pop_back();
    }
}

#if COPY_INLINE
    inline void VM::copyObject(Object& dest, const Object& src)
    {
        if (IS_PRIMITIVE(dest) && IS_PRIMITIVE(src))
        {
            dest.type = src.type;
            dest.as = src.as;
            return;
        }

        dest = src;
    }
#endif

inline void VM::pushScope(ui8 depth, Object* window, Cell** cells)
{
    scopeUndo.push_back({depth, depthRecords[depth]});
    depthRecords[depth] = { window, cells };
}

inline void VM::popScope()
{
    const auto& scope = scopeUndo.back();
    depthRecords[scope.offset] = scope.record;
    scopeUndo.pop_back();
}

inline void VM::clearScopes(bool keepGlobal)
{
    if (frames.size() == 0) return;

    if (keepGlobal)
        frames.resize(1);
    else
        frames.clear();

    scopeUndo.clear();
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
    std::string concat = AS_(string, str1)->str + AS_(string, str2)->str;
    return ALLOC(String, concat);
}

Object VM::arithOper(Opcode oper, ui8 firstOper)
{
    const Object& a = registers[firstOper];
    const Object& b = registers[readByte()];

    if (IS_(INT, a) && IS_(INT, b))
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
    else if (IS_(STRING, a) && IS_(STRING, b) && (oper == OP_ADD))
        return concatStrings(a, b);
    else
        throw TypeMismatch(
            "Cannot apply arithmetic operator to non-numeric values.",
            OBJ_NUM,
            (IS_NUM(a) ? b.type : a.type)
        );
}

Object VM::compareOper(Opcode op, ui8 firstOper)
{
    const Object& a = registers[firstOper];
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
            if (IS_(STRING, a) && IS_(STRING, b))
            {
                const String& s1 = *(AS_(string, a));
                const String& s2 = *(AS_(string, b));
                return s2.contains(s1);
            }
            else if (IS_(INT, a) && IS_(RANGE, b))
            {
                const Range& range = *(AS_(range, b));
                return range.contains(AS_(int, a));
            }
            else if (!IS_(STRING, b) && !IS_(RANGE, b))
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

Object VM::bitOper(Opcode op, ui8 firstOper)
{
    const Object& a = registers[firstOper];
    const Object& b = registers[readByte()];

    if (!IS_(INT, a) || !IS_(INT, b))
        throw TypeMismatch(
            "Cannot apply bitwise operator to non-integer values.",
            OBJ_INT,
            (IS_(INT, a) ? b.type : a.type)
        );

    ui64 aVal = AS_UINT(a);
    ui64 bVal = AS_UINT(b);

    switch (op)
    {
        case OP_AND:        return fromUnsigned(aVal & bVal);
        case OP_OR:         return fromUnsigned(aVal | bVal);
        case OP_XOR:        return fromUnsigned(aVal ^ bVal);
        case OP_SHIFT_L:    return fromUnsigned(aVal << bVal);
        case OP_SHIFT_R:    return fromUnsigned(aVal >> bVal);
        default: UNREACHABLE();
    }
}

Object VM::unaryOper(Opcode op, ui8 oper)
{
    const Object& obj = registers[oper];

    switch (op)
    {
        case OP_INCR:
        case OP_DECR:
        {
            if (!IS_NUM(obj))
                throw TypeMismatch(
                    "Cannot increment or decrement a non-numeric value.",
                    OBJ_NUM,
                    obj.type
                );
            if (IS_(INT, obj))
                return AS_(int, obj) + i64(op == OP_INCR ? 1 : -1);
            else
                return AS_(dec, obj) + double(op == OP_INCR ? 1 : -1);
        }
        case OP_NEG:
        {
            if (!IS_NUM(obj))
                throw TypeMismatch(
                    "Cannot negate a non-numeric value.",
                    OBJ_NUM,
                    obj.type
                );
            if (IS_(INT, obj))
                return i64(AS_NUM(obj) * -1);
            else
                return (AS_NUM(obj) * -1);
        }
        case OP_NOT: return !isTruthy(obj);
        case OP_COMP:
        {
            if (!IS_(INT, obj))
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

void VM::callFunc(const Object& callee, ui8 start, ui8 argCount)
{
    Function* func = AS_(func, callee);
    if (func->argCount != argCount)
    {
        throw RuntimeError(
            Token(),
            FORMAT_STR("Expected {} argument{} but found {}.",
            func->argCount, (func->argCount == 1 ? "" : "s"), argCount)
        );
    }

    const ByteCode& code = func->code;
    frames.emplace_back(CallFrame::Args{
        currentFunc, registers, ip,
        static_cast<ui8>(code.depth - 1)
        #if WATCH_EXEC
        , this->dis
        #endif
    });

    currentFunc = func;
    registers += start;
    ip = code.block.data();
    end = ip + code.block.size();
    pool = code.pool.data();

    #if WATCH_EXEC
        this->dis = new Disassembler(code);
    #endif

    Cell** cells = (func->cells.empty() ? nullptr : &(func->cells[0]));
    pushScope(code.depth, registers, cells);
}

void VM::callNative(const Object& callee, ui8 start, ui8 argCount)
{
    auto* func = Natives::functions[AS_(native, callee)];
    (*func)(&registers[start], argCount, Token());
}

void VM::callObj(const Object& callee, ui8 start, ui8 argCount)
{
    if (!IS_CALLABLE(callee))
        throw TypeMismatch(
            "Object is not callable.",
            OBJ_FUNC,
            callee.type
        );

    switch (callee.type)
    {
        case OBJ_NATIVE:
            callNative(callee, start, argCount);
            break;
        case OBJ_FUNC:
            callFunc(callee, start, argCount);
            break;
        default:
            UNREACHABLE();
    }
}

inline void VM::restoreData()
{
    CallFrame& frame = frames.back();
    currentFunc = frame.function;
    registers = frame.regStart;
    ip = frame.ip;
    end = &(currentFunc->code.block.back()) + 1;
    pool = &(currentFunc->code.pool.front());
    #if WATCH_EXEC
        delete this->dis;
        this->dis = frame.dis;
    #endif

    frames.pop_back();
    popScope();
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

        if (AS_(iter, iter)->next(var))
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
                    FORMAT_STR("Invalid opcode {}.", static_cast<ui8>(op)));        \
                DEBUG_OP(op);                                                       \
                DISPATCH_OP(op);                                                    \
            } while (false)
        #define SWITCH(op)  DISPATCH();
        #define CASE(op)    CASE_##op
        #define DEFAULT     CASE_NO_REACH

    #else // if !COMPUTED_GOTO
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
            SET_REGSLOT(dest);
            DISPATCH();
        }
        CASE(OP_MOVE_R):
        {
            ui8 dest = readByte();
            ui8 src = readByte();
            registers[dest] = std::move(registers[src]);
            SET_REGSLOT_MAX(dest, src);
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

        CASE(OP_GET_GLOBAL):
        {
            ui8 dest = readByte();
            ui8 src = readByte();

            COPY(registers[dest], depthRecords[0].window[src]);
            SET_REGSLOT(dest);
            DISPATCH();
        }
        CASE(OP_SET_GLOBAL):
        {
            ui8 dest = readByte();
            ui8 src = readByte();

            COPY(depthRecords[0].window[dest], registers[src]);
            DISPATCH();
        }

        CASE(OP_GET_CELL):
        {
            ui8 dest = readByte();
            ui8 src = readByte();

            COPY(registers[dest], *(currentFunc->cells[src]->location));
            SET_REGSLOT(dest);
            DISPATCH();
        }
        CASE(OP_SET_CELL):
        {
            ui8 dest = readByte();
            ui8 src = readByte();

            COPY(*(currentFunc->cells[dest]->location), registers[src]);
            DISPATCH();
        }

        CASE(OP_GET_LOCAL):
        CASE(OP_SET_LOCAL):
        {
            ui8 dest = readByte();
            ui8 src = readByte();

            COPY(registers[dest], registers[src]);
            SET_REGSLOT_MAX(dest, src);
            DISPATCH();
        }

        CASE(OP_LIST):
        {
            registers[readByte()] = ALLOC(List, DEFAULT_LIST_SIZE);
            SET_REGSLOT(*(ip - 1));
            DISPATCH();
        }
        CASE(OP_EXT_LIST):
        {
            ui8 listReg = readByte();
            ui8 startReg = readByte();
            ui8 count = readByte();

            auto& array = AS_(list, registers[listReg])->array;
            for (ui8 i = 0; i < count; i++)
                array.push(registers[startReg + i]);
            DISPATCH();
        }

        CASE(OP_TUPLE):
        {
            registers[readByte()] = ALLOC(Tuple);
            SET_REGSLOT(*(ip - 1));
            DISPATCH();
        }
        CASE(OP_EXT_TUPLE):
        {
            ui8 tupleReg = readByte();
            ui8 startReg = readByte();
            ui8 count = readByte();
            auto& entries = AS_(tuple, registers[tupleReg])->entries;
            for (ui8 i = 0; i < count; i++)
                entries.push(registers[startReg + i]);
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
            registers[dest] = arithOper(op, dest);
            SET_REGSLOT(regSlot - 1);
            DISPATCH();
        }

        // Comparison operators.

        CASE(OP_GT):    CASE(OP_LT):    CASE(OP_EQUAL):     CASE(OP_IN):
        {
            ui8 dest = readByte();
            registers[dest] = compareOper(op, dest);
            SET_REGSLOT(regSlot - 1);
            DISPATCH();
        }

        // Bit-wise operators.

        CASE(OP_AND):       CASE(OP_OR):        CASE(OP_XOR):
        CASE(OP_SHIFT_L):   CASE(OP_SHIFT_R):
        {
            ui8 dest = readByte();
            registers[dest] = bitOper(op, dest);
            SET_REGSLOT(regSlot - 1);
            DISPATCH();
        }

        // Unary operators.

        CASE(OP_INCR):      CASE(OP_DECR):      CASE(OP_NOT):
        CASE(OP_NEG):       CASE(OP_COMP):
        {
            ui8 dest = readByte();
            registers[dest] = unaryOper(op, dest);
            DISPATCH();
        }

        CASE(OP_PRINT_VALID):
        {
            const Object& obj = registers[readByte()];
            if (IS_VALID(obj) && !IS_(TUPLE, obj))
                FORMAT_PRINT("{}\n", obj.printVal());
            DISPATCH();
        }

        // Functions.

        CASE(OP_CALL_NAT):
        {
            ui8 callee = readByte();
            ui8 start = readByte();
            ui8 argCount = readByte();

            #if WATCH_REG
                ui8 currentSlot = regSlot;
            #endif
            SET_REGSLOT(start);

            auto& func = Natives::functions[callee];
            func(&registers[start], argCount, Token()); // Temporarily.

            SET_REGSLOT(currentSlot);
            DISPATCH();
        }
        CASE(OP_CALL_DEF):
        {
            const Object& callee = registers[readByte()];
            ui8 start = readByte();
            ui8 argCount = readByte();

            callObj(callee, start, argCount);
            SET_REGSLOT(0);
            DISPATCH();
        }

        CASE(OP_RETURN):
        {
            ui8 retSlot = readByte();
            COPY(registers[-1], registers[retSlot]);

            // Correct regSlot after return.
            restoreData();
            closeCells();
            DISPATCH();
        }
        CASE(OP_VOID):
        {
            // To avoid reallocating the return value each time.
            static auto ret = Object(ALLOC(Tuple));
            registers[readByte()] = ret;
            DISPATCH();
        }

        CASE(OP_CAPTURE_VAL):
        {
            auto* func = AS_(func, registers[readByte()]);
            ui8 depth = readByte();
            ui8 slot = readByte();

            func->cells.push(captureValue(depth, slot));
            DISPATCH();
        }
        CASE(OP_CAPTURE_CELL):
        {
            auto* func = AS_(func, registers[readByte()]);
            ui8 depth = readByte();
            ui8 index = readByte();

            func->cells.push(depthRecords[depth].cells[index]);
            DISPATCH();
        }
        CASE(OP_EXIT_SCOPE):
        {
            closeCells();
            DISPATCH();
        }

        DEFAULT:
        {
            #if defined(DEBUG)
                ASSERT(false, FORMAT_STR("Opcode {} should not be reachable.", 
                    static_cast<ui8>(op)));
            #elif defined(NDEBUG)
                UNREACHABLE();
            #endif
        }
    }
}

void VM::executeCode(Function* script)
{
    currentFunc = script;
    ip = script->code.block.data();
    end = ip + script->code.block.size();
    pool = script->code.pool.data();

    #if WATCH_EXEC
        Disassembler dis(script->code);
        this->dis = &dis;
    #endif

    frames.reserve(CALL_FRAMES_DEFAULT);
    depthRecords.reserve(MAX_SCOPE_DEPTH);
    scopeUndo.reserve(MAX_SCOPE_DEPTH);
    activeCells.reserve(CODE_MAX);

    depthRecords[0] = {registers, nullptr};

    try
    {
        #if !COMPUTED_GOTO
            while (ip < end)
            {
                #if WATCH_EXEC
                    this->dis->disassembleOp(*ip);
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
        clearScopes(true); // Escape all nested calls.
    }
    catch (RuntimeError& error)
    {
        error.report();
        clearScopes(true);
    }

    #if WATCH_EXEC
        this->dis = nullptr;
    #endif

    clearScopes(false); // Clear all function scopes (including global script).
}

/* FuncContext logic. */

VM::CallFrame::CallFrame(const Args& args) :
    function(args.function), regStart(args.regStart),
    ip(args.ip), offset(args.offset)
    #if WATCH_EXEC
    , dis(args.dis)
    #endif
    {}