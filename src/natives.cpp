#include "../include/natives.h"

#include "format-inl.h"

#include "../include/common.h"
#include "../include/error.h"
#include "../include/linear_alloc.h"
#include "../include/object.h"
#include <array>
#include <chrono>
#include <cstdio> // For fflush.
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

const std::array<Natives::NativeFunc,
Natives::FuncType::NUM_FUNCS> Natives::functions = {
    Natives::print, Natives::println, Natives::format,
    Natives::type, Natives::len, Natives::clock,
    Natives::range, Natives::read
};

const std::array<const char*,
Natives::FuncType::NUM_FUNCS> Natives::funcNames = {
    "print", "println", "format", "type", "len",
    "clock", "range", "read"
};

const std::unordered_map<std::string_view,
    Natives::FuncType> Natives::builtins = {
    {"print", Natives::FUNC_PRINT},
    {"println", Natives::FUNC_PRINTLN},
    {"format", Natives::FUNC_FORMAT},
    {"type", Natives::FUNC_TYPE},
    {"len", Natives::FUNC_LEN},
    {"clock", Natives::FUNC_CLOCK},
    {"range", Natives::FUNC_RANGE},
    {"read", Natives::FUNC_READ}
};

void Natives::print(Natives::iter it, ui8 args, const Token& error)
{
    (void) error;

    for (ui8 i = 0; i < args; i++)
    {
        switch (it[i].type)
        {
            // Fast path printing.
            case OBJ_INT:   CH_PRINT("{}", AS_INT(it[i]));      break;
            case OBJ_DEC:   CH_PRINT("{:.6}", AS_DEC(it[i]));   break;
            case OBJ_BOOL:  CH_PRINT("{}", AS_BOOL(it[i]));     break;
            case OBJ_NULL:  CH_PRINT("null");                   break;
            case OBJ_STRING:
            {
                CH_PRINT("{}", AS_STRING(it[i])->str);
                break;
            }
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

    // To avoid reallocating the return value each time.
    static auto ret = Object(CH_ALLOC(Tuple));
    it[-1] = ret;
}

void Natives::println(Natives::iter it, ui8 args, const Token& error)
{
    print(it, args, error);
    if (!inRepl) CH_PRINT("\n");
}

#if !defined(CH_USE_FMT_LIB)
    // Work in progress.
    static std::string defaultFormat(Natives::iter it, ui8 args,
        const Token& error)
    {
        using sizeT = std::string::size_type;
        const std::string& str = AS_STRING(it[0])->str;
        sizeT size = str.size();
        ui8 count = 0;

        std::string newStr;
        if (size != 0)
        {
            newStr.reserve(str.size() + args - 1);

            sizeT pos = 0;
            sizeT start = pos;
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
                    if (++count > static_cast<ui8>(args - 1))
                        throw RuntimeError(error, "Too few format arguments.");

                    newStr += it[count].printVal();
                    pos += 2;
                }

                start = pos; // Mark our new start.
            }
        }

        if (count < static_cast<ui8>(args - 1))
            throw RuntimeError(error, "Too many format arguments.");
        if (newStr.empty()) newStr = str;

        return newStr;
    }
#endif

void Natives::format(Natives::iter it, ui8 args, const Token& error)
{
    if (args == 0)
        throw RuntimeError(error, "String argument not provided.");
    else if (!IS_STRING(it[0]))
        throw RuntimeError(error, "First argument must be a string.");

    std::string result;

    #if defined(CH_USE_FMT_LIB)
        fmt_store store;
        for (ui8 i = 1; i < args; i++)
            store.push_back(it[i].printVal());

        const std::string& str = AS_STRING(it[0])->str;
        try
        {
            result = fmt::vformat(str, store);
        }
        catch (std::runtime_error&) // Formatting error (too few arguments).
        {
            throw RuntimeError(error, "Too few format arguments.");
        }

        // TODO (possibly): detect if user passed too *many* arguments.
        // fmt does not throw any error in that case.
    #else
        result = defaultFormat(it, args, error);
    #endif

    it[-1] = Object(CH_ALLOC(String, result));
}

void Natives::type(Natives::iter it, ui8 args, const Token& error)
{
    if (args != 1)
    {
        throw RuntimeError(error,
            CH_STR("Expected 1 argument but found {}.", args)
        );
    }

    it[-1] = Object(it->type);
}

void Natives::len(Natives::iter it, ui8 args, const Token& error)
{
    if (args != 1)
    {
        throw RuntimeError(error,
            CH_STR("Expected 1 argument but found {}.", args)
        );
    }

    const Object& obj = *it;
    if (!IS_ITERABLE(obj))
        throw RuntimeError(error, "Argument provided is not iterable.");

    i64 len = 0;
    switch (obj.type)
    {
        case OBJ_STRING:
            len = AS_STRING(obj)->str.size();
            break;
        case OBJ_RANGE:
        {
            auto* range = AS_RANGE(obj);
            len = range->length();
            break;
        }
        case OBJ_LIST:
            len = AS_LIST(obj)->array.count();
            break;
        default:
            CH_UNREACHABLE();
    }

    it[-1] = Object(len);
}

void Natives::clock(Natives::iter it, ui8 args, const Token& error)
{   
    if (args != 0)
    {
        throw RuntimeError(error,
            CH_STR("Expected 0 arguments but found {}.", args)
        );
    }

    using clock = std::chrono::steady_clock;
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;
    static const auto start = clock::now();

    auto time = clock::now();
    auto ret = duration_cast<nanoseconds>(time - start);
    it[-1] = Object(i64(ret.count()));
}

void Natives::range(Natives::iter it, ui8 args, const Token& error)
{
    if ((args != 2) && (args != 3))
    {
        throw RuntimeError(error,
            CH_STR("Expect 2 or 3 arguments but found {}.", args)
        );
    }
    if (!IS_INT(it[0]) || !IS_INT(it[1]) || ((args == 3) && !IS_INT(it[2])))
        throw RuntimeError(error, "Arguments must be integers.");

    std::array<i64, 3> limits = {AS_INT(it[0]), AS_INT(it[1]), 1};
    if (args == 3)
        limits[2] = AS_INT(it[2]);
    it[-1] = Object(CH_ALLOC(Range, limits));
}

void Natives::read(Natives::iter it, ui8 args, const Token& error)
{
    if (args > 1)
    {
        throw RuntimeError(error,
            CH_STR("Expect 0 or 1 arguments but found {}.", args)
        );
    }
    if (args == 1)
    {
        if (!IS_STRING(it[0]))
            throw RuntimeError(error, "Argument must be a string."); // Temporarily.
        CH_PRINT("{}", AS_STRING(it[0])->str);
        fflush(stdout);
    }

    std::ios_base::sync_with_stdio(false);
    std::string input;
    std::getline(std::cin, input);
    it[-1] = Object(CH_ALLOC(String, input));
}