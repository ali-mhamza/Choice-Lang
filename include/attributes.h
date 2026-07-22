#pragma once
#include "common.h"
#include <climits>

using AttrBaseType = u8;
using VarAttr = AttrBaseType;

#define ATTR_LIST               \
    X(PRIVATE, Private, 1)      \
    X(STATIC, Static, 2)        \
    X(COMPUTED, Computed, 3)    \
    X(CLOSED, Closed, 4)        \
    X(TEST, Test, 5)

#define NUM_BITS(type)          (sizeof(type) * CHAR_BIT)
#define POS_BIT(type, shift)    (1 << (NUM_BITS(type) - shift))

enum DeclAttr : AttrBaseType
{
    #define X(name, _, shift) \
        ATTR_##name = AttrBaseType(POS_BIT(AttrBaseType, shift)),

    ATTR_LIST

    #undef X
};

#define X(_, name, pos)                                         \
    [[nodiscard]] static inline bool is##name(VarAttr attr) {   \
        return ((attr & POS_BIT(AttrBaseType, pos)) != 0);      \
    }

ATTR_LIST

#undef ATTR_LIST
#undef NUM_BITS
#undef POS_BIT
#undef X

static inline
void markAttribute(VarAttr& varAttr, DeclAttr declAttr)
{
    varAttr |= declAttr;
}