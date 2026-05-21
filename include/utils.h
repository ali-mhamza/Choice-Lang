#include "common.h"
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

/* File extensions. */

constexpr std::string_view CH_FILE_EXT{".ch"};
constexpr std::string_view CH_BYTECODE_EXT{".chbc"};
constexpr std::string_view CH_DEBUG_EXT{".chdbg"};

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