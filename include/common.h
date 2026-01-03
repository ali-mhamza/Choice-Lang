#pragma once
#include <cstdint>
#include <string>
#include <vector>

#define VERSION_MAJOR   0
#define VERSION_MINOR   0
#define VERSION_PATCH   1

// #define TIME_TOTAL
#define TIME_RUN
#define WATCH_EXEC
#define WATCH_REG

#define FALLTHROUGH() [[fallthrough]]

#if defined(__cpp_lib_unreachable) // Check for C++23 support.
	#include <utility>
	#define UNREACHABLE() std::unreachable()
#elif defined(__GNUC__) || defined(__clang__)
	#define UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
	#define UNREACHABLE() __assume(false)
#endif

#if defined(__cpp_lib_print) && defined(__cpp_lib_format)
	#include <print>
	#define FORMAT_PRINT	std::print
	#define FORMAT_STR		std::format
#else
	#ifndef FMT_HEADER_ONLY
		#define FMT_HEADER_ONLY
	#endif
	#include <fmt/base.h>
	#include <fmt/core.h>
	#define FORMAT_PRINT	fmt::print
	#define FORMAT_STR		fmt::format
#endif

#ifndef NDEBUG
#include <cstdlib>
#define ASSERT(expr, msg) \
	do { \
		if (expr) \
			break; \
		else \
		{ \
			FORMAT_PRINT("ASSERTION FAILED [%s: %s, %d]: %s.\n", \
				(__FILE__), (__func__), (__LINE__), msg); \
			exit(EXIT_FAILURE); \
		} \
	} while (false)
#else
#define ASSERT(expr, msg)
#endif

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