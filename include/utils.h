#include "common.h"
#include <array>
#include <string>
#include <string_view>

bool ends_with(const std::string_view str, const std::string_view suffix);
bool starts_with(const std::string_view str, const std::string_view prefix);
std::array<i64, 3> constructRange(const std::string_view tokText);

static inline bool isBinary(char c)
{
    return ((c == '0') || (c == '1'));
}

static inline bool isOctal(char c)
{
    return ((c >= '0') && (c <= '7'));
}

static inline bool isHex(char c)
{
    return isxdigit(c);
}

static inline ui8 fromBinary(char c)
{
    return (c - '0');
}

static inline ui8 fromOctal(char c)
{
    return (c - '0');
}

static inline ui8 fromHex(char c)
{
    if (isdigit(c)) return (c - '0');
    if (isupper(c)) return (c - 'A' + 10);
    return (c - 'a' + 10);
}