/*  This hash table has a basic implementation of open addressing to deal with hash collisions.
*   It uses linear probing (rather than quadratic probing or similar alternatives) to traverse through the array upon
*   collisions.
*   Deletions are dealt with using tombstones rather than back-shifting (or other methods), and the main structure  *   is an AOS (array of structs).
*/

#pragma once
#include "array.h"
#include "entryStruct.h"
#include "hashFunctions.h"
#include <cstdint>
#include <iostream>
#include <tuple>
#include <type_traits>

#define KVHTEMP             template<typename Key, typename Value, typename HashFunc>
#define KVHTEMP_DEFAULT     template<typename Key, typename Value, typename HashFunc = Hasher<Key>>
#define EKV                 Entry<Key, Value>

template<typename Key>
struct Hasher
{
    Hash operator()(const Key& key)
    {
        return hashKey(key);
    }

    Hash operator()(const Key& key, size_t size)
    {
        return hashKey(key, size);
    }
};

template<typename Key, typename Value>
struct Pair
{
    EKV* entry;
    const Key* first;
    Value* second;

    Pair(EKV* entry) :
        entry(entry), first(&(entry->key)), second(&(entry->value)) {}
};

template<typename Key, typename Value>
struct std::tuple_size<Pair<Key, Value>> : std::integral_constant<size_t, 2> {};

template<typename Key, typename Value, size_t I>
struct std::tuple_element<I, Pair<Key, Value>>
{
    using type = std::conditional_t<I == 0, const Key, Value>;
};

template<size_t I, typename Key, typename Value>
decltype(auto) get(Pair<Key, Value>& pair)
{
    if constexpr (I == 0)
        return static_cast<const Key&>(*(pair.first));
    else
        return static_cast<Value&>(*(pair.second));
}

template<size_t I, typename Key, typename Value>
decltype(auto) get(const Pair<Key, Value>& pair)
{
    if constexpr (I == 0)
        return static_cast<const Key&>(*(pair.first));
    else
        return static_cast<const Value&>(*(pair.second));
}

template<size_t I, typename Key, typename Value>
decltype(auto) get(Pair<Key, Value>&& pair)
{
    if constexpr (I == 0)
        return static_cast<const Key&&>(*(pair.first));
    else
        return static_cast<Value&&>(*(pair.second));
}

KVHTEMP_DEFAULT
class linearTable
{
    private:
        HashFunc getHash;
        // Keeping the same load factor for 
        // both implementations.
        static constexpr double loadFactor = 0.8;
        Array<EKV> entries;
        size_t count;
        size_t maxIndex;

        linearTable(size_t size);

        void reorder();
        void resize();
        // Searches for existing key.
        // Returns reference to available bucket
        // if not found.
        EKV& findSlot(const Key& key, size_t* pos);
        // Adds a key with no value.
        EKV& emptyAdd(const Key& key);

    public:
        linearTable();
        linearTable(const linearTable& other);
        ~linearTable() = default;
        linearTable& operator=(const linearTable& other);
        Value& operator[](const Key& key);

        void add(const Key& key, const Value& value);
        Value* get(const Key& key);
        void set(const Key& key, const Value& value);
        void remove(const Key& key);
        void merge(const linearTable& other);
        size_t size();
        void clear();

        // For debugging.
        void printTable();

        using IterPair = Pair<Key, Value>;

        class iterator
        {
            private:
                EKV* ptr;
                EKV* end;
                IterPair pair;

            public:
                iterator() = default;
                iterator(EKV* ptr, EKV* end);
                iterator(const iterator& other);
                iterator& operator=(const iterator& other);

                IterPair& operator*();
                IterPair* operator->();
                iterator& operator++();
                iterator& operator++(int);
                bool operator==(const iterator& other) const;
                bool operator!=(const iterator& other) const;
        };

        class const_iterator
        {
            private:
                EKV* ptr;
                EKV* end;
                IterPair pair;

