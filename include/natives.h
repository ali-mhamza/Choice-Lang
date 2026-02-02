#pragma once
#include "../include/common.h"
#include "../include/object.h"
#include "../include/token.h"
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
        NUM_FUNCS
    };

    Object print(iter it, ui8 args, const Token& error);
    Object type(iter it, ui8 args, const Token& error);
    Object clock(iter it, ui8 args, const Token& error);

    extern const std::function<Object(iter, ui8,
        const Token&)> functions[NUM_FUNCS];
    extern const std::string_view funcNames[NUM_FUNCS];
    extern const std::unordered_map<std::string_view, FuncType> builtins;
}