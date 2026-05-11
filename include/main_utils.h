#pragma once
#include "bytecode.h"
#include "common.h"
#include "diagnostic.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#ifdef DEBUG
    #define CHECK_EOF()     \
        do {                \
            if (it == end)  \
			    eofError(); \
        } while (false)
#else
    #define CHECK_EOF()
#endif

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
void optionShowTokens(SourceManager* manager, FileID id, const vT& tokens);
void optionShowBytes(const ByteCode& chunk);
void optionLoad(const char* fileName);
void optionDis(const char* fileName);
void optionCacheBytes(FileID id, const ByteCode& chunk);
[[nodiscard]] bool fileNameCheck(const std::string_view fileName);