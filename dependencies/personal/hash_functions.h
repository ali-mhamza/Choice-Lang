#pragma once
#include "common.h"
#include <climits>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

using Hash = std::uint32_t;

template<typename Key>
inline Hash hashKey(const Key& key, size_type size = size_max);
template<typename T>
inline Hash hashNumeric(T key);
// We pass a reference instead of the actual pointer since
// function pointers cannot (generally) be coerced into const T*
// pointers.
template<typename T>
inline Hash hashPointer(const T& ptr);
inline Hash hashChar(char key);
inline Hash hashString(std::string_view string);
inline Hash hashCStr(const char* string, size_type length = size_max);

template<typename Key>
struct Hasher
{
    Hash operator()(const Key& key)
    {
        return hashKey(key);
    }

    Hash operator()(const Key& key, size_type size)
    {
        return hashKey(key, size);
    }
};

template<typename Key>
inline Hash hashKey(const Key& key, size_type size)
{
    if constexpr (std::is_pointer_v<Key>)
        return hashPointer(key);
    if constexpr (std::is_arithmetic_v<Key>)
        return hashNumeric(key);
    if constexpr (std::is_same_v<Key, char>)
        return hashChar(key);
    if constexpr (std::is_same_v<Key, std::string_view>)
        return hashString(key);
    if constexpr (std::is_same_v<Key, std::string>)
        return hashString(key);
    if constexpr (std::is_same_v<Key, const char *>)
        return hashCStr(key);
    if constexpr (std::is_same_v<Key, char *>)
        return hashCStr(key, size);
    return 0; // Error return.
}

// Using Jenkins' one-at-a-time function.
inline Hash hashBytes(const std::uint8_t* bytes, size_type size)
{
    Hash hash{0};

    for (size_type i{0}; i < size; i++)
    {
        hash += bytes[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

template<typename T>
inline Hash hashPointer(const T& ptr)
{
    const auto* temp{reinterpret_cast<const std::uint8_t*>(&ptr)};
    return hashBytes(temp, sizeof(T));
}

template<typename T>
inline Hash hashNumeric(T key)
{
    const auto* bytes{reinterpret_cast<const std::uint8_t*>(&key)};
    return hashBytes(bytes, sizeof(T));
}

inline Hash hashChar(char key)
{
    return static_cast<Hash>(key);
}

inline Hash hashString(std::string_view string)
{
    const auto* bytes{reinterpret_cast<const std::uint8_t*>(string.data())};
    return hashBytes(bytes, string.size());
}

inline Hash hashCStr(const char* string, size_type length)
{
    // Assumes null-terminated.
    if (length == size_max) length = strlen(string);

    const auto* bytes{reinterpret_cast<const std::uint8_t*>(string)};
    return hashBytes(bytes, length);
}