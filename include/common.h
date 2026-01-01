#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#define COMP_AST
//#define TIME_TOTAL
#define TIME_RUN

#if defined(__cpp_lib_unreachable) // Check for C++23 support.
#include <utility>
#define UNREACHABLE() std::unreachable()
#elif defined(__GNUC__) || defined(__clang__)
#define UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
#define UNREACHABLE() __assume(false)
#endif

#define GETV(variant, type) std::get<type>(variant)

using i8        = int8_t;
using i16       = int16_t;
using i32       = int32_t;
using i64       = int64_t;

using ui8       = uint8_t;
using ui16      = uint16_t;
using ui32      = uint32_t;
using ui64      = uint64_t;

using Hash      = uint32_t;

class Token;
class Object;
using vT    	= std::vector<Token>;
using vByte 	= std::vector<ui8>;
using vObj  	= std::vector<Object>;

constexpr int TAB_SIZE = 4;
// Whether we're running an externally loaded
// program or not.
extern bool external;
extern std::string file;