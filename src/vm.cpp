#include "../include/vm.h"
#include "../include/bytecode.h"
#include "../include/common.h"
#include "../include/config.h"
#include "../include/disasm.h"
#include "../include/error.h"
#include "../include/linear_alloc.h"
#include "../include/natives.h"
#include "../include/object.h"
#include "../include/opcodes.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint> // For INT64_MIN.
#include <cstring>
#include <string>
#include <utility> // For std::move.

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

    #undef MAX
#else
    #define SET_REGSLOT(slot)
    #define SET_REGSLOT_MAX(a, b)
#endif

VM::VM()
{
    for (ui8 i{0}; i < Natives::FuncType::NUM_FUNCS; i++)
        globalRegisters[i] = Object(Natives::FuncType(i));
    SET_REGSLOT(Natives::NUM_FUNCS);
}

VM::~VM()
{
    delete[] globalRegisters;
}

inline ui8 VM::readByte()
{
    ip++;
    return *(ip - 1);
}

inline ui16 VM::readShort()
{
    ui16 b1{ip[0]};
    ui16 b2{ip[1]};
    ip += 2;
    return static_cast<ui16>((b1 << 8) | b2);
}

inline ui32 VM::readLong()
{
    ui32 b1{ip[0]};
    ui32 b2{ip[1]};
    ui32 b3{ip[2]};
    ui32 b4{ip[3]};
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
        case OBJ_STRING:    return (AS_STRING(obj)->str.size() != 0);
        case OBJ_LIST:      return (AS_LIST(obj)->array.count() != 0);
        case OBJ_TABLE:     return (AS_TABLE(obj)->table.size() != 0);
        case OBJ_TUPLE:     return (AS_TUPLE(obj)->entries.count() != 0);
        // Rest are always truthy.
        default:            return true;
    }
}

inline Cell* VM::captureValue(ui8 slot)
{
    // We always capture from the current scope.
    Object* addr{registers + slot};
    for (auto it{activeCells.rbegin()}; it != activeCells.rend(); it++)
    {
        Cell* cell{*it};
        if (cell->location < addr)
            break;
        else if (cell->location == addr)
            return cell;
    }

    Cell* cell{CH_ALLOC(Cell, addr)};
    // Insert the cell in sorted order.
    auto it{std::lower_bound(activeCells.begin(),
        activeCells.end(),
        cell,
        [](Cell* c1, Cell* c2) -> bool {
            return c1->location < c2->location;
        }
    )};
    activeCells.insert(it, cell);
    return cell;
}

