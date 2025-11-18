#include "../include/object.h"
#include <type_traits>
using namespace Object;

// Base.

Base::Base() :
    type(OBJ_INVALID), size(0) {}

Base::Base(ObjType type, size_t size) :
    type(type), size(size) {}

// Int.

template<typename SizeInt>
Int<SizeInt>::Int() :
    Base(OBJ_INT, sizeof(SizeInt)), value(0) {}

template<typename SizeInt>
Int<SizeInt>::Int(SizeInt value) :
    Base(OBJ_INT, sizeof(SizeInt)), value(value) {}

// UInt.
template<typename SizeUInt>
UInt<SizeUInt>::UInt() :
    Base(OBJ_INT, sizeof(SizeUInt)), value(0) {}

template<typename SizeUInt>
UInt<SizeUInt>::UInt(SizeUInt value) :
    Base(OBJ_UINT, sizeof(SizeUInt)), value(value) {}

// Dec.

template<typename SizeDec>
Dec<SizeDec>::Dec() :
    Base(OBJ_DEC, sizeof(SizeDec)), value(0.0) {}

// template<typename SizeDec>
// Dec<SizeDec>::Dec() :
//     Base(OBJ_DEC, sizeof(SizeDec))
// {
//     if constexpr (std::is_same_v<SizeDec, float>)
//         value = 0.0f;
//     else if constexpr (std::is_same_v<SizeDec, double>)
//         value = 0.0;
// }

template<typename SizeDec>
Dec<SizeDec>::Dec(SizeDec value) :
    Base(OBJ_DEC, sizeof(SizeDec)), value(value) {}

// Bool.

Bool::Bool(bool value) :
    Base(OBJ_BOOL, sizeof(bool)), value(value) {}

// String.

String::String() :
    Base(OBJ_STRING, sizeof(std::string)), value("") {} // Default initialize to empty string.

String::String(std::string& value) :
    Base(OBJ_STRING, sizeof(std::string)), value(value) {}

// Null.
Null::Null() :
    Base(OBJ_NULL, sizeof(Base)) {}