#include "common.h"
#include "object.h"
#include <array>
#include <string_view>

bool ends_with(std::string_view str, std::string_view suffix);
bool starts_with(std::string_view str, std::string_view prefix);
std::array<i64, 3> constructRange(std::string_view tokText);
std::string normalizeNewlines(const std::string_view& sv);