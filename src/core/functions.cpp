#include "../../include/core.h"
#include "../../include/error.h"
#include <limits>
#include <random>

#define FUNCTION_LIST   \
    X(print)            \
    X(println)          \
    X(typeof)           \
    X(len)              \
    X(clock)            \
    X(read)             \
    X(quit)             \
    X(getattr)          \
    X(setattr)          \
    X(binary)           \
    X(hex)              \
    X(members)          \
    X(random)

/* Forward declarations. */

#define FUNCTION(name) void name(iter it, u8 argCount);

namespace Core
{
    namespace Functions
    {
        #define X(name) FUNCTION(name)
        FUNCTION_LIST
        #undef X
    };
};

#undef FUNCTION


/* Utility arrays. */

const std::array<Core::sv, to_num(Core::Function::Count)>
Core::Functions::names{
    #define X(name) #name,
    FUNCTION_LIST
    #undef X
};

const std::array<Core::Callable::Func, to_num(Core::Function::Count)>
Core::Functions::impls{
    #define X(name) name,
    FUNCTION_LIST
    #undef X
};

const std::unordered_map<Core::sv, Core::Function>
Core::Functions::search{
    #define X(name) {#name, Function::name},
    FUNCTION_LIST
    #undef X
};


/* Implementations. */

#define FUNC(name) void Core::Functions::name(iter it, u8 args)

FUNC(print)
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

    it[-1] = Object::typed(ObjType::Void);
}

FUNC(println)
{
    print(it, args);
    if (!inRepl)
    {
        CH_PRINT("\n");
        fflush(stdout);
    }
}

FUNC(typeof)
{
    checkArity(1, 1, true, args);

    if (IS_INSTANCE(*it))
        it[-1] = AS_INSTANCE(*it)->type;
    else
        it[-1] = it->type();
}

FUNC(len)
{
    checkArity(1, 1, true, args);

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

FUNC(clock)
{
    checkArity(0, 0, true, args);

    using clock = std::chrono::steady_clock;
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;
    static const auto start{clock::now()};

    auto time{clock::now()};
    auto ret{duration_cast<nanoseconds>(time - start)};
    it[-1] = Object{i64(ret.count())};
}

FUNC(read)
{
    checkArity(0, 1, true, args);

    if (args == 1)
    {
        if (!IS_STRING_LIKE(it[0]))
            throw RuntimeError(WRONG_ARG_TYPE, "argument must be a string");
        CH_PRINT("{}", it->getObjectText());
        fflush(stdout);
    }

    std::string input{};
    std::getline(std::cin, input);
    it[-1] = Object{CH_ALLOC_STRING(input)};
}

FUNC(quit)
{
    checkArity(0, 1, true, args);

    if ((args == 1) && !IS_INT(it[0]))
        throw RuntimeError(WRONG_ARG_TYPE, "argument must be an integer");

    i64 code{AS_INT(it[0])};
    if (code < 0)
        throw RuntimeError(WRONG_ARG_TYPE, "argument cannot be negative");

    u8 exitCode{static_cast<u8>((args == 0) ? 0 : (code & 0xff))};
    exit(exitCode);
    // No return value.
}

FUNC(getattr)
{
    checkArity(2, 2, true, args);

    if (!IS_INSTANCE(it[0]))
        throw RuntimeError(WRONG_ARG_TYPE, "first argument must be a type instance");
    if (!IS_STRING_LIKE(it[1]))
        throw RuntimeError(WRONG_ARG_TYPE, "second argument must be string-like");

    std::string field{it[1].getObjectText()};
    it[-1] = AS_INSTANCE(it[0])->getField(field, nullptr);
}

FUNC(setattr)
{
    checkArity(3, 3, true, args);

    if (!IS_INSTANCE(it[0]))
        throw RuntimeError(WRONG_ARG_TYPE, "first argument must be a type instance");
    if (!IS_STRING_LIKE(it[1]))
        throw RuntimeError(WRONG_ARG_TYPE, "second argument must be string-like");

    std::string field{it[1].getObjectText()};
    AS_INSTANCE(it[0])->setField(field, it[2], nullptr);
    it[-1] = Object::typed(ObjType::Void);
}

FUNC(binary)
{
    checkArity(1, 1, true, args);

    if (!IS_INT(it[0]))
        throw RuntimeError(WRONG_ARG_TYPE, "argument must be an integer");

    std::string bin{CH_STR("{:#b}", AS_INT(it[0]))};
    it[-1] = CH_ALLOC_STRING(bin);
}

FUNC(hex)
{
    checkArity(1, 1, true, args);

    if (!IS_INT(it[0]))
        throw RuntimeError(WRONG_ARG_TYPE, "argument must be an integer");

    std::string hex{CH_STR("{:#x}", AS_INT(it[0]))};
    it[-1] = CH_ALLOC_STRING(hex);
}

static Object listFromMemberArray(const Core::sv members[], u8 count)
{
    List* list{CH_ALLOC_LIST(DEFAULT_LIST_SIZE)};
    for (u8 i{0}; i < count; i++)
        list->array.push(CH_ALLOC_TEXT(members[i]));
    return list;
}

static void appendCommonMembers(Object& obj)
{
    List* list{AS_LIST(obj)};
    for (u8 i{0}; i < Core::Methods::methodCount; i++)
        list->array.push(CH_ALLOC_TEXT(Core::Methods::names[i]));
}

FUNC(members)
{
    checkArity(1, 1, true, args);

    #define START_CHECK switch (it[0].type()) {
    #define X(type)                                 \
        case ObjType::type:                         \
        {                                           \
            it[-1] = listFromMemberArray(           \
                Core::Methods::type::names,         \
                Core::Methods::type::methodCount    \
            );                                      \
            break;                                  \
        }
    #define END_CHECK                                       \
            default:                                        \
                it[-1] = CH_ALLOC_LIST(DEFAULT_LIST_SIZE);  \
        }

    START_CHECK
        METHOD_TYPE_LIST
    END_CHECK

    appendCommonMembers(it[-1]);

    #undef START_CHECK
    #undef X
    #undef END_CHECK
}

FUNC(random)
{
    checkArity(0, 2, true, args);

    static std::mt19937_64 generator{};
    constexpr u64 intMax{static_cast<u64>(std::numeric_limits<i64>::max())};

    if (args == 0)
    {
        u64 value{generator()};
        if (value > intMax)
            it[-1] = static_cast<i64>(-1 * (value - intMax));
        else
            it[-1] = static_cast<i64>(value);
    }

    if (args == 2)
    {
        if (!IS_INT(it[0]) || !IS_INT(it[1]))
            throw RuntimeError(WRONG_ARG_TYPE, "range value must be integers");

        i64 min{AS_INT(it[0])}, max{AS_INT(it[1])};
        if (min >= max)
        {
            // TODO: Report error.
        }

        u64 value{generator()};
        it[-1] = static_cast<i64>(min + (value % (max - min + 1)));
    }
}

#undef FUNCTION_LIST