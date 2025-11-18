#pragma once
#include <cstdint>
#include <vector>

class Token;
using vT = std::vector<Token>;

constexpr bool timeRun = false;
constexpr bool dumpTokens = true;
constexpr bool dumpBytecode = false;
constexpr bool executeCode = false;
constexpr int TAB_SIZE = 4;