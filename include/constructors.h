#pragma once
#include "common.h"
#include "object.h"

class Object;

namespace Constructors
{
    using iter = Object*;
    using Ctor = Object (*)(iter, u8);

    enum CtorType : u8
    {
        CTOR_OBJ, // To make a plain Object.
        CTOR_INT,
        CTOR_DEC,
        CTOR_BOOL,
        CTOR_STRING,
        CTOR_RANGE,
        CTOR_LIST,
        CTOR_TABLE,
        NUM_CTORS
    };

    Object Obj(iter it, u8 args);
    Object Int(iter it, u8 args);
    Object Dec(iter it, u8 args);
    Object Bool(iter it, u8 args);
    Object String(iter it, u8 args);
    Object Range(iter it, u8 args);
    Object List(iter it, u8 args);
    Object Table(iter it, u8 args);

    extern const std::array<ObjType, NUM_CTORS> types;
    extern const std::array<Ctor, NUM_CTORS> ctors;
    extern const std::array<const char*, NUM_CTORS> ctorNames;
    extern const std::unordered_map<ObjType, CtorType> builtins;
}