#include "../../include/core.h"
#include "../../include/error.h"
#include <fast_float/fast_float.h>

/* Forward declarations. */

#define CTOR(name) ::Object name(iter it, u8 argCount);

namespace Core
{
    namespace Ctors
    {
        CTOR(Object);
        CTOR(Int);
        CTOR(Dec);
        CTOR(Bool);
        CTOR(Text);
        CTOR(String);
        CTOR(Range);
        CTOR(List);
        CTOR(Table);
    };
};

#undef CTOR


/* Utility arrays. */

const std::array<Core::sv, to_num(Core::Ctor::Count)>
Core::Ctors::names{
    "Object",
    "Int",
    "Dec",
    "Bool",
    "Text",
    "String",
    "Range",
    "List",
    "Table"
};

const std::array<Core::Callable::Ctor, to_num(Core::Ctor::Count)>
Core::Ctors::impls{
    Core::Ctors::Object,
    Core::Ctors::Int,
    Core::Ctors::Dec,
    Core::Ctors::Bool,
    Core::Ctors::Text,
    Core::Ctors::String,
    Core::Ctors::Range,
    Core::Ctors::List,
    Core::Ctors::Table
};

const std::unordered_map<ObjType, Core::Ctor>
Core::Ctors::search{
    {ObjType::Void,     Ctor::Object},
    {ObjType::Int,      Ctor::Int},
    {ObjType::Dec,      Ctor::Dec},
    {ObjType::Bool,     Ctor::Bool},
    {ObjType::Text,     Ctor::Text},
    {ObjType::String,   Ctor::String},
    {ObjType::Range,    Ctor::Range},
    {ObjType::List,     Ctor::List},
    {ObjType::Table,    Ctor::Table}
};

const std::array<ObjType, to_num(Core::Ctor::Count)>
Core::Ctors::types{
    ObjType::Void,
    ObjType::Int,
    ObjType::Dec,
    ObjType::Bool,
    ObjType::Text,
    ObjType::String,
    ObjType::Range,
    ObjType::List,
    ObjType::Table
};


/* Implementations. */

using Obj = ::Object; // To avoid name collisions.

Obj Core::Ctors::Object(iter it, u8 args)
{
    (void) it;
    checkArity(0, 0, true, args);

    return Obj::typed(ObjType::Void);
}

Obj Core::Ctors::Int(iter it, u8 args)
{
    checkArity(0, 2, false, args);

    if (args == 0)
        return Obj{i64(0)};

    if (args == 1)
    {
        if (IS_INT(*it))
            return Obj{AS_INT(*it)};
        else if (IS_DEC(*it))
            return Obj{static_cast<i64>(AS_DEC(*it))};
        else if (IS_STRING_LIKE(*it))
        {
            std::string_view str{it->getObjectText()};
            i64 value{};
            auto answer{fast_float::from_chars(str.data(), str.data() + str.size(),
                value)};

            if (!answer)
                throw RuntimeError(NUMERIC_LIT_PARSE_FAIL);
            return Obj{value};
        }
        else
            throw RuntimeError(WRONG_ARG_TYPE, "argument must be a number or a string");
    }

    if (args == 2)
    {
        if (!IS_STRING_LIKE(it[0]))
            throw RuntimeError(WRONG_ARG_TYPE, "first argument is not a string");
        if (!IS_INT(it[1]))
            throw RuntimeError(WRONG_ARG_TYPE, "second argument is not an integer");

        i64 base{AS_INT(it[1])};
        if ((base < 2) || (base > 36))
            throw RuntimeError(INVALID_NUM_BASE, "base must be >= 2 and <= 36");

        std::string_view str{it->getObjectText()};
        i64 value{};
        auto answer{fast_float::from_chars(str.data(), str.data() + str.size(),
            value, static_cast<int>(base))};

        if (!answer)
            throw RuntimeError(NUMERIC_LIT_PARSE_FAIL);
        return Obj{value};
    }

    CH_UNREACHABLE();
}

