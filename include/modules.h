#pragma once
#include <personal/hash_table.h>
#include <filesystem>
#include <string>
#include <utility>

class Object;
using ModuleTable = HashTable<std::string, Object>;

[[nodiscard]] std::pair<bool, ModuleTable>
getModuleTable(const std::filesystem::path& path);