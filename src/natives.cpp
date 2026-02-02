#include "../include/natives.h"
#include "../include/common.h"
#include "../include/error.h"
#include <array>
#include <chrono>

const std::function<Object(Natives::iter, ui8,
    const Token&)> Natives::functions[Natives::NUM_FUNCS] = {
        Natives::print, Natives::type, Natives::clock,
        Natives::range
};

const std::string_view Natives::funcNames[NUM_FUNCS] = {
    "print", "type", "clock", "range"
};

const std::unordered_map<std::string_view,
    Natives::FuncType> Natives::builtins = {
    {"print", Natives::FUNC_PRINT},
    {"type", Natives::FUNC_TYPE},
    {"clock", Natives::FUNC_CLOCK},
    {"range", Natives::FUNC_RANGE}
};

Object Natives::print(Natives::iter it, ui8 args, const Token& error)
{
    (void) error;
    
    for (int i = 0; i < args; i++)
    {
        switch (it[i].type)
        {
            // Fast path printing.
            case OBJ_INT:   FORMAT_PRINT("{}", AS_INT(it[i]));      break;
            case OBJ_DEC:   FORMAT_PRINT("{:.6f}", AS_DEC(it[i]));  break;
            case OBJ_BOOL:  FORMAT_PRINT("{}", AS_BOOL(it[i]));     break;
            case OBJ_NULL:  FORMAT_PRINT("null");                   break;
            case OBJ_STRING:
            {
                FORMAT_PRINT("{}", AS_STRING(*(it + i)).str);
                break;
            }
            // Slower alternative.
            default: FORMAT_PRINT("{}", it[i].printVal());
        }
        if (i != args - 1)
            FORMAT_PRINT(" ");
    }
    FORMAT_PRINT("\n");

    return Object();
}

Object Natives::type(Natives::iter it, ui8 args, const Token& error)
{
    if (args != 1)
        throw RuntimeError(error,
            FORMAT_STR("Expected 1 argument but found {}.", args)
        );

    return Object(new String(it->printType()));
}

Object Natives::clock(Natives::iter it, ui8 args, const Token& error)
{
    (void) it;

    if (args != 0)
        throw RuntimeError(error,
            FORMAT_STR("Expected 0 arguments but found {}.", args)
        );

    using clock = std::chrono::steady_clock;
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;
    static const auto start = clock::now();

    auto time = clock::now();
    auto ret = duration_cast<nanoseconds>(time - start);
    return Object(i64(ret.count()));
}

Object Natives::range(Natives::iter it, ui8 args, const Token& error)
{
    if ((args != 2) && (args != 3))
        throw RuntimeError(error,
            FORMAT_STR("Expect 2 or 3 arguments but found {}.", args)
        );
    if (!IS_INT(it[0]) || !IS_INT(it[1]) || ((args == 3) && !IS_INT(it[2])))
        throw RuntimeError(error,
            FORMAT_STR("Arguments must be integers.")
        );

    std::array<i64, 3> limits = {AS_INT(*it), AS_INT(*(it + 1)), 1};
    if (args == 3)
        limits[2] = AS_INT(*(it + 2));
    return Object(new Range(limits));
}