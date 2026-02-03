#pragma once
#include "config.h"
#include <cstdint>
#include <string>
#include <vector>

#define VERSION_MAJOR   0
#define VERSION_MINOR   0
#define VERSION_PATCH   1

#define FALLTHROUGH() [[fallthrough]]

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

#if defined(__GNUC__) || defined(__clang__)
	#define COMPUTED_GOTO 1
#elif defined(_MSC_VER)
	#define COMPUTED_GOTO 0
#endif

#if defined(DEBUG)
	#include <cstdlib>
	#define ASSERT(expr, msg)											\
		do {															\
			if (expr)													\
				break;													\
			else														\
			{															\
				FORMAT_PRINT("ASSERTION FAILED [{}: {}, {}]: {}.\n",	\
					(__FILE__), (__func__), (__LINE__), msg);			\
				exit(EXIT_FAILURE);										\
			}															\
		} while (false)
#else
	#define ASSERT(expr, msg)
#endif

#ifdef USE_ALLOC
	#define ALLOC(type, dealloc, ...) allocator.alloc<type, dealloc>(__VA_ARGS__)
#else
	#define ALLOC(type, dealloc, ...) new type(__VA_ARGS__)
#endif

#define ASSERT_MEM(expr, msg, arena)								\
    do {															\
        if (expr)													\
            break;													\
        else														\
        {															\
            FORMAT_PRINT("ASSERTION FAILED [{}: {}, {}]: {}.\n",    \
                (__FILE__), (__func__), (__LINE__), msg);			\
            free(arena);                                            \
            exit(EXIT_FAILURE);										\
        }															\
    } while (false)

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

// Whether we're running an externally loaded
// program or not.
extern bool external;
extern std::string file;

#ifdef LINEAR_ALLOC
	class LinearAlloc;
	extern LinearAlloc allocator;
#endif