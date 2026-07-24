#pragma once
#include "common.h"
#include <climits>

#define ATTR_BASE_TYPE u8
using VarAttr = ATTR_BASE_TYPE;

#define ATTR_LIST           \
    X(PRIVATE, Private)     \
    X(STATIC, Static)       \
    X(COMPUTED, Computed)   \
    X(CLOSED, Closed)       \
    X(TEST, Test)

#define NUM_BITS        (sizeof(ATTR_BASE_TYPE) * CHAR_BIT)
#define POS_BIT(shift)  (1 << (NUM_BITS - (shift)))

enum DeclAttr : ATTR_BASE_TYPE
{
    #define X(name, _) ATTR_##name,

    ATTR_LIST
    NUM_ATTRS

    #undef X
};

#define X(NAME, name)                                           \
    [[nodiscard]] static inline bool is##name(VarAttr attr) {   \
        return ((attr & POS_BIT(ATTR_##NAME + 1)) != 0);        \
    }                                                           \
    static inline void mark##name(VarAttr& varAttr) {           \
        varAttr |= POS_BIT(ATTR_##NAME + 1);                    \
    }

ATTR_LIST

#undef X

#undef ATTR_BASE_TYPE
#undef ATTR_LIST
#undef NUM_BITS
#undef POS_BIT
#undef X