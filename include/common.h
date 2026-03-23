#pragma once
#include "config.h"
#include <cstdint>
#include <string>
#include <vector>

/* Macros. */

// Compiler and OS.

#if defined(__GNUC__)
	#define CH_COMPILER "g++"
#elif defined(__clang__)
	#define CH_COMPILER "Clang"
#elif defined(_MSC_VER)
	#define CH_COMPILER "MSVC"
#elif defined(__APPLE_CC__)
	#define CH_COMPILER "Apple Clang"
#else
	#define CH_COMPILER "unknown compiler"
#endif

#if defined(__linux__)
	#define CH_LOCAL_OS "Linux"
#elif defined(_WIN32) || defined(_WIN64)
	#define CH_LOCAL_OS "Windows"
#elif defined(__APPLE__) || defined(__MACH__)
	#define CH_LOCAL_OS "Apple"
#else
	#define CH_LOCAL_OS "unknown OS"
#endif

// Version number.

#define CH_VERSION_MAJOR	0
#define CH_VERSION_MINOR	0
#define CH_VERSION_PATCH	1

// Fallthrough.

#define CH_FALLTHROUGH() [[fallthrough]]

// Format printing and string-building.

#if defined(__cpp_lib_print) && defined(__cpp_lib_format)
	#include <format>
	#include <print>
	#define CH_PRINT	std::print
	#define CH_STR		std::format
#else
	#define CH_USE_FMT_LIB

	#ifndef FMT_HEADER_ONLY
		#define FMT_HEADER_ONLY
	#endif
	#include "args.h"
	#include "format.h"

	#define CH_PRINT	fmt::print
	#define CH_STR		fmt::format

	using fmt_store = fmt::dynamic_format_arg_store<
		fmt::format_context
	>;
#endif

// Goto usage.

#if defined(__GNUC__) || defined(__clang__)
	#define CH_COMPUTED_GOTO	1
#elif defined(_MSC_VER)
	#define CH_COMPUTED_GOTO	0
#endif

// Assert macro.

#if defined(DEBUG)
	#include <cstdlib>
	#define CH_ASSERT(expr, msg)										\
		do {															\
			if (expr)													\
				break;													\
			else														\
			{															\
				CH_PRINT("CH_ASSERTION FAILED [{}: {}, {}]: {}\n",		\
					(__FILE__), (__func__), (__LINE__), msg);			\
				exit(EXIT_FAILURE);										\
			}															\
		} while (false)
#else
	#define CH_ASSERT(expr, msg)
#endif

// Allocation approach and assertions.

#if CH_USE_ALLOC
	#if !defined(CH_ALLOC_SIZE)
		#define CH_ALLOC_SIZE MiB(10)
	#endif

	#define CH_ALLOC(type, ...) allocator.alloc<type, CustomDealloc<type>>(__VA_ARGS__)

	#define CH_ASSERT_MEM(expr, msg, arena)								\
		do {															\
			if (expr)													\
				break;													\
			else														\
			{															\
				CH_PRINT("ASSERTION FAILED [{}: {}, {}]: {}\n",			\
					(__FILE__), (__func__), (__LINE__), msg);			\
				free(arena);                                            \
				exit(EXIT_FAILURE);										\
			}															\
		} while (false)
#else
	#define CH_ALLOC(type, ...) new type(__VA_ARGS__)
#endif

// Unreachable points.

#if defined(DEBUG)
	#define CH_UNREACHABLE() CH_ASSERT(false, 	\
		"This point should not be reachable.")
#elif defined(NDEBUG)
	#if defined(__cpp_lib_unreachable) // Check for C++23 support.
		#include <utility>
		#define CH_UNREACHABLE()	std::unreachable()
	#elif defined(__GNUC__) || defined(__clang__)
		#define CH_UNREACHABLE()	__builtin_unreachable()
	#elif defined(_MSC_VER)
		#define CH_UNREACHABLE()	__assume(false)
	#endif
#else
	#define CH_UNREACHABLE()
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

#if CH_USE_ALLOC && defined(CH_LINEAR_ALLOC)
	class LinearAlloc;
	extern LinearAlloc allocator;
#endif