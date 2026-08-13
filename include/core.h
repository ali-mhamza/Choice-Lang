#pragma once
#include "common.h"
#include "linear_alloc.h"
#include "object.h"
#include <array>
#include <string_view>
#include <type_traits>
#include <unordered_map>

// Core/built-in parts of the language.
// Includes built-in functions, constructors and type methods.

namespace Core
{
    using iter  = Object*;
    using sv    = std::string_view;

    namespace Callable
    {
        using Func      = void (*)(iter, u8);
        using Ctor      = Object (*)(iter, u8);
        using Method    = Object (*)(Object, iter, u8);
    };

    enum class Function : u8
    {
        Print,
        Println,
        Typeof,
        Len,
        Clock,
        Read,
        Quit,
        Count
    };

    enum class Ctor : u8
    {
        Object,
        Int,
        Dec,
        Bool,
        Text,
        String,
        Range,
        List,
        Table,
        Count
    };

    struct Method : public HeapObj
    {
        const sv name{};
        Object instance{};
        const Callable::Method callable{};

        Method(sv name, Callable::Method callable) :
            name{name}, callable{callable} {}

        bool operator==(const Method& other) const
        {
            return ((this->instance == other.instance)
                    && (this->callable == other.callable));
        }

        Object call(iter it, u8 argCount)
        {
            return callable(instance, it, argCount);
        }
    };

    namespace Functions
    {
        extern const std::array<sv, to_num(Function::Count)> names;
        extern const std::array<Callable::Func, to_num(Function::Count)> impls;
        extern const std::unordered_map<sv, Function> search;
    };

    namespace Ctors
    {
        extern const std::array<sv, to_num(Ctor::Count)> names;
        extern const std::array<Callable::Ctor, to_num(Ctor::Count)> impls;
        extern const std::unordered_map<ObjType, Ctor> search;
        extern const std::array<ObjType, to_num(Ctor::Count)> types;
    };

    namespace Methods
    {
        Object getMember(ObjType type, const std::string& name);
        Object getMember(const std::string& name);

        #define METHOD_TYPE_LIST                                            \
            X(Int, 0)   X(Dec, 0)   X(Bool, 0)  X(Text, 0)  X(String, 0)    \
            X(Range, 0) X(List, 0)  X(Table, 0)

        #define NAMESPACE(name, count)                                                  \
            namespace name                                                              \
            {                                                                           \
                constexpr u8 methodCount{count};                                        \
                extern const std::array<sv, methodCount> names;                         \
                extern const std::array<Callable::Method, methodCount> impls;           \
                extern const std::unordered_map<sv, u8> search;                         \
                inline Object getMember(const std::string& name)                        \
                {                                                                       \
                    auto it{search.find(name)};                                         \
                    if (it != search.end())                                             \
                        return CH_ALLOC(Method, names[it->second], impls[it->second]);  \
                    return Methods::getMember(name);                                    \
                }                                                                       \
            }

        #define X(name, count) NAMESPACE(name, count)

        METHOD_TYPE_LIST

        #undef X
        #undef NAMESPACE
    };
};