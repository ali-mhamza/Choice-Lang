#pragma once
#include "bytecode.h"
#include "common.h"
#include "diagnostic.h"
#include "object.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

/* File extensions. */

constexpr std::string_view CH_FILE_EXT{".ch"};
constexpr std::string_view CH_BYTECODE_EXT{".chbc"};
constexpr std::string_view CH_DEBUG_EXT{".chdbg"};

/* Miscellaneous helpers. */

[[nodiscard]] std::string readFile(std::ifstream& stream);
[[nodiscard]] std::string readFile(
    const std::filesystem::path& filePath,
    bool binary = false
);
bool fileMoreRecent(
    const std::filesystem::path& a,
    const std::filesystem::path& b
);
void normalizeInput(std::string& input);
void optionShowTokens(FileID id, const vT& tokens);
void optionShowBytes(const Function* func);
void optionLoad(const std::filesystem::path& file);
void optionDis(const std::filesystem::path& file);
void optionCacheBytes(FileID id, const ByteCode& chunk);
[[nodiscard]] bool fileNameCheck(const std::string_view fileName);