            public:
                const_iterator() = default;
                const_iterator(const EKV* ptr, const EKV* end);
                const_iterator(const const_iterator& other);
                const_iterator& operator=(const const_iterator& other);

                const IterPair& operator*() const;
                const IterPair* operator->() const;
                const_iterator& operator++();
                const_iterator& operator++(int);
                bool operator==(const const_iterator& other) const;
                bool operator!=(const const_iterator& other) const;
        };

        [[nodiscard]] iterator begin() noexcept;
        [[nodiscard]] iterator end() noexcept;
        [[nodiscard]] const_iterator begin() const noexcept;
        [[nodiscard]] const_iterator end() const noexcept;
        [[nodiscard]] const_iterator cbegin() const noexcept;
        [[nodiscard]] const_iterator cend() const noexcept;
};

KVHTEMP
linearTable<Key, Value, HashFunc>::linearTable() :
    getHash(HashFunc()), entries(2), count(0), maxIndex(SIZE_MAX) {}

KVHTEMP
linearTable<Key, Value, HashFunc>::linearTable(const linearTable<Key, Value, HashFunc>& other) :
    entries(other.entries), count(other.count),
    maxIndex(other.maxIndex) {}

KVHTEMP
linearTable<Key, Value, HashFunc>::linearTable(size_t size) :
    getHash(HashFunc()), entries(size), count(0), maxIndex(SIZE_MAX) {}

KVHTEMP
linearTable<Key, Value, HashFunc>& linearTable<Key, Value, HashFunc>::
operator=(const linearTable<Key, Value, HashFunc>& other)
{
    this->entries = other.entries;
    return *this;
}

KVHTEMP
Value& linearTable<Key, Value, HashFunc>::operator[](const Key& key)
{
    EKV& existEntry = findSlot(key, nullptr);
    if (existEntry.state == VALID) // Key already exists.
        return existEntry.value;
    return emptyAdd(key).value;
}

KVHTEMP
void linearTable<Key, Value, HashFunc>::reorder()
{
    size_t capacity = entries.capacity();
    linearTable<Key, Value, HashFunc> newTable(capacity);
    for (size_t i = 0; i < maxIndex + 1; i++)
    {
        EKV& entry = entries.slot(i);
        if (entry.state != VALID)
            continue;
        newTable.add(entry.key, entry.value);
    }
    this->entries = newTable.entries;
    this->count = newTable.count; // Might have changed since we dropped tombstones.
    this->maxIndex = newTable.maxIndex;
}

KVHTEMP
void linearTable<Key, Value, HashFunc>::resize()
{   
    if ((entries.capacity() * loadFactor) < count + 1)
    {
        if (count == 0)
            entries.grow();
        else
        {
            entries.increaseCapacity();
            reorder();
        }
    }
}

KVHTEMP
EKV& linearTable<Key, Value, HashFunc>::findSlot(const Key& key, size_t* pos)
{
    std::uint32_t hash = getHash(key);
    auto bitmask = static_cast<std::uint32_t>(entries.capacity() - 1);
    size_t index = hash & bitmask;

    EKV* tombstone = nullptr;

    EKV* entry = &(entries.slot(index));
    if (pos != nullptr)
        *pos = index;
    while (entry->state != EMPTY)
    {
        if (pos != nullptr)
            *pos = index;
        
        if (entry->key == key)
            return *entry;
        
        if (entry->state == TOMBSTONE)
            tombstone = entry;

        index = (index + 1) & bitmask;
        if (pos != nullptr)
            *pos = index;
        entry = &(entries.slot(index));
    }

    // We didn't find an entry with the key.
    // If we found a tombstone, return it (to reuse it).
    // Otherwise, return the first empty slot we found.
    return (tombstone == nullptr ? *entry : *tombstone);
}

