#pragma once
#include "config.h"
#include <cstdint>
#include <string>
#include <vector>

/* Macros. */

// Compiler and OS.

#if defined(__GNUC__)
	#define COMPILER "g++"
#elif defined(__clang__)
	#define COMPILER "Clang"
#elif defined(_MSC_VER)
	#define COMPILER "MSVC"
#elif defined(__APPLE_CC__)
	#define COMPILER "Apple Clang"
#else
	#define COMPILER "unknown compiler"
#endif

#if defined(__linux__)
	#define LOCAL_OS "Linux"
#elif defined(_WIN32) || defined(_WIN64)
	#define LOCAL_OS "Windows"
#elif defined(__APPLE__) || defined(__MACH__)
	#define LOCAL_OS "Apple"
#else
	#define LOCAL_OS "unknown OS"
#endif

// Version number.

#define VERSION_MAJOR   0
#define VERSION_MINOR   0
#define VERSION_PATCH   1

// Fallthrough.

#define FALLTHROUGH() [[fallthrough]]

// Format printing and string-building.

#if defined(__cpp_lib_print) && defined(__cpp_lib_format)
	#include <format>
	#include <print>
	#define FORMAT_PRINT	std::print
	#define FORMAT_STR		std::format
#else
	#ifndef FMT_HEADER_ONLY
		#define FMT_HEADER_ONLY
	#endif
	#include "format.h" // From libfmt.
	#define FORMAT_PRINT	fmt::print
	#define FORMAT_STR		fmt::format
#endif

// Goto usage.

#if defined(__GNUC__) || defined(__clang__)
	#define COMPUTED_GOTO 1
#elif defined(_MSC_VER)
	#define COMPUTED_GOTO 0
#endif

// Assert macro.

#if defined(DEBUG)
	#include <cstdlib>
	#define ASSERT(expr, msg)											\
		do {															\
			if (expr)													\
				break;													\
			else														\
			{															\
				FORMAT_PRINT("ASSERTION FAILED [{}: {}, {}]: {}\n",		\
					(__FILE__), (__func__), (__LINE__), msg);			\
				exit(EXIT_FAILURE);										\
			}															\
		} while (false)
#else
	#define ASSERT(expr, msg)
#endif

// Allocation approach and assertions.

#if USE_ALLOC
	#if !defined(ALLOC_SIZE)
		#define ALLOC_SIZE MiB(10)
	#endif

	#define ALLOC(type, ...) allocator.alloc<type, CustomDealloc<type>>(__VA_ARGS__)

	#define ASSERT_MEM(expr, msg, arena)								\
		do {															\
			if (expr)													\
				break;													\
			else														\
			{															\
				FORMAT_PRINT("ASSERTION FAILED [{}: {}, {}]: {}\n",		\
					(__FILE__), (__func__), (__LINE__), msg);			\
				free(arena);                                            \
				exit(EXIT_FAILURE);										\
			}															\
		} while (false)
#else
	#define ALLOC(type, ...) new type(__VA_ARGS__)
#endif

// Unreachable points.

#if defined(DEBUG)
	#define UNREACHABLE() ASSERT(false, 		\
		"This point should not be reachable.")
#elif defined(NDEBUG)
	#if defined(__cpp_lib_unreachable) // Check for C++23 support.
		#include <utility>
		#define UNREACHABLE() std::unreachable()
	#elif defined(__GNUC__) || defined(__clang__)
		#define UNREACHABLE() __builtin_unreachable()
	#elif defined(_MSC_VER)
		#define UNREACHABLE() __assume(false)
	#endif
#else
	#define UNREACHABLE()
#endif

/* Type aliases. */

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
using vBit		= vByte::const_iterator;

/* Global variables. */

// Whether we're running an externally loaded
// program or not.
extern bool external;
extern bool inRepl;
extern std::string file;

#if USE_ALLOC && defined(LINEAR_ALLOC)
	class LinearAlloc;
	extern LinearAlloc allocator;
#endif