inline void VM::closeCells(Object* limit)
{
    // Close all cells that were declared *in this scope*.
    // Do not clear or close ALL cells.
    while (!activeCells.empty())
    {
        Cell* cell{activeCells.back()};
        if (cell->location < limit)
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

Object VM::concatStrings(const Object& str1, const Object& str2)
{
    std::string concat{AS_STRING(str1)->str + AS_STRING(str2)->str};
    return CH_ALLOC(String, concat);
}

Object VM::makeRange(const Object& start, const Object& stop)
{
    if (!IS_INT(start) || !IS_INT(stop))
    {
        throw TypeMismatch(
            "Can only construct ranges from integer values.",
            OBJ_INT,
            IS_INT(start) ? stop.type : start.type
        );
    }

    std::array<i64, 3> nums{AS_INT(start), AS_INT(stop), 1};
    return CH_ALLOC(Range, nums);
}

Object VM::makeReference()
{
    // Mimicking the compiler.
    enum VarType : ui8 { GLOBAL, CELL, LOCAL };

    VarType type{static_cast<VarType>(readByte())};
    ui8 index{readByte()};
    Object* addr{};

    switch (type)
    {
        case GLOBAL:
            addr = &globalRegisters[index];
            break;
        case CELL:
            addr = currentClosure->cells[index]->location;
            break;
        case LOCAL:
            addr = &registers[index];
            break;
        default:
            CH_UNREACHABLE();
    }

    return CH_ALLOC(Cell, addr);
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
        default: CH_UNREACHABLE();
    }
}

Object VM::arithOper(Opcode oper, ui8 firstOper)
{
    const Object& a{registers[firstOper]};
    const Object& b{registers[readByte()]};

    if (IS_INT(a) && IS_INT(b))
    {
        i64 aVal{AS_INT(a)};
        i64 bVal{AS_INT(b)};
        switch (oper)
        {
            case OP_ADD:    return aVal + bVal;
            case OP_SUB:    return aVal - bVal;
            case OP_MULT:   return aVal * bVal;
            case OP_DIV:
            {
                if (bVal == 0)
                    throw RuntimeError(Token(), "Division by zero.");
                return static_cast<double>(aVal) / bVal;
            }
            case OP_MOD:
            {
                if (bVal == 0)
                    throw RuntimeError(Token(), "Modulus with divisor zero.");
                return aVal % bVal;
            }
            case OP_POWER:  return static_cast<i64>(pow(aVal, bVal));
            default: CH_UNREACHABLE();
        }
    }
    else if (IS_NUM(a) && IS_NUM(b))
    {
        double aVal{static_cast<double>(AS_NUM(a))};
        double bVal{static_cast<double>(AS_NUM(b))};
        switch (oper)
        {
            case OP_ADD:    return aVal + bVal;
            case OP_SUB:    return aVal - bVal;
            case OP_MULT:   return aVal * bVal;
            case OP_DIV:
            {
                if (bVal == 0.0)
                    throw RuntimeError(Token(), "Division by zero.");
                return aVal / bVal;
            }
            case OP_POWER:  return pow(aVal, bVal);
            case OP_MOD:
            {
                throw TypeMismatch(
                    "Cannot apply modulus operator to non-integers.",
                    OBJ_INT,
                    (IS_INT(a) ? b.type : a.type)
                );
            }
            default: CH_UNREACHABLE();
        }
    }
    else if (IS_STRING(a) && IS_STRING(b) && (oper == OP_ADD))
        return concatStrings(a, b);
    else
    {
        throw TypeMismatch(
            "Cannot apply arithmetic operator to non-numeric values.",
            OBJ_NUM,
            (IS_NUM(a) ? b.type : a.type)
        );
    }
}

Object VM::compareOper(Opcode op, ui8 firstOper)
{
    const Object& a{registers[firstOper]};
    const Object& b{registers[readByte()]};

    if (((op == OP_GT) || (op == OP_LT))
        && (!IS_COMPARABLE(a) || !IS_COMPARABLE(b)))
    {
        throw TypeMismatch(
            "Cannot compare given values.",
            OBJ_COMPARABLE,
            IS_COMPARABLE(a) ? b.type : a.type
        );
    }

    switch (op)
    {
        case OP_EQUAL:  return (a == b);
        case OP_GT:     return (a > b);
        case OP_LT:     return (a < b);
        case OP_IN:     return a.in(b);
        default: CH_UNREACHABLE();
    }
}

static inline i64 fromUnsigned(ui64 num)
{
    i64 i{};
    std::memcpy(&i, &num, sizeof(ui64));
    return i;
}

Object VM::bitOper(Opcode op, ui8 firstOper)
{
    const Object& a{registers[firstOper]};
    const Object& b{registers[readByte()]};

    if (!IS_INT(a) || !IS_INT(b))
    {
        throw TypeMismatch(
            "Cannot apply bitwise operator to non-integer values.",
            OBJ_INT,
            (IS_INT(a) ? b.type : a.type)
        );
    }

    ui64 aVal{AS_UINT(a)};
    ui64 bVal{AS_UINT(b)};

    switch (op)
    {
        case OP_AND:        return fromUnsigned(aVal & bVal);
        case OP_OR:         return fromUnsigned(aVal | bVal);
        case OP_XOR:        return fromUnsigned(aVal ^ bVal);
        case OP_SHIFT_L:
        {
            if (bVal >= 64)
                throw RuntimeError(Token(), "Shift value too high.");
            return fromUnsigned(aVal << bVal);
        }
        case OP_SHIFT_R:
        {
            if (bVal >= 64)
                throw RuntimeError(Token(), "Shift value too high.");
            // Manually perform wraparound to maintain LHS signed-ness.
            i64 term{(AS_INT(a) >= 0) ? 0 : INT64_MIN};
            return fromUnsigned(aVal >> bVal) + term;
        }
        default: CH_UNREACHABLE();
    }
}

Object VM::unaryOper(Opcode op, ui8 oper)
{
    const Object& obj{registers[oper]};

    switch (op)
    {
        case OP_INCR:
        case OP_DECR:
        {
            if (!IS_NUM(obj))
            {
                throw TypeMismatch(
                    "Cannot increment or decrement a non-numeric value.",
                    OBJ_NUM,
                    obj.type
                );
            }
            if (IS_INT(obj))
                return AS_INT(obj) + i64(op == OP_INCR ? 1 : -1);
            else
                return AS_DEC(obj) + double(op == OP_INCR ? 1 : -1);
        }
        case OP_NEG:
        {
            if (!IS_NUM(obj))
            {
                throw TypeMismatch(
                    "Cannot negate a non-numeric value.",
                    OBJ_NUM,
                    obj.type
                );
            }
            if (IS_INT(obj))
                return i64(AS_INT(obj) * -1);
            else
                return (AS_DEC(obj) * -1);
        }
        case OP_NOT: return !isTruthy(obj);
        case OP_COMP:
        {
            if (!IS_INT(obj))
            {
                throw TypeMismatch(
                    "Cannot apply bitwise operator to non-integer values.",
                    OBJ_INT,
                    obj.type
                );
            }
            return i64(~AS_UINT(obj));
        }
        default: CH_UNREACHABLE();
    }
}

void VM::callFunc(const Object& callee, ui8 start, ui8 argCount)
{
    Function* func{};
    Closure* closure{};

    if (IS_CLOSURE(callee))
    {
        closure = AS_CLOSURE(callee);
        func = closure->function;
    }
    else
    {
        closure = nullptr;
        func = AS_FUNC(callee);
    }

    if (func->argCount != argCount)
    {
        throw RuntimeError(
            Token(),
            CH_STR("Expected {} argument{} but found {}.",
            func->argCount, (func->argCount == 1 ? "" : "s"), argCount)
        );
    }

    const ByteCode& code{func->code};
    frames.emplace_back(CallFrame::Args{
        currentFunc, currentClosure, registers, ip
        #if WATCH_EXEC
        , this->dis
        #endif
    });

    currentFunc = func;
    currentClosure = closure;
    registers += start;
    ip = code.block.data();
    end = ip + code.block.size();
    pool = code.pool.data();

    #if WATCH_EXEC
        this->dis = new Disassembler(code);
    #endif
}

void VM::callNative(const Object& callee, ui8 start, ui8 argCount)
{
    auto* func{Natives::functions[AS_NATIVE(callee)]};
    func(&registers[start], argCount, Token());
}

void VM::callObj(const Object& callee, ui8 start, ui8 argCount)
{
    if (!IS_CALLABLE(callee))
    {
        throw TypeMismatch(
            "Object is not callable.",
            OBJ_FUNC,
            callee.type
        );
    }

    switch (callee.type)
    {
        case OBJ_NATIVE:
            callNative(callee, start, argCount);
            break;
        case OBJ_FUNC:
        case OBJ_CLOSURE:
        case OBJ_LAMBDA:
            callFunc(callee, start, argCount);
            break;
        default:
            CH_UNREACHABLE();
    }
}

inline void VM::restoreData()
{
    CallFrame& frame{frames.back()};
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
}

// Handle regSlot.
void VM::startIter()
{
    Object& var{registers[readByte()]};
    Object& iterable{registers[readByte()]};

    ObjIter* iter{};
    if ((iter = iterable.makeIter()) == nullptr)
    {
        throw TypeMismatch(
            "Given object is not iterable.",
            OBJ_ITER,
            iterable.type
        );
    }

    if (iter->start(var))
    {
        iterable = Object{iter};
        ip += 3; // Skip our fail-case jump.
        #if WATCH_EXEC
            this->dis->ip += 3;
        #endif
    }
}

void VM::updateIter()
{
    closeCells(scopeStarts.back());

    Object& var{registers[readByte()]};
    Object& iter{registers[readByte()]};
    ui16 jump{readShort()};

    if (AS_ITER(iter)->next(var))
    {
        ip -= jump;
        #if WATCH_EXEC
            this->dis->ip -= jump;
        #endif
    }
}

#if WATCH_REG
#include "../include/common.h"

void VM::printRegister()
{
    ui8 i{0};
    while (i <= regSlot)
    {
        if (!IS_VALID(registers[i]))
            break;
        CH_PRINT("[{}]", registers[i].printVal());
        i++;
    }
    if (i != 0) CH_PRINT("\n");
}

#endif

void VM::executeOp(Opcode op)
{
    #if CH_COMPUTED_GOTO
        static void* dispatchTable[] = {
            #define LABEL_ENABLE(op)    &&CASE_##op
            #define LABEL_DISABLE(op)   &&CASE_NO_REACH

            #define LABEL(op, state) LABEL_##state(op),
            #include "../include/opcode_list.inc"
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
                CH_ASSERT(IS_VALID_OP(op),                                          \
                    CH_STR("Invalid opcode {}.", static_cast<ui8>(op)));            \
                DEBUG_OP(op);                                                       \
                DISPATCH_OP(op);                                                    \
            } while (false)
        #define SWITCH(op)  DISPATCH();
        #define CASE(op)    CASE_##op
        #define DEFAULT     CASE_NO_REACH

    #else // if !CH_COMPUTED_GOTO
        #define SWITCH(op)  switch (op)
        #define CASE(op)    case op
        #define DISPATCH()  break
        #define DEFAULT     default

    #endif

    SWITCH(op)
    {
        CASE(OP_LOAD_R):
        {
            ui8 dest{readByte()};
            registers[dest] = loadOper();
            SET_REGSLOT(dest);
            DISPATCH();
        }
        CASE(OP_MOVE_R):
        {
            ui8 dest{readByte()};
            ui8 src{readByte()};
            registers[dest] = std::move(registers[src]);
            SET_REGSLOT_MAX(dest, src);
            DISPATCH();
        }

        CASE(OP_LOOP):
        {
            ui16 jump{readShort()};
            ip -= jump;
            #if WATCH_EXEC
                this->dis->ip -= jump;
            #endif
            DISPATCH();
        }
        CASE(OP_JUMP):
        {
            ui16 jump{readShort()};
            ip += jump;
            #if WATCH_EXEC
                this->dis->ip += jump;
            #endif
            DISPATCH();
        }
        CASE(OP_JUMP_TRUE):
        {
            ui8 check{readByte()};
            ui16 jump{readShort()};
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
            ui8 check{readByte()};
            ui16 jump{readShort()};
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
            ui8 dest{readByte()};
            ui8 src{readByte()};

            if (IS_REF(registers[dest]))
            {
                Cell* cell{AS_REF(registers[dest])};
                COPY(*(cell->location), globalRegisters[src]);
            }
            else
                COPY(registers[dest], globalRegisters[src]);
            SET_REGSLOT(dest);
            DISPATCH();
        }
        CASE(OP_SET_GLOBAL):
        {
            ui8 dest{readByte()};
            ui8 src{readByte()};

            if (IS_REF(registers[src]))
            {
                Cell* cell{AS_REF(registers[src])};
                COPY(globalRegisters[dest], *(cell->location));
            }
            else
                COPY(globalRegisters[dest], registers[src]);
            DISPATCH();
        }

        CASE(OP_GET_CELL):
        {
            ui8 dest{readByte()};
            ui8 src{readByte()};
            Object& obj{*(currentClosure->cells[src]->location)};

            if (IS_REF(obj))
            {
                Cell* cell{AS_REF(obj)};
                COPY(registers[dest], *(cell->location));
            }
            else
                COPY(registers[dest], obj);
            SET_REGSLOT(dest);
            DISPATCH();
        }
        CASE(OP_SET_CELL):
        {
            ui8 dest{readByte()};
            ui8 src{readByte()};
            Object& obj{*(currentClosure->cells[dest]->location)};

            if (IS_REF(obj))
            {
                Cell* cell{AS_REF(obj)};
                COPY(*(cell->location), registers[src]);
            }
            else
                COPY(obj, registers[src]);
            DISPATCH();
        }

        CASE(OP_GET_LOCAL):
        CASE(OP_SET_LOCAL):
        {
            ui8 dest{readByte()};
            ui8 src{readByte()};
            Object* destObj{&registers[dest]};
            Object* srcObj{&registers[src]};

            if (IS_REF(*destObj))
                destObj = AS_REF(*destObj)->location;
            if (IS_REF(*srcObj))
                srcObj = AS_REF(*srcObj)->location;

            COPY(*destObj, *srcObj);
            SET_REGSLOT_MAX(dest, src);
            DISPATCH();
        }

        CASE(OP_LIST):
        {
            registers[readByte()] = CH_ALLOC(List, DEFAULT_LIST_SIZE);
            SET_REGSLOT(*(ip - 1));
            DISPATCH();
        }
        CASE(OP_EXT_LIST):
        {
            ui8 listReg{readByte()};
            ui8 startReg{readByte()};
            ui8 count{readByte()};

            auto& array{AS_LIST(registers[listReg])->array};
            for (ui8 i{0}; i < count; i++)
                array.push(registers[startReg + i]);
            DISPATCH();
        }

        CASE(OP_RANGE):
        {
            Object& start{registers[readByte()]};
            Object& stop{registers[readByte()]};

            start = makeRange(start, stop);
            DISPATCH();
        }
        CASE(OP_FORMAT_STR):
        {
            // Artificial block scope since the std::string
            // destructor will not be called if 'goto' is
            // used (the block scope calls all destructors when it
            // ends before 'goto' is reached).

            {
                std::string str{};
                ui8 index{readByte()};
                ui8 count{readByte()};

                for (ui8 i{0}; i < count; i++)
                    str += registers[index + i].printVal();
                registers[index] = CH_ALLOC(String, str);
            }
            DISPATCH();
        }

        CASE(OP_TUPLE):
        {
            registers[readByte()] = CH_ALLOC(Tuple);
            SET_REGSLOT(*(ip - 1));
            DISPATCH();
        }
        CASE(OP_EXT_TUPLE):
        {
            ui8 tupleReg{readByte()};
            ui8 startReg{readByte()};
            ui8 count{readByte()};
            auto& entries{AS_TUPLE(registers[tupleReg])->entries};
            for (ui8 i{0}; i < count; i++)
                entries.push(registers[startReg + i]);
            DISPATCH();
        }

        CASE(OP_MAKE_ITER):
        {
            startIter();
            DISPATCH();
        }
        CASE(OP_UPDATE_ITER):
        {
            updateIter();
            DISPATCH();
        }

        // Arithmetic operators.

        CASE(OP_ADD):   CASE(OP_SUB):   CASE(OP_MULT):
        CASE(OP_DIV):   CASE(OP_MOD):   CASE(OP_POWER):
        {
            ui8 dest{readByte()};
            registers[dest] = arithOper(op, dest);
            SET_REGSLOT(regSlot - 1);
            DISPATCH();
        }

        // Comparison operators.

        CASE(OP_GT):    CASE(OP_LT):    CASE(OP_EQUAL):     CASE(OP_IN):
        {
            ui8 dest{readByte()};
            registers[dest] = compareOper(op, dest);
            SET_REGSLOT(regSlot - 1);
            DISPATCH();
        }

        // Bit-wise operators.

        CASE(OP_AND):       CASE(OP_OR):        CASE(OP_XOR):
        CASE(OP_SHIFT_L):   CASE(OP_SHIFT_R):
        {
            ui8 dest{readByte()};
            registers[dest] = bitOper(op, dest);
            SET_REGSLOT(regSlot - 1);
            DISPATCH();
        }

        // Unary operators.

        CASE(OP_INCR):      CASE(OP_DECR):      CASE(OP_NOT):
        CASE(OP_NEG):       CASE(OP_COMP):
        {
            ui8 dest{readByte()};
            registers[dest] = unaryOper(op, dest);
            DISPATCH();
        }

        CASE(OP_PRINT_VALID):
        {
            const Object& obj{registers[readByte()]};
            if (IS_VALID(obj) && !IS_TUPLE(obj))
                CH_PRINT("{}\n", obj.printVal());
            DISPATCH();
        }

        // Functions.

        CASE(OP_CALL_NAT):
        {
            ui8 callee{readByte()};
            ui8 start{readByte()};
            ui8 argCount{readByte()};

            #if WATCH_REG
                ui8 currentSlot = regSlot;
            #endif
            SET_REGSLOT(start);

            const auto& func{Natives::functions[callee]};
            func(&registers[start], argCount, Token()); // Temporarily.

            SET_REGSLOT(currentSlot);
            DISPATCH();
        }
        CASE(OP_CALL_DEF):
        {
            const Object& callee{registers[readByte()]};
            ui8 start{readByte()};
            ui8 argCount{readByte()};

            callObj(callee, start, argCount);
            SET_REGSLOT(0);
            DISPATCH();
        }

        CASE(OP_RETURN):
        {
            ui8 retSlot{readByte()};
            registers[-1] = std::move(registers[retSlot]);

            // Correct regSlot after return.
            restoreData();
            closeCells(registers);
            DISPATCH();
        }
        CASE(OP_VOID):
        {
            // To avoid reallocating the return value each time.
            static Object ret{CH_ALLOC(Tuple)};
            registers[readByte()] = ret;
            DISPATCH();
        }

        CASE(OP_CLOSURE):
        {
            ui8 slot{readByte()};
            auto* func{AS_FUNC(registers[slot])};
            registers[slot] = CH_ALLOC(Closure, func);
            DISPATCH();
        }
        CASE(OP_CAPTURE_VAL):
        {
            auto* closure{AS_CLOSURE(registers[readByte()])};
            ui8 slot{readByte()};

            closure->addCell(captureValue(slot));
            DISPATCH();
        }
        CASE(OP_CAPTURE_CELL):
        {
            auto* closure{AS_CLOSURE(registers[readByte()])};
            ui8 index{readByte()};

            closure->addCell(currentClosure->cells[index]);
            DISPATCH();
        }

        CASE(OP_MAKE_REF):
        {
            ui8 slot{readByte()};
            registers[slot] = makeReference();
            DISPATCH();
        }

        CASE(OP_ENTER_SCOPE):
        {
            scopeStarts.emplace_back(registers + readByte());
            DISPATCH();
        }
        CASE(OP_EXIT_SCOPE):
        {
            closeCells(scopeStarts.back());
            scopeStarts.pop_back();
            DISPATCH();
        }

        DEFAULT:
        {
            #if defined(DEBUG)
                CH_ASSERT(false, CH_STR("Opcode {} should not be reachable.", 
                    static_cast<ui8>(op)));
            #elif defined(NDEBUG)
                CH_UNREACHABLE();
            #endif
        }
    }
}

void VM::executeCode(Function* script)
{
    currentFunc = script;
    // The global scope doesn't capture any variables,
    // so it doesn't need to have an active closure.
    registers = globalRegisters;
    ip = script->code.block.data();
    end = ip + script->code.block.size();
    pool = script->code.pool.data();

    #if WATCH_EXEC
        Disassembler dis(script->code);
        this->dis = &dis;
    #endif

    frames.reserve(CALL_FRAMES_DEFAULT);
    scopeStarts.reserve(MAX_SCOPE_DEPTH);
    activeCells.reserve(CODE_MAX);

    try
    {
        #if !CH_COMPUTED_GOTO
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
    }
    catch (RuntimeError& error)
    {
        error.report();
    }

    #if WATCH_EXEC
        this->dis = nullptr;
    #endif

    frames.clear();
    scopeStarts.clear();
}

/* FuncContext logic. */

VM::CallFrame::CallFrame(const Args& args) :
    function(args.function),
    closure(args.closure),
    regStart(args.regStart),
    ip(args.ip)
    #if WATCH_EXEC
    , dis(args.dis)
    #endif
    {}