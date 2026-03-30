#pragma once
#include "bytecode.h"
#include "common.h"     // For vObj, vByte, vT in multiple function signatures.
#include <fstream>      // For ifstream in readCache.
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

std::string readFile(const char* fileName);
vObj reconstructPool(const vByte& poolBytes);
ByteCode readCache(std::ifstream& fileIn);
void optionShowTokens(const vT& tokens);
void optionShowBytes(const ByteCode& chunk);
void optionLoad(const char* fileName);
void optionDis(const char* fileName);
void optionCacheBytes(const ByteCode& chunk, const char* fileName);
bool fileNameCheck(std::string_view fileName);