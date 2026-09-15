#pragma once
#include "generics.h"
#include "hash_functions.h"
#include <cstdint>

enum class EntryState : std::uint8_t
{
    Valid,
    Tombstone,
    Empty
};

template<typename Key, typename Value>
struct Entry
{
    // static_assert(
    //     std::is_default_constructible_v<Key> && std::is_default_constructible_v<Value>,
    //     "Key and value types must be default-constructible."
    // );

    // static_assert(
    //     std::is_copy_constructible_v<Key> && std::is_copy_constructible_v<Value>,
    //     "Key and value types must be copy-constructible."
    // );

    // static_assert(has_equal_v<Key>, "Key must be comparable.");

    Key key{};
    Value value{};
    Hash hash{};
    EntryState state{EntryState::Empty};

    Entry() = default;

    static Entry valueEntry(const Key& key, const Value& value)
    {
        Entry entry{};
        entry.key = key;
        entry.value = value;
        entry.state = EntryState::Valid;
        return entry;
    }

    static Entry hashEntry(const Key& key, Hash hash)
    {
        Entry entry{};
        entry.key = key;
        entry.hash = hash;
        entry.state = EntryState::Valid;
        return entry;
    }

    Entry(const Key& key, const Value& value, Hash hash) :
        key{key}, value{value}, hash{hash}, state{EntryState::Valid} {}

    ~Entry() = default;

    bool operator==(const Entry& other) const
    {
        return ((this->hash == other.hash) && // For short-circuit evaluation.
                (this->key == other.key)); // Cannot add a key twice.
    }
};

#define EKV Entry<Key, Value>

template<typename Key, typename Value, typename EKVType>
struct Pair
{
    const EKV* entry{};
    const Key* first{};

    using ValueType =
        std::conditional_t<std::is_const_v<EKVType>, const Value, Value>;

    ValueType* second{};

    Pair() = default;
    Pair(EKV* entry) :
        entry{entry}, first{&(entry->key)}, second{&(entry->value)} {}
    Pair(const EKV* entry) :
        entry{entry}, first{&(entry->key)}, second{&(entry->value)} {}
};

template<typename Key, typename Value, typename EKVType>
struct std::tuple_size<Pair<Key, Value, EKVType>> :
    std::integral_constant<size_type, 2> {};

template<typename Key, typename Value, typename EKVType, size_type I>
struct std::tuple_element<I, Pair<Key, Value, EKVType>>
{
    using type = std::conditional_t<I == 0, const Key, Value>;
};

template<size_type I, typename Key, typename Value, typename EKVType>
decltype(auto) get(Pair<Key, Value, EKVType>& pair)
{
    if constexpr (I == 0)
        return static_cast<const Key&>(*(pair.first));
    else
        return static_cast<Value&>(*(pair.second));
}

template<size_type I, typename Key, typename Value, typename EKVType>
decltype(auto) get(const Pair<Key, Value, EKVType>& pair)
{
    if constexpr (I == 0)
        return static_cast<const Key&>(*(pair.first));
    else
        return static_cast<const Value&>(*(pair.second));
}

template<size_type I, typename Key, typename Value, typename EKVType>
decltype(auto) get(Pair<Key, Value, EKVType>&& pair)
{
    if constexpr (I == 0)
        return static_cast<const Key&&>(*(pair.first));
    else
        return static_cast<Value&&>(*(pair.second));
}