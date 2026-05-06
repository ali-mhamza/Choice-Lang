#include "common.h"
#include <string_view>
#include <vector>

[[nodiscard]]
bool ends_with(const std::string_view str, const std::string_view suffix);
[[nodiscard]]
bool starts_with(const std::string_view str, const std::string_view prefix);
[[nodiscard]]
std::vector<std::string> split(std::string_view str, std::string_view delim);

[[nodiscard]] static inline bool isBinary(char c)
{
    return ((c == '0') || (c == '1'));
}

[[nodiscard]] static inline bool isOctal(char c)
{
    return ((c >= '0') && (c <= '7'));
}

[[nodiscard]] static inline bool isHex(char c)
{
    return isxdigit(c);
}

[[nodiscard]] static inline u8 fromBinary(char c)
{
    return (c - '0');
}

[[nodiscard]] static inline u8 fromOctal(char c)
{
    return (c - '0');
}

[[nodiscard]] static inline u8 fromHex(char c)
{
    if (isdigit(c)) return (c - '0');
    if (isupper(c)) return (c - 'A' + 10);
    return (c - 'a' + 10);
}