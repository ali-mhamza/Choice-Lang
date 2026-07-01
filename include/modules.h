#pragma once
#include <personal/hash_table.h>
#include <string_view>
#include <utility>

class Object;
using ModuleTable = HashTable<std::string, Object>;
[[nodiscard]] std::pair<bool, ModuleTable>
getModuleTable(std::string_view file, std::string_view dir);