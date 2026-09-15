#pragma once
#include <cstdint>
#include <limits>

using size_type = std::uint64_t;
constexpr size_type size_max{std::numeric_limits<size_type>::max()};

#if !defined(TABLE_LOAD_FACTOR)
    #define TABLE_LOAD_FACTOR 0.8
#endif