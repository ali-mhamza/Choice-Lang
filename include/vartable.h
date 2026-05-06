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
    [[nodiscard]] bool operator==(const VarEntry& other) const;
};

[[nodiscard]] Hash hashVarEntry(const VarEntry& entry);
struct VarHasher
{
    [[nodiscard]] Hash operator()(const VarEntry& entry) const
    {
        return hashVarEntry(entry);
    }
};