KVHTEMP
EKV& linearTable<Key, Value, HashFunc>::emptyAdd(const Key& key)
{
    // This method is only called internally,
    // so we can skip checks for the key existing
    // while being careful to use the method properly.
    
    resize(); // Grow if needed.

    std::uint32_t hash = getHash(key);
    size_t index;
    EKV& newEntry = findSlot(key, &index);

    if (newEntry.state != TOMBSTONE)
        count++;

    newEntry.key = key;
    newEntry.hash = hash;
    newEntry.state = VALID;

    if (maxIndex == SIZE_MAX)
        maxIndex = index;
    else
        maxIndex = (maxIndex > index ? maxIndex : index);

    return newEntry;
}

KVHTEMP
void linearTable<Key, Value, HashFunc>::add(const Key& key,
    const Value& value)
{
    EKV& existEntry = findSlot(key, nullptr);
    if (existEntry.state == VALID) // Key already exists.
    {
        existEntry.value = value;
        return;
    }

    resize(); // Grow size if needed.

    std::uint32_t hash = getHash(key);
    size_t index;
    EKV& newEntry = findSlot(key, &index);

    if (newEntry.state != TOMBSTONE)
        count++;

    newEntry.key = key;
    newEntry.value = value;
    newEntry.hash = hash;
    newEntry.state = VALID;

    if (maxIndex == SIZE_MAX)
        maxIndex = index;
    else
        maxIndex = (maxIndex > index ? maxIndex : index);
}

KVHTEMP
Value* linearTable<Key, Value, HashFunc>::get(const Key& key)
{
    if (count == 0) return nullptr;
    
    EKV& entry = findSlot(key, nullptr);
    if (entry.state != VALID)
        return nullptr;
    else
        return &(entry.value);
}

KVHTEMP
void linearTable<Key, Value, HashFunc>::set(const Key& key,
    const Value& value)
{
    EKV& entry = findSlot(key, nullptr);
    if (entry.state != VALID)
        add(key, value);
    else
        entry.value = value;
}

KVHTEMP
void linearTable<Key, Value, HashFunc>::remove(const Key& key)
{
    EKV& entry = findSlot(key, nullptr);
    if (entry.state == VALID) // Leave it if it's already empty.
        entry.state = TOMBSTONE;
}

KVHTEMP
void linearTable<Key, Value, HashFunc>::merge(const linearTable<Key, Value, HashFunc>& other)
{
    size_t capacity = other.entries.capacity();
    for (size_t i = 0; i < capacity; i++)
    {
        EKV entry = other.entries.slot(i);
        if (entry.state == VALID)
            add(entry.key, entry.value);
    }
}

KVHTEMP
size_t linearTable<Key, Value, HashFunc>::size()
{
    return count;
}

KVHTEMP
void linearTable<Key, Value, HashFunc>::clear()
{
    entries.clear();
}

KVHTEMP
void linearTable<Key, Value, HashFunc>::printTable()
{
    size_t cap = entries.capacity();
    for (size_t i = 0; i < cap; i++)
    {
        std::cout << "Slot " << i << ": ";
        EKV& entry = entries.slot(i);
        if (entry.state == EMPTY)
            std::cout << "EMPTY\n";
        else
        {
            std::cout << "(" << entry.key << ", "
                << entry.value << ")";
            if (entry.state == TOMBSTONE)
                std::cout << " (TOMB)\n";
            else
                std::cout << '\n';
        }
    }
}

// Iterator implementation.

#define tableIter       linearTable<Key, Value, HashFunc>::iterator

KVHTEMP
tableIter::iterator(EKV* ptr, EKV* end) :
    ptr(ptr), end(end), pair(ptr) {}

KVHTEMP
tableIter::iterator(const tableIter& other) :
    ptr(other.ptr), end(other.end), pair(other.pair) {}

KVHTEMP
typename tableIter& tableIter::operator=(const tableIter& other)
{
    this->ptr = other.ptr;
    this->end = other.end;
    this->pair = other.pair;
    return *this;
}

