#include "common.h"
#include <array>
#include <string>
#include <string_view>

bool ends_with(const std::string_view str, const std::string_view suffix);
bool starts_with(const std::string_view str, const std::string_view prefix);
std::array<i64, 3> constructRange(const std::string_view tokText);