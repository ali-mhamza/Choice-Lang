#include "../include/vartable.h"
#include "../include/common.h"
#include "hashFunctions.h"
#include <string>

VarEntry::VarEntry(std::string_view name, ui8 scope) :
    name(std::string(name)), scope(scope) {}

bool VarEntry::operator==(const VarEntry& other) const
{
    return ((this->scope == other.scope) // For short-circuit evaluation.
            && (this->name == other.name));
}

Hash hashVarEntry(const VarEntry& entry)
{
    Hash nameHash = hashBytes(
        reinterpret_cast<const ui8*>(entry.name.data()), entry.name.size()
    );
    Hash scopeHash = hashBytes(
        reinterpret_cast<const ui8*>(&entry.scope), sizeof(entry.scope)
    );
    return nameHash + scopeHash;
}