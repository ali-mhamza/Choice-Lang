#pragma once
#include <cstdint>

#define KVTEMP template<typename Key, typename Value>

enum EntryState : std::uint8_t
{
    VALID,
    TOMBSTONE,
    EMPTY
};

KVTEMP
struct Entry
{
    Key key;
    Value value;
    std::uint32_t hash;
    EntryState state;

    Entry();
    Entry(const Key& key, const Value& value);
    Entry(const Key& key, std::uint32_t hash);
    Entry(const Key& key, const Value& value, std::uint32_t hash);
    ~Entry() = default;
    bool operator==(const Entry& other) const;
};

KVTEMP
Entry<Key, Value>::Entry() :
    state(EMPTY) {}

KVTEMP
Entry<Key, Value>::Entry(const Key& key, const Value& value) :
    key(key), value(value), state(VALID) {}

KVTEMP
Entry<Key, Value>::Entry(const Key& key, std::uint32_t hash) :
    key(key), hash(hash), state(VALID) {}

KVTEMP
Entry<Key, Value>::Entry(const Key& key, const Value& value,
    std::uint32_t hash) :
    key(key), value(value), hash(hash), state(VALID) {}

KVTEMP
bool Entry<Key, Value>::operator==(const Entry& other) const
{
    return ((this->hash == other.hash) && // For short-circuit evaluation.
            (this->key == other.key)); // Cannot add a key twice.
}