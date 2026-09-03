#include "../include/config.h"
#include "../include/core.h"
#include "../include/object.h"
#include "../include/utils.h"
#include <climits>

// Basic ISA check.
static_assert(CHAR_BIT == 8, "Incompatible ISA for interpreter.");

// Check that BUILTIN_GLOBALS constant matches correct number.
static_assert(BUILTIN_GLOBALS ==
    to_num(Core::Function::Count) + to_num(Core::Ctor::Count) + 1);

// Check that object type can fit within 5 bits (+ 3 bits for mutability
// flags = 1 byte).
static_assert(ObjType::Count <= static_cast<ObjType>(TYPE_MASK + 1),
    "Too many object types defined.");