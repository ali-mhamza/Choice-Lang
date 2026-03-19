#pragma once
#include <climits>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

using Hash = uint32_t;

template<typename Key>
inline Hash hashKey(const Key& key, size_t size = SIZE_MAX);
template<typename T>
inline Hash hashNumeric(T key);
inline Hash hashChar(char key);
inline Hash hashString(std::string_view string);
inline Hash hashCStr(const char* string, size_t length = SIZE_MAX);

// Need to have the implementations here since
// this file is #included in source files
// with template implementations.

template<typename Key>
inline Hash hashKey(const Key& key, size_t size)
{
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
inline Hash hashBytes(const uint8_t* bytes, size_t size)
{
    Hash hash = 0;

    for (size_t i = 0; i < size; i++)
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
inline Hash hashNumeric(T key)
{
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&key);
    return hashBytes(bytes, sizeof(T));
}

inline Hash hashChar(char key)
{
    return (uint32_t) key;
}

inline Hash hashString(std::string_view string)
{
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(string.data());
    return hashBytes(bytes, string.size());
}

inline Hash hashCStr(const char* string, size_t length)
{
    if (length == SIZE_MAX)
        length = strlen(string); // Null-terminated.
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(string);
    return hashBytes(bytes, length);
}