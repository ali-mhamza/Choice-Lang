#include "../include/vartable.h"
#include <cstring>

// Pasting the function here.
// Using Jenkins' one-at-a-time function.
static Hash hashBytes(const uint8_t* bytes, size_t size)
{
    Hash hash = 0;

    for (size_t i = 0; i < size; i++)
    {
        hash += bytes[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

VarEntry::VarEntry(std::string_view name, ui8 scope) :
    name(std::string(name)), scope(scope) {}

bool VarEntry::operator==(const VarEntry& other) const
{
    return ((this->scope == other.scope) // For short-circuit evaluation.
            && (this->name == other.name));
}

std::ostream& operator<<(std::ostream& os, const VarEntry& entry)
{
    os << "{" << entry.name << ", " << entry.scope << "}";
    return os;
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