KVHTEMP
typename linearTable<Key, Value, HashFunc>::IterPair&
tableIter::operator*()
{
    return pair;
}

KVHTEMP
typename linearTable<Key, Value, HashFunc>::IterPair*
tableIter::operator->()
{
    return &pair;
}

KVHTEMP
typename tableIter& tableIter::operator++()
{
    ++ptr;
    while ((ptr != end) && (ptr->state != VALID))
        ++ptr;
    pair = Pair(ptr);
    return *this;
}

KVHTEMP
typename tableIter& tableIter::operator++(int)
{
    ptr++;
    while ((ptr != end) && (ptr->state != VALID))
        ptr++;
    pair = Pair(ptr);
    return *this;
}

KVHTEMP
bool tableIter::operator==(const tableIter& other) const
{
    return (this->ptr == other.ptr);
}

KVHTEMP
bool tableIter::operator!=(const tableIter& other) const
{
    return (this->ptr != other.ptr);
}

KVHTEMP
typename tableIter linearTable<Key, Value, HashFunc>::begin() noexcept
{
    EKV* ptr = &(entries.front());
    EKV* end = ptr + entries.capacity();

    while ((ptr != end) && (ptr->state != VALID))
        ptr++;
    return tableIter(ptr, end);
}

KVHTEMP
typename tableIter linearTable<Key, Value, HashFunc>::end() noexcept
{
    EKV* end = &(entries.front()) + entries.capacity();
    return tableIter(end, end);
}

// Const iterator implementation.

#define constTableIter  linearTable<Key, Value, HashFunc>::const_iterator

KVHTEMP
constTableIter::const_iterator(const EKV* ptr, const EKV* end) :
    ptr(ptr), end(end), pair(ptr) {}

KVHTEMP
constTableIter::const_iterator(const constTableIter& other) :
    ptr(other.ptr), end(other.end), pair(other.pair) {}

KVHTEMP
typename constTableIter& constTableIter::operator=(const constTableIter& other)
{
    this->ptr = other.ptr;
    this->end = other.end;
    this->pair = other.pair;
    return *this;
}

KVHTEMP
const typename linearTable<Key, Value, HashFunc>::IterPair&
constTableIter::operator*() const
{
    return pair;
}

KVHTEMP
const typename linearTable<Key, Value, HashFunc>::IterPair*
constTableIter::operator->() const
{
    return &pair;
}

KVHTEMP
typename constTableIter& constTableIter::operator++()
{
    ++ptr;
    while ((ptr != end) && (ptr->state != VALID))
        ++ptr;
    pair = Pair(ptr);
    return *this;
}

KVHTEMP
typename constTableIter& constTableIter::operator++(int)
{
    ptr++;
    while ((ptr != end) && (ptr->state != VALID))
        ptr++;
    pair = Pair(ptr);
    return *this;
}

KVHTEMP
bool constTableIter::operator==(const constTableIter& other) const
{
    return (this->ptr == other.ptr);
}

KVHTEMP
bool constTableIter::operator!=(const constTableIter& other) const
{
    return (this->ptr != other.ptr);
}

KVHTEMP
typename constTableIter linearTable<Key, Value, HashFunc>::begin()
const noexcept
{
    return cbegin();
}

KVHTEMP
typename constTableIter linearTable<Key, Value, HashFunc>::end()
const noexcept
{
    return cend();
}

KVHTEMP
typename constTableIter linearTable<Key, Value, HashFunc>::cbegin()
const noexcept
{
    EKV* ptr = &(entries.front());
    EKV* end = ptr + entries.capacity();

    while ((ptr != end) && (ptr->state != VALID))
        ptr++;
    return constTableIter(ptr, end);
}

KVHTEMP
typename constTableIter linearTable<Key, Value, HashFunc>::cend()
const noexcept
{
    EKV* end = &(entries.front()) + entries.capacity();
    return constTableIter(end, end);
}

#undef tableIter
#undef constTableIter