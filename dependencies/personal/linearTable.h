/*  This hash table has a basic implementation of open addressing to deal with hash collisions.
*   It uses linear probing (rather than quadratic probing or similar alternatives) to traverse through the array upon
*   collisions.
*   Deletions are dealt with using tombstones rather than back-shifting (or other methods), and the main structure is
*   an AOS (array of structs).
*/

#pragma once
#include "array.h"
#include "entryStruct.h"
#include "hashFunctions.h"
#include <cstdint>
#include <iostream> // For debugging.

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

        // For debugging.
        void printTable();
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
    uint32_t hash = getHash(key);
    uint32_t bitmask = static_cast<uint32_t>(entries.capacity() - 1);
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

    uint32_t hash = getHash(key);
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

    uint32_t hash = getHash(key);
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