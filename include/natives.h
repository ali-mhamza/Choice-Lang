#pragma once
#include "common.h"
#include "token.h"
#include <array>
#include <string_view>
#include <unordered_map>

class Object;

namespace Natives
{
    using iter = Object*;
    using NativeFunc = void (*)(iter, u8);

    enum FuncType : u8
    {
        FUNC_PRINT,
        FUNC_PRINTLN,
        FUNC_FORMAT,
        FUNC_TYPEOF,
        FUNC_LEN,
        FUNC_CLOCK,
        FUNC_RANGE,
        FUNC_READ,
        FUNC_QUIT,
        NUM_FUNCS
    };

    void print(iter it, u8 args);
    void println(iter it, u8 args);
    void format(iter it, u8 args);
    void typeof(iter it, u8 args);
    void len(iter it, u8 args);
    void clock(iter it, u8 args);
    void range(iter it, u8 args);
    void read(iter it, u8 args);
    void quit(iter it, u8 args);

    extern const std::array<NativeFunc, NUM_FUNCS> functions;
    extern const std::array<const char*, NUM_FUNCS> funcNames;
    extern const std::unordered_map<std::string_view, FuncType> builtins;
}