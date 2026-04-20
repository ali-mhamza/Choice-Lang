#pragma once
#include <type_traits>

#if defined(_MSVC_LANG)
    #define CPP_VERSION _MSVC_LANG
#elif defined (_MSC_VER)
    #define CPP_VERSION 0 // Dummy value.
#else
    #define CPP_VERSION __cplusplus
#endif

// #if CPP_VERSION < 202002L

template<typename T, typename = void>
struct has_equal : std::false_type {};

template<typename T>
struct has_equal<T, std::void_t<decltype(std::declval<T>().operator==())>> : std::true_type {};

template<typename T>
inline constexpr bool has_equal_v = has_equal<T>::value;

template<typename T, typename = void>
struct has_not_equal : std::false_type {};

template<typename T>
struct has_not_equal<T, std::void_t<decltype(std::declval<T>().operator!=())>> : std::true_type {};

template<typename T>
inline constexpr bool has_not_equal_v = has_not_equal<T>::value;

template<typename T, typename = void>
struct has_less : std::false_type {};

template<typename T>
struct has_less<T, std::void_t<decltype(std::declval<T>().operator<())>> : std::true_type {};

template<typename T>
inline constexpr bool has_less_v = has_less<T>::value;

template<typename T, typename = void>
struct has_greater : std::false_type {};

template<typename T>
struct has_greater<T, std::void_t<decltype(std::declval<T>().operator>())>> : std::true_type {};

template<typename T>
inline constexpr bool has_greater_v = has_greater<T>::value;

template<typename T, typename = void>
struct has_greater_equal : std::false_type {};

template<typename T>
struct has_greater_equal<T, std::void_t<decltype(std::declval<T>().operator>=())>> : std::true_type {};

template<typename T>
inline constexpr bool has_greater_equal_v = has_greater_equal<T>::value;

template<typename T, typename = void>
struct has_less_equal : std::false_type {};

template<typename T>
struct has_less_equal<T, std::void_t<decltype(std::declval<T>().operator<=())>> : std::true_type {};

template<typename T>
inline constexpr bool has_less_equal_v = has_less_equal<T>::value;

template<typename T>
inline constexpr bool can_compare_v = has_equal<T>::value && has_not_equal<T>::value
    && has_greater<T>::value && has_less<T>::value
    && has_greater_equal<T>::value && has_less_equal<T>::value;

// #else /* CPP_VERSION >= 202002L */

// #endif