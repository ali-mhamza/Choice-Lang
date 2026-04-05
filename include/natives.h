#pragma once
#include "common.h"         // For ui8 in NativeFunc type alias.
#include "token.h"
#include <array>
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
        FUNC_PRINTLN,
        FUNC_FORMAT,
        FUNC_TYPE,
        FUNC_LEN,
        FUNC_CLOCK,
        FUNC_RANGE,
        FUNC_READ,
        FUNC_QUIT,
        NUM_FUNCS
    };

    void print(iter it, ui8 args, const Token& error);
    void println(iter it, ui8 args, const Token& error);
    void format(iter it, ui8 args, const Token& error);
    void type(iter it, ui8 args, const Token& error);
    void len(iter it, ui8 args, const Token& error);
    void clock(iter it, ui8 args, const Token& error);
    void range(iter it, ui8 args, const Token& error);
    void read(iter it, ui8 args, const Token& error);
    void quit(iter it, ui8 args, const Token& error);

    extern const std::array<NativeFunc, NUM_FUNCS> functions;
    extern const std::array<const char*, NUM_FUNCS> funcNames;
    extern const std::unordered_map<std::string_view, FuncType> builtins;
}