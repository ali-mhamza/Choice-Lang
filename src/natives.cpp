#include "../include/natives.h"
#include "../include/common.h"
#include "../include/error.h"
#include <chrono>

const std::function<Object(Natives::iter, ui8,
    const Token&)> Natives::functions[Natives::NUM_FUNCS] = {
        Natives::print, Natives::type, Natives::clock
};

const std::string_view Natives::funcNames[NUM_FUNCS] = {
    "print", "type", "clock"
};

const std::unordered_map<std::string_view,
    Natives::FuncType> Natives::builtins = {
    {"print", Natives::FUNC_PRINT},
    {"type", Natives::FUNC_TYPE},
    {"clock", Natives::FUNC_CLOCK}
};

Object Natives::print(Natives::iter it, ui8 args, const Token& error)
{
    (void) error;
    
    for (int i = 0; i < args; i++)
    {
        switch ((it + i)->type)
        {
            // Fast path printing.
            case OBJ_INT:   FORMAT_PRINT("{}", AS_INT(*(it + i)));      break;
            case OBJ_DEC:   FORMAT_PRINT("{:.6f}", AS_DEC(*(it + i)));  break;
            case OBJ_BOOL:  FORMAT_PRINT("{}", AS_BOOL(*(it + i)));     break;
            case OBJ_NULL:  FORMAT_PRINT("null");                       break;
            case OBJ_STRING:
            {
                HeapObj& temp = AS_HEAP_VAL(*(it + i));
                FORMAT_PRINT("{}", AS_STRING(&temp).str);
                break;
            }
            // Slower alternative.
            default:        FORMAT_PRINT("{}", (it + i)->printVal());
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

    HeapObj* str = new String(it->printType());
    return Object(str);
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