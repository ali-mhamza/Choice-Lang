#include "../include/constructors.h"
#include "../include/common.h"
#include "../include/config.h"
#include "../include/diagnostic.h"
#include "../include/error.h"
#include "../include/linear_alloc.h"
#include "../include/object.h"
#include <fast_float/fast_float.h>
#include <array>
#include <unordered_map>

const std::array<ObjType,
Constructors::CtorType::NUM_CTORS> Constructors::types{
    OBJ_VOID, OBJ_INT, OBJ_DEC, OBJ_BOOL, OBJ_STRING,
    OBJ_RANGE, OBJ_LIST, OBJ_TABLE
};

const std::array<Constructors::Ctor,
Constructors::CtorType::NUM_CTORS> Constructors::ctors{
    Constructors::Obj, Constructors::Int, Constructors::Dec,
    Constructors::Bool, Constructors::String, Constructors::Range,
    Constructors::List, Constructors::Table
};

const std::array<const char*,
Constructors::CtorType::NUM_CTORS> Constructors::ctorNames{
    "Object", "Int", "Dec", "Bool", "String", "Range", "List",
    "Table"
};

const std::unordered_map<ObjType,
Constructors::CtorType> Constructors::builtins{
    {OBJ_VOID, Constructors::CTOR_OBJ},
    {OBJ_INT, Constructors::CTOR_INT},
    {OBJ_DEC, Constructors::CTOR_DEC},
    {OBJ_BOOL, Constructors::CTOR_BOOL},
    {OBJ_STRING, Constructors::CTOR_STRING},
    {OBJ_RANGE, Constructors::CTOR_RANGE},
    {OBJ_LIST, Constructors::CTOR_LIST},
    {OBJ_TABLE, Constructors::CTOR_TABLE}
};

Object Constructors::Obj(iter it, u8 args)
{
    (void) it;
    if (args != 0)
    {
        throw RuntimeError(ARITY_MISMATCH,
            CH_STR("expected 0 arguments but found {}", args)
        );
    }

    static Object obj{CH_ALLOC(Void)};
    return obj;
}

Object Constructors::Int(iter it, u8 args)
{
    if (args == 0)
        return Object{i64(0)};

    if (args == 1)
    {
        if (IS_INT(*it))
            return Object{AS_INT(*it)};
        else if (IS_STRING(*it))
        {
            const std::string& str{AS_STRING(*it)->str};
            i64 value{};
            auto answer{fast_float::from_chars(str.data(), str.data() + str.size(),
                value)};

            if (!answer)
                throw RuntimeError(NUMERIC_LIT_PARSE_FAIL);
            return Object{value};
        }
    }

    if (args == 2)
    {
        if (!IS_STRING(it[0]))
            throw RuntimeError(WRONG_ARG_TYPE, "first argument is not a string");
        if (!IS_INT(it[1]))
            throw RuntimeError(WRONG_ARG_TYPE, "second argument is not an integer");

        i64 base{AS_INT(it[1])};
        if ((base < 2) || (base > 36))
            throw RuntimeError(INVALID_NUM_BASE, "base must be >= 2 and <= 36");

        const std::string& str{AS_STRING(*it)->str};
        i64 value{};
        auto answer{fast_float::from_chars(str.data(), str.data() + str.size(),
            value, static_cast<int>(base))};

        if (!answer)
            throw RuntimeError(NUMERIC_LIT_PARSE_FAIL);
        return Object{value};
    }

    throw RuntimeError(ARITY_MISMATCH,
        CH_STR("expected at most 2 arguments but found {}", args)
    );
}

Object Constructors::Dec(iter it, u8 args)
{
    if (args == 0)
        return Object{0.0};

    if (args == 1)
    {
        if (IS_DEC(*it))
            return Object{AS_DEC(*it)};
        else if (IS_STRING(*it))
        {
            const std::string& str{AS_STRING(*it)->str};
            double value{};
            auto answer{fast_float::from_chars(str.data(), str.data() + str.size(),
                value)};

            if (!answer)
                throw RuntimeError(NUMERIC_LIT_PARSE_FAIL);
            return Object{value};
        }
    }

    throw RuntimeError(ARITY_MISMATCH,
        CH_STR("expected at most 1 argument but found {}", args)
    );
}

Object Constructors::Bool(iter it, u8 args)
{
    if (args > 1)
    {
        throw RuntimeError(ARITY_MISMATCH,
            CH_STR("expected at most 1 argument but found {}", args)
        );
    }

    if (args == 0)
        return Object{false};
    return Object{it->isTruthy()};
}

Object Constructors::String(iter it, u8 args)
{
    if (args > 1)
    {
        throw RuntimeError(ARITY_MISMATCH,
            CH_STR("expected at most 1 argument but found {}", args)
        );
    }

    if (args == 0)
        return Object{CH_ALLOC(::String, "")};
    return Object{CH_ALLOC(::String, it->printVal())};
}

Object Constructors::Range(iter it, u8 args)
{
    if ((args != 2) && (args != 3))
    {
        throw RuntimeError(ARITY_MISMATCH,
            CH_STR("expect 2 or 3 arguments but found {}", args)
        );
    }
    if (!IS_INT(it[0]) || !IS_INT(it[1]) || ((args == 3) && !IS_INT(it[2])))
        throw RuntimeError(WRONG_ARG_TYPE, "arguments must be integers");

    i64 start{AS_INT(it[0])};
    i64 stop{AS_INT(it[1])};

    // Step size 0 is allowed if start == stop (step size will never be used).
    if ((args == 3) && (AS_INT(it[2]) == 0) && (start != stop))
        throw RuntimeError(WRONG_ARG_TYPE, "cannot have a step size of zero");

    std::array nums{start, stop, ((stop >= start) ? i64(1) : i64(-1))};
    if (args == 3) nums[2] = AS_INT(it[2]);

    Range::validateRange(nums); // May throw on error.
    return Object{CH_ALLOC(::Range, nums)};
}

Object Constructors::List(iter it, u8 args)
{
    if (args > 1)
    {
        throw RuntimeError(ARITY_MISMATCH,
            CH_STR("expected at most 1 argument but found {}", args)
        );
    }

    ::List* list{CH_ALLOC(::List, DEFAULT_LIST_SIZE)};
    if (args == 1)
    {
        if (!IS_ITERABLE(*it))
            throw RuntimeError(WRONG_ARG_TYPE, "argument is not iterable");
        ObjIter* iter{it->makeIter()}; // Guaranteed to succeed.
        Object temp{};

        if (iter->start(temp))
        {
            list->array.push(temp);
            while (iter->next(temp))
                list->array.push(temp);
        }
    }

    return list;
}

Object Constructors::Table(iter it, u8 args)
{
    if (args > 1)
    {
        throw RuntimeError(ARITY_MISMATCH,
            CH_STR("expected at most 1 argument but found {}", args)
        );
    }

    ::Table* table{CH_ALLOC(::Table)};
    auto insertEntry = [table](const Object& obj) {
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
        Object temp{};

        if (iter->start(temp))
        {
            insertEntry(temp);
            while (iter->next(temp))
                insertEntry(temp);
        }
    }

    return table;
}