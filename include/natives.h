#pragma once
#include "common.h"
#include "token.h"
#include <functional>
#include <string_view>
#include <unordered_map>

class Object;

namespace Natives
{
    // using iter = vObj::const_iterator;
    using iter = Object*;
    using NativeFunc = void (*)(iter, ui8, const Token&);

    enum FuncType
    {
        FUNC_PRINT,
        FUNC_TYPE,
        FUNC_CLOCK,
        FUNC_RANGE,
        FUNC_READ,
        NUM_FUNCS
    };

    void print(iter it, ui8 args, const Token& error);
    void type(iter it, ui8 args, const Token& error);
    void clock(iter it, ui8 args, const Token& error);
    void range(iter it, ui8 args, const Token& error);
    void read(iter it, ui8 args, const Token& error);

    extern const NativeFunc functions[NUM_FUNCS];
    extern const std::string_view funcNames[NUM_FUNCS];
    extern const std::unordered_map<std::string_view, FuncType> builtins;
}