Obj Core::Ctors::Dec(iter it, u8 args)
{
    checkArity(0, 1, true, args);

    if (args == 0)
        return Obj{0.0};

    if (args == 1)
    {
        if (IS_DEC(*it))
            return Obj{AS_DEC(*it)};
        else if (IS_INT(*it))
            return Obj{static_cast<double>(AS_INT(*it))};
        else if (IS_STRING_LIKE(*it))
        {
            std::string_view str{it->getObjectText()};
            double value{};
            auto answer{fast_float::from_chars(str.data(), str.data() + str.size(),
                value)};

            if (!answer)
                throw RuntimeError(NUMERIC_LIT_PARSE_FAIL);
            return Obj{value};
        }
        else
            throw RuntimeError(WRONG_ARG_TYPE, "argument must be a number or a string");
    }

    CH_UNREACHABLE();
}

Obj Core::Ctors::Bool(iter it, u8 args)
{
    checkArity(0, 1, true, args);

    if (args == 0)
        return Obj{false};
    return Obj{it->isTruthy()};
}

Obj Core::Ctors::Text(iter it, u8 args)
{
    checkArity(0, 1, true, args);

    if (args == 0)
        return Obj{CH_ALLOC_TEXT("")};
    return Obj{CH_ALLOC_TEXT(it->printVal())};
}

Obj Core::Ctors::String(iter it, u8 args)
{
    checkArity(0, 1, true, args);

    if (args == 0)
        return Obj{CH_ALLOC_STRING("")};
    return Obj{CH_ALLOC_STRING(it->printVal())};
}

Obj Core::Ctors::Range(iter it, u8 args)
{
    checkArity(2, 3, true, args);

    if (!IS_INT(it[0]) || !IS_INT(it[1]) || ((args == 3) && !IS_INT(it[2])))
        throw RuntimeError(WRONG_ARG_TYPE, "arguments must be integers");

    i64 start{AS_INT(it[0])};
    i64 stop{AS_INT(it[1])};

    // Step size 0 is allowed if start == stop (step size will never be used).
    if ((args == 3) && (AS_INT(it[2]) == 0) && (start != stop))
        throw RuntimeError(INVALID_RANGE_STEP, "cannot have a step size of zero");

    std::array nums{start, stop, ((stop >= start) ? i64(1) : i64(-1))};
    if (args == 3) nums[2] = AS_INT(it[2]);

    Range::validateRange(nums); // May throw on error.
    return Obj{CH_ALLOC_RANGE(nums)};
}

Obj Core::Ctors::List(iter it, u8 args)
{
    checkArity(0, 1, true, args);

    auto* list{CH_ALLOC_LIST(DEFAULT_LIST_SIZE)};
    if (args == 1)
    {
        if (!IS_ITERABLE(*it))
            throw RuntimeError(WRONG_ARG_TYPE, "argument is not iterable");
        ObjIter* iter{it->makeIter()}; // Guaranteed to succeed.
        Obj temp{};

        if (iter->start(temp))
        {
            list->array.push(temp);
            while (iter->next(temp))
                list->array.push(temp);
        }

        CH_DEALLOC(iter);
    }

    return list;
}

Obj Core::Ctors::Table(iter it, u8 args)
{
    checkArity(0, 1, true, args);

    auto* table{CH_ALLOC_TABLE()};
    auto insertEntry = [table](const Obj& obj) {
        if (!IS_LIST(obj) || (AS_LIST(obj)->array.count() != 2))
        {
            throw RuntimeError(WRONG_ARG_TYPE,
                "every collection entry must be a list of size 2");
        }

        const auto& array{AS_LIST(obj)->array};
        table->table.add(array[0], array[1]);
    };

    if (args == 1)
    {
        // Lists and tables are the only compatible collection type.
        if (!IS_LIST(*it) && !IS_TABLE(*it))
        {
            throw RuntimeError(WRONG_ARG_TYPE,
                "argument must be a collection of key-value pairs");
        }

        ObjIter* iter{it->makeIter()}; // Guaranteed to succeed.
        Obj temp{};

        if (iter->start(temp))
        {
            insertEntry(temp);
            while (iter->next(temp))
                insertEntry(temp);
        }

        CH_DEALLOC(iter);
    }

    return table;
}