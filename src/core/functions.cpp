#include "../../include/core.h"
#include "../../include/error.h"

/* Forward declarations. */

#define FUNCTION(name) void name(iter it, u8 argCount);

namespace Core
{
    namespace Functions
    {
        FUNCTION(print);
        FUNCTION(println);
        FUNCTION(typeof);
        FUNCTION(len);
        FUNCTION(clock);
        FUNCTION(read);
        FUNCTION(quit);
        FUNCTION(getattr);
        FUNCTION(setattr);
    };
};

#undef FUNCTION


/* Utility arrays. */

const std::array<Core::sv, to_num(Core::Function::Count)>
Core::Functions::names{
    "print",
    "println",
    "typeof",
    "len",
    "clock",
    "read",
    "quit",
    "getattr",
    "setattr"
};

const std::array<Core::Callable::Func, to_num(Core::Function::Count)>
Core::Functions::impls{
    print,
    println,
    typeof,
    len,
    clock,
    read,
    quit,
    getattr,
    setattr
};

const std::unordered_map<Core::sv, Core::Function>
Core::Functions::search{
    {"print",   Function::print},
    {"println", Function::println},
    {"typeof",  Function::typeof},
    {"len",     Function::len},
    {"clock",   Function::clock},
    {"read",    Function::read},
    {"quit",    Function::quit},
    {"getattr", Function::getattr},
    {"setattr", Function::setattr}
};


/* Implementations. */

void Core::Functions::print(iter it, u8 args)
{
    for (u8 i{0}; i < args; i++)
    {
        switch (it[i].type())
        {
            // Fast path printing.
            case ObjType::Int:      CH_PRINT("{}", AS_INT(it[i]));          break;
            case ObjType::Bool:     CH_PRINT("{}", AS_BOOL(it[i]));         break;
            case ObjType::Null:     CH_PRINT("null");                       break;
            case ObjType::Text:
            case ObjType::String:   CH_PRINT("{}", it[i].getObjectText());  break;
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

    it[-1] = Object{ObjType::Void};
}

void Core::Functions::println(iter it, u8 args)
{
    print(it, args);
    if (!inRepl)
    {
        CH_PRINT("\n");
        fflush(stdout);
    }
}

void Core::Functions::typeof(iter it, u8 args)
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

void Core::Functions::len(iter it, u8 args)
{
    if (args != 1)
    {
        throw RuntimeError(ARITY_MISMATCH,
            CH_STR("expected 1 argument but found {}", args)
        );
    }

    const Object& obj{*it};
    // Collection == iterable, in this context.
    if (!IS_COLLECTION(obj))
        throw RuntimeError(OBJ_NOT_ITERABLE, "argument provided is not iterable");

    i64 len{0};
    switch (obj.type())
    {
        case ObjType::Text:
            len = AS_TEXT(obj)->len;
            break;
        case ObjType::String:
            len = AS_STRING(obj)->str.size();
            break;
        case ObjType::Range:
            len = AS_RANGE(obj)->length();
            break;
        case ObjType::List:
            len = AS_LIST(obj)->array.count();
            break;
        case ObjType::Table:
            len = AS_TABLE(obj)->table.size();
            break;
        default:
            CH_UNREACHABLE();
    }

    it[-1] = Object{len};
}

void Core::Functions::clock(iter it, u8 args)
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

void Core::Functions::read(iter it, u8 args)
{
    if (args > 1)
    {
        throw RuntimeError(ARITY_MISMATCH,
            CH_STR("expect 0 or 1 arguments but found {}", args)
        );
    }
    if (args == 1)
    {
        if (!IS_STRING_LIKE(it[0]))
            throw RuntimeError(WRONG_ARG_TYPE, "argument must be a string");
        CH_PRINT("{}", it->getObjectText());
        fflush(stdout);
    }

    std::ios_base::sync_with_stdio(false);
    std::string input{};
    std::getline(std::cin, input);
    it[-1] = Object{CH_ALLOC(String, input)};
}

void Core::Functions::quit(iter it, u8 args)
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

void Core::Functions::getattr(iter it, u8 args)
{
    if (args != 2)
    {
        throw RuntimeError(ARITY_MISMATCH,
            CH_STR("expect 2 arguments but found {}", args)
        );
    }

    if (!IS_INSTANCE(it[0]))
        throw RuntimeError(WRONG_ARG_TYPE, "first argument must be a type instance");
    if (!IS_STRING_LIKE(it[1]))
        throw RuntimeError(WRONG_ARG_TYPE, "second argument must be string-like");

    std::string field{it[1].getObjectText()};
    it[-1] = AS_INSTANCE(it[0])->getField(field, nullptr);
}

void Core::Functions::setattr(iter it, u8 args)
{
    if (args != 3)
    {
        throw RuntimeError(ARITY_MISMATCH,
            CH_STR("expect 3 arguments but found {}", args)
        );
    }

    if (!IS_INSTANCE(it[0]))
        throw RuntimeError(WRONG_ARG_TYPE, "first argument must be a type instance");
    if (!IS_STRING_LIKE(it[1]))
        throw RuntimeError(WRONG_ARG_TYPE, "second argument must be string-like");

    std::string field{it[1].getObjectText()};
    AS_INSTANCE(it[0])->setField(field, it[2], nullptr);
    it[-1] = Object{ObjType::Void};
}