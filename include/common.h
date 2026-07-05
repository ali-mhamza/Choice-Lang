/*
 * Common macros, type aliases, and global variables used
 * throughout the interpreter.
 */

#pragma once
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

#define CH_VERSION \
	(CH_VERSION_MAJOR * 100 + CH_VERSION_MINOR * 10 + CH_VERSION_PATCH)

// Fallthrough.

#define CH_FALLTHROUGH() [[fallthrough]]

// Likely and unlikely branches.

#if __cplusplus >= 202002L
    #define CH_LIKELY(x)	(x) [[likely]]
    #define CH_UNLIKELY(x)	(x) [[unlikely]]
#elif defined(__GNUC__) || defined(__clang__)
    #define CH_LIKELY(x)	(__builtin_expect(!!(x), 1))
    #define CH_UNLIKELY(x)	(__builtin_expect(!!(x), 0))
#else
    #define CH_LIKELY(x)	(x)
    #define CH_UNLIKELY(x)	(x)
#endif

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
	#include <fmt/args.h>
	#include <fmt/format.h>

	#define CH_PRINT	fmt::print
	#define CH_STR		fmt::format

	using fmt_store = fmt::dynamic_format_arg_store<
		fmt::format_context
	>;
#endif

// Constructor name.

#define CH_CONSTRUCTOR "Self"

// Opcode appearance.

// Strip the "OP_" prefix when printing opcodes.
#define OPCODE_STRIP_PREFIX 1

// Inserting quote-marks.

#define CH_QUOTE_MARK "'"
#define CH_QUOTED(expr) CH_STR("{}{}{}", CH_QUOTE_MARK, (expr), CH_QUOTE_MARK)

// Goto usage.

#if defined(__GNUC__) || defined(__clang__)
	#define CH_COMPUTED_GOTO	1
#else
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
				CH_PRINT("ASSERTION FAILED [{}: {}, {}]: {}\n",			\
					(__FILE__), (__func__), (__LINE__), msg);			\
				exit(EXIT_FAILURE);										\
			}															\
		} while (false)
#elif defined(NDEBUG)
	#if __cplusplus >= 202302L
		#define CH_ASSERT(expr, msg) [[assume(expr)]]
	// Check Clang first, since __GNUC__ may be defined on
	// Clang despite it not supporting the GNU version of this
	// attribute.
	#elif defined(__clang__)
		#define CH_ASSERT(expr, msg) __builtin_assume(expr)
	#elif defined(__GNUC__)
		#define CH_ASSERT(expr, msg) __attribute__((assume(expr)))
	#elif defined(_MSC_VER)
		#define CH_ASSERT(expr, msg) __assume(expr)
	#else
		#define CH_ASSERT(expr, msg)
	#endif
#else
	#define CH_ASSERT(expr, msg)
#endif

// Allocation approach and assertions.

#if CH_USE_ALLOC
	#include "gen_alloc.h"

	#if !defined(CH_ALLOC_SIZE)
		#define CH_ALLOC_SIZE MiB(10)
	#endif

	#define CH_ALLOC(type, ...) allocator.alloc<type, CustomDealloc<type>>(__VA_ARGS__)
	#define CH_DEALLOC(ptr)

	#if defined(DEBUG)
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
		#define CH_ASSERT_MEM(expr, msg, arena)
	#endif /* defined(DEBUG) */
#else
	#define CH_ALLOC(type, ...) new type{__VA_ARGS__}
	#define CH_DEALLOC(ptr) delete ptr
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

using i8    = std::int8_t;
using i16   = std::int16_t;
using i32   = std::int32_t;
using i64   = std::int64_t;

using u8    = std::uint8_t;
using u16   = std::uint16_t;
using u32   = std::uint32_t;
using u64   = std::uint64_t;

using Hash  	= u32;
using FileID    = u16;

struct Token;
class Object;
using vT    = std::vector<Token>;
using vByte = std::vector<u8>;
using vObj  = std::vector<Object>;
using vBit  = vByte::const_iterator;

/* Global variables. */

class SourceManager;
extern SourceManager sourceManager;

class DiagnosticEngine;
extern DiagnosticEngine diagEngine;

// Where debug info (if any) is to be stored when caching bytecode.
enum DebugInfoState : u8;
extern DebugInfoState debugInfoState;
// Whether or not we are in the REPL or executing
// a given file.
extern bool inRepl;

#if CH_USE_ALLOC && defined(CH_LINEAR_ALLOC)
	class LinearAlloc;
	extern LinearAlloc allocator;
#endif