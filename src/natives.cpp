/*
 * Implementations of built-in, native functions, as well
 * as necessary data to print them out or resolve them.
 */

#include "../include/natives.h"
#include "../include/common.h"
#include "../include/error.h"
#include "../include/linear_alloc.h"
#include "../include/object.h"
#include <fmt/format-inl.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

const std::array<Natives::NativeFunc,
Natives::FuncType::NUM_FUNCS> Natives::functions{
    Natives::print, Natives::println, Natives::format,
    Natives::typeof, Natives::len, Natives::clock,
    Natives::range, Natives::read, Natives::quit
};

const std::array<const char*,
Natives::FuncType::NUM_FUNCS> Natives::funcNames{
    "print", "println", "format", "typeof", "len",
    "clock", "range", "read", "quit"
};

const std::unordered_map<std::string_view,
    Natives::FuncType> Natives::builtins{
    {"print", Natives::FUNC_PRINT},
    {"println", Natives::FUNC_PRINTLN},
    {"format", Natives::FUNC_FORMAT},
    {"typeof", Natives::FUNC_TYPEOF},
    {"len", Natives::FUNC_LEN},
    {"clock", Natives::FUNC_CLOCK},
    {"range", Natives::FUNC_RANGE},
    {"read", Natives::FUNC_READ},
    {"quit", Natives::FUNC_QUIT}
};

void Natives::print(iter it, u8 args)
{
    for (u8 i{0}; i < args; i++)
    {
        switch (it[i].type())
        {
            // Fast path printing.
            case OBJ_INT:       CH_PRINT("{}", AS_INT(it[i]));          break;
            case OBJ_BOOL:      CH_PRINT("{}", AS_BOOL(it[i]));         break;
            case OBJ_NULL:      CH_PRINT("null");                       break;
            case OBJ_STRING:    CH_PRINT("{}", AS_STRING(it[i])->str);  break;
            // Slower alternative.
            default: CH_PRINT("{}", it[i].printVal());
        }
        if (i != args - 1)
            CH_PRINT(" ");
    }

    if (inRepl)
        CH_PRINT("\n");
    else
        fflush(stdout);

    it[-1] = Object{OBJ_VOID};
}

void Natives::println(iter it, u8 args)
{
    print(it, args);
    if (!inRepl) CH_PRINT("\n");
}

#if !defined(CH_USE_FMT_LIB)
    // Work in progress.
    [[nodiscard]]
    static std::string defaultFormat(iter it, u8 args)
    {
        using sizeT = std::string::size_type;
        const std::string& str{AS_STRING(it[0])->str};
        sizeT size{str.size()};
        u8 count{0};

        std::string newStr{};
        if (size != 0)
        {
            newStr.reserve(str.size() + args - 1);

            sizeT pos{0};
            sizeT start{pos};
            while ((pos = str.find("{}", pos)) != std::string::npos)
            {
                if ((pos > 0) && (pos < size - 2)
                    && (str[pos - 1] == '{') && (str[pos + 2] == '}'))
                {
                    newStr.append(str, start, pos - start - 1);
                    newStr += "{}";
                    pos += 3;
                }
                else
                {
                    newStr.append(str, start, pos - start);
                    if (++count > static_cast<u8>(args - 1))
                        throw RuntimeError(ARITY_MISMATCH, "Too few format arguments.");

                    newStr += it[count].printVal();
                    pos += 2;
                }

                start = pos; // Mark our new start.
            }
        }

        if (count < static_cast<u8>(args - 1))
            throw RuntimeError(ARITY_MISMATCH, "Too many format arguments.");
        if (newStr.empty()) newStr = str;

        return newStr;
    }
#endif

void Natives::format(iter it, u8 args)
{
    if (args == 0)
        throw RuntimeError(WRONG_ARG_TYPE, "string argument not provided");
    else if (!IS_STRING(it[0]))
        throw RuntimeError(WRONG_ARG_TYPE, "first argument must be a string");

    std::string result{};

    #if defined(CH_USE_FMT_LIB)
        fmt_store store{};
        for (u8 i{1}; i < args; i++)
            store.push_back(it[i].printVal());

        const std::string& str{AS_STRING(it[0])->str};
        try
        {
            result = fmt::vformat(str, store);
        }
        catch (std::runtime_error& err) // Formatting error.
        {
            throw RuntimeError(FORMAT_STR_PROBLEM, err.what());
        }

    #else
        result = defaultFormat(it, args);
    #endif

    it[-1] = Object{CH_ALLOC(String, result)};
}

void Natives::typeof(iter it, u8 args)
{
    if (args != 1)
    {
        throw RuntimeError(ARITY_MISMATCH,
            CH_STR("expected 1 argument but found {}", args)
        );
    }

    // Use implicit conversion here to avoid the overload
    // which takes an ObjType enum argument.

    if (IS_INSTANCE(*it))
        it[-1] = AS_INSTANCE(*it)->type;
    else
        it[-1] = it->type();
}

void Natives::len(iter it, u8 args)
{
    if (args != 1)
    {
        throw RuntimeError(ARITY_MISMATCH,
            CH_STR("expected 1 argument but found {}", args)
        );
    }

    const Object& obj{*it};
    // Some may be collections but not iterable (for now), like tables.
    // Some may be iterable but not collections, like ranges.
    if (!IS_COLLECTION(obj) && !IS_ITERABLE(obj))
        throw RuntimeError(OBJ_NOT_ITERABLE, "argument provided is not iterable");

    i64 len{0};
    switch (obj.type())
    {
        case OBJ_STRING:
            len = AS_STRING(obj)->str.size();
            break;
        case OBJ_RANGE:
            len = AS_RANGE(obj)->length();
            break;
        case OBJ_LIST:
            len = AS_LIST(obj)->array.count();
            break;
        case OBJ_TABLE:
            len = AS_TABLE(obj)->table.size();
            break;
        default:
            CH_UNREACHABLE();
    }

    it[-1] = Object{len};
}

void Natives::clock(iter it, u8 args)
{
    if (args != 0)
    {
        throw RuntimeError(ARITY_MISMATCH,
            CH_STR("expected 0 arguments but found {}", args)
        );
    }

    using clock = std::chrono::steady_clock;
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;
    static const auto start{clock::now()};

    auto time{clock::now()};
    auto ret{duration_cast<nanoseconds>(time - start)};
    it[-1] = Object{i64(ret.count())};
}

void Natives::range(iter it, u8 args)
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
    it[-1] = Object{CH_ALLOC(Range, nums)};
}

void Natives::read(iter it, u8 args)
{
    if (args > 1)
    {
        throw RuntimeError(ARITY_MISMATCH,
            CH_STR("expect 0 or 1 arguments but found {}", args)
        );
    }
    if (args == 1)
    {
        if (!IS_STRING(it[0]))
            throw RuntimeError(WRONG_ARG_TYPE, "argument must be a string");
        CH_PRINT("{}", AS_STRING(it[0])->str);
        fflush(stdout);
    }

    std::ios_base::sync_with_stdio(false);
    std::string input{};
    std::getline(std::cin, input);
    it[-1] = Object{CH_ALLOC(String, input)};
}

void Natives::quit(iter it, u8 args)
{
    if (args > 1)
    {
        throw RuntimeError(ARITY_MISMATCH,
            CH_STR("expect 0 or 1 arguments but found {}", args)
        );
    }
    if ((args == 1) && !IS_INT(it[0]))
        throw RuntimeError(WRONG_ARG_TYPE, "argument must be an integer");

    i64 code{AS_INT(it[0])};
    if (code < 0)
        throw RuntimeError(WRONG_ARG_TYPE, "argument cannot be negative");

    u8 exitCode{static_cast<u8>((args == 0) ? 0 : (code & 0xff))};
    exit(exitCode);
    // No return value.
}