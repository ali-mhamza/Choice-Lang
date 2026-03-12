#include "../include/natives.h"
#include "../include/common.h"
#include "../include/error.h"
#include "../include/linear_alloc.h"
#include "../include/object.h"
#include <array>
#include <chrono>
#include <iostream>

const Natives::NativeFunc
Natives::functions[Natives::NUM_FUNCS] = {
    Natives::print, Natives::type, Natives::clock,
    Natives::range, Natives::read
};

const char* Natives::funcNames[NUM_FUNCS] = {
    "print", "type", "clock", "range", "read"
};

const std::unordered_map<std::string_view,
    Natives::FuncType> Natives::builtins = {
    {"print", Natives::FUNC_PRINT},
    {"type", Natives::FUNC_TYPE},
    {"clock", Natives::FUNC_CLOCK},
    {"range", Natives::FUNC_RANGE},
    {"read", Natives::FUNC_READ}
};

void Natives::print(Natives::iter it, ui8 args, const Token& error)
{
    (void) error;
    // To avoid reallocating the return value each time.
    static auto ret = Object(ALLOC(Tuple, ObjDealloc<Tuple>));

    for (ui8 i = 0; i < args; i++)
    {
        switch (it[i].type)
        {
            // Fast path printing.
            case OBJ_INT:   FORMAT_PRINT("{}", AS_(int, it[i]));        break;
            case OBJ_DEC:   FORMAT_PRINT("{:.6f}", AS_(dec, it[i]));    break;
            case OBJ_BOOL:  FORMAT_PRINT("{}", AS_(bool, it[i]));       break;
            case OBJ_NULL:  FORMAT_PRINT("null");                       break;
            case OBJ_STRING:
            {
                FORMAT_PRINT("{}", AS_(string, it[i])->str);
                break;
            }
            // Slower alternative.
            default: FORMAT_PRINT("{}", it[i].printVal());
        }
        if (i != args - 1)
            FORMAT_PRINT(" ");
    }
    FORMAT_PRINT("\n");

    it[-1] = ret;
}

void Natives::type(Natives::iter it, ui8 args, const Token& error)
{
    if (args != 1)
        throw RuntimeError(error,
            FORMAT_STR("Expected 1 argument but found {}.", args)
        );

    it[-1] = Object(it->type);
}

void Natives::clock(Natives::iter it, ui8 args, const Token& error)
{   
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
    it[-1] = Object(i64(ret.count()));
}

void Natives::range(Natives::iter it, ui8 args, const Token& error)
{
    if ((args != 2) && (args != 3))
        throw RuntimeError(error,
            FORMAT_STR("Expect 2 or 3 arguments but found {}.", args)
        );
    if (!IS_(INT, it[0]) || !IS_(INT, it[1]) || ((args == 3) && !IS_(INT, it[2])))
        throw RuntimeError(error, "Arguments must be integers.");

    std::array<i64, 3> limits = {AS_(int, it[0]), AS_(int, it[1]), 1};
    if (args == 3)
        limits[2] = AS_(int, it[2]);
    it[-1] = Object(ALLOC(Range, ObjDealloc<Range>, limits));
}

void Natives::read(Natives::iter it, ui8 args, const Token& error)
{
    if (args > 1)
        throw RuntimeError(error,
            FORMAT_STR("Expect 0 or 1 arguments but found {}.", args)
        );
    if (args == 1)
    {
        if (!IS_(STRING, it[0]))
            throw RuntimeError(error, "Argument must be a string."); // Temporarily.
        FORMAT_PRINT("{}", AS_(string, it[0])->str);
    }

    std::ios_base::sync_with_stdio(false);
    std::string input;
    std::getline(std::cin, input);
    it[-1] = Object(ALLOC(String, ObjDealloc<String>, input));
}