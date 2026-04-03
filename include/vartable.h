#pragma once
#include "common.h"     // For Hash, ui8.
#include <string>
#include <string_view>

struct VarEntry
{
    std::string name{};
    ui8 scope{};

    VarEntry() = default;
    VarEntry(std::string_view name, ui8 scope);
    bool operator==(const VarEntry& other) const;
};

Hash hashVarEntry(const VarEntry& entry);
struct VarHasher
{
    Hash operator()(const VarEntry& entry) const
    {
        return hashVarEntry(entry);
    }
};