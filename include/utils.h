#include "common.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

/* File extensions. */

constexpr std::string_view CH_FILE_EXT{".ch"};
constexpr std::string_view CH_BYTECODE_EXT{".chbc"};
constexpr std::string_view CH_DEBUG_EXT{".chdbg"};

/* Diagnostic colors. */

constexpr std::string_view GREEN{"\x1b[32m"};
constexpr std::string_view RED{"\033[31m"};
constexpr std::string_view YELLOW{"\033[33m"};
constexpr std::string_view BOLD{"\033[1m"};
constexpr std::string_view NORMAL{"\033[0m"};

#define CH_PRINT_SUCCESS(msg) CH_PRINT(stdout, "{}" msg "{}", GREEN, NORMAL)
#define CH_PRINT_SUCCESS_ARGS(msg, ...)                            \
    CH_PRINT(stdout, "{}" msg "{}", GREEN, __VA_ARGS__, NORMAL)

#define CH_PRINT_WARNING(msg) CH_PRINT(stderr, "{}" msg "{}", YELLOW, NORMAL)
#define CH_PRINT_WARNING_ARGS(msg, ...)                            \
    CH_PRINT(stderr, "{}" msg "{}", YELLOW, __VA_ARGS__, NORMAL)

#define CH_PRINT_ERROR(msg) CH_PRINT(stderr, "{}" msg "{}", RED, NORMAL)
#define CH_PRINT_ERROR_ARGS(msg, ...)                              \
    CH_PRINT(stderr, "{}" msg "{}", RED, __VA_ARGS__, NORMAL)

/* General helpers. */

[[nodiscard]] std::ifstream openFile(
	const std::filesystem::path& filePath,
	bool binary = false,
	const std::string_view message = "Failed to open file."
);
[[nodiscard]] std::string readFile(std::ifstream& stream);
[[nodiscard]] std::string readFile(
    const std::filesystem::path& filePath,
    bool binary = false
);

[[nodiscard]] bool fileMoreRecent(
    const std::filesystem::path& a,
    const std::filesystem::path& b
);

void normalizeInput(std::string& input);

[[nodiscard]]
bool ends_with(const std::string_view str, const std::string_view suffix);
[[nodiscard]]
bool starts_with(const std::string_view str, const std::string_view prefix);

[[nodiscard]]
std::vector<std::string> split(std::string_view str, std::string_view delim);

/* Inline functions. */

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