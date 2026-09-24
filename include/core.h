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
        print,
        println,
        typeof,
        len,
        clock,
        read,
        quit,
        getattr,
        setattr,
        binary,
        hex,
        members,
        random,
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
        const Callable::Method callable{};
        Object instance{};

        Method(const Object& instance, sv name, Callable::Method callable) :
            name{name}, callable{callable}, instance{instance} {}

        bool operator==(const Method& other) const
        {
            return ((this->instance == other.instance)
                    && (this->callable == other.callable));
        }

        Object call(iter it, u8 args)
        {
            return callable(instance, it, args);
        }
    };

    void checkArity(u8 min, u8 max, bool exclusive, u8 args);

    namespace Functions
    {
        extern const std::array<sv, to_num(Function::Count)> names;
        extern const std::array<Callable::Func, to_num(Function::Count)> impls;
        extern const HashTable<sv, Function> search;
    };

    namespace Ctors
    {
        extern const std::array<sv, to_num(Ctor::Count)> names;
        extern const std::array<Callable::Ctor, to_num(Ctor::Count)> impls;
        extern const HashTable<ObjType, Ctor> search;
        extern const std::array<ObjType, to_num(Ctor::Count)> types;
    };

    namespace Methods
    {
        extern const u8 methodCount;
        /* C-style arrays so we don't have to declare size here as well. */
        extern const sv names[];
        extern const Callable::Method impls[];
        extern const HashTable<sv, u8> search;

        Object getTypeMember(Object& obj, const std::string& name);
        Object getCommonMember(Object& obj, const std::string& name);

        #define METHOD_TYPE_LIST    \
            X(Dec)                  \
            X(Text)                 \
            X(String)               \
            X(Range)                \
            X(List)                 \
            X(Table)

        #define NAMESPACE(name)                                                     \
            namespace name                                                          \
            {                                                                       \
                extern const u8 methodCount;                                        \
                extern const sv names[];                                            \
                extern const Callable::Method impls[];                              \
                extern const HashTable<sv, u8> search;                              \
                inline Object getMember(Object& obj, const std::string& name)       \
                {                                                                   \
                    const u8* it{search.get(name)};                                 \
                    if (it != nullptr)                                              \
                        return CH_ALLOC_CORE_METHOD(obj, names[*it], impls[*it]);   \
                    return Methods::getCommonMember(obj, name);                     \
                }                                                                   \
            }

        #define X(name) NAMESPACE(name)

        METHOD_TYPE_LIST

        #undef X
        #undef NAMESPACE
    };
};