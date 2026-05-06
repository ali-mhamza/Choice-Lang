#pragma once
#include "common.h"
#include <string>
#include <string_view>

struct VarEntry
{
    std::string name{};
    u8 scope{};

    VarEntry() = default;
    VarEntry(std::string_view name, u8 scope);
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