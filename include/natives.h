#pragma once
#include "common.h"
#include "object.h"
#include "token.h"
#include "vm.h"
#include <functional>
#include <string_view>
#include <unordered_map>

namespace Natives
{
    // using iter = vObj::const_iterator;
    using iter = Object*;

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

    extern const std::function<void(iter, ui8,
        const Token&)> functions[NUM_FUNCS];
    extern const std::string_view funcNames[NUM_FUNCS];
    extern const std::unordered_map<std::string_view, FuncType> builtins;
}