/*  This hash table has a basic implementation of open addressing to deal with hash collisions.
*   It uses linear probing (rather than quadratic probing or similar alternatives) to traverse through the
*   array upon collisions.
*   Deletions are dealt with using tombstones rather than back-shifting (or other methods), and the main
*   structure is an AOS (array of structs).
*/

#pragma once
#include "common.h"
#include "entry_struct.h"
#include "hash_functions.h"
#include "table_error.h"
#include <cstdint>
#include <iostream>
#include <memory>
#include <tuple>
#include <type_traits>

template<typename Key, typename Value>
using LinearDefaultAlloc = std::allocator<EKV>;

template<
    typename Key,
    typename Value,
    typename HashFunc = Hasher<Key>,
    typename Alloc = LinearDefaultAlloc<Key, Value>
>
class LinearTable
{
    // static_assert(
    //     std::is_default_constructible_v<EKV>
    //     && std::is_nothrow_default_constructible_v<EKV>,
    //     "Entry type must be default-constructible without throwing."
    // );

    // static_assert(
    //     std::is_default_constructible_v<Alloc>
    //     && std::is_nothrow_default_constructible_v<Alloc>,
    //     "Allocator type must be default-constructible without throwing."
    // );

    private:
        using param_key_type = std::conditional_t<
            sizeof(Key) <= 8,
            Key,
            const Key&
        >;
        using param_value_type = std::conditional_t<
            sizeof(Value) <= 8,
            Value,
            const Value&
        >;
        using signed_type = std::int64_t;

        static Alloc allocator;
        static HashFunc getHash;
        static constexpr size_type defaultSize{2};
        static constexpr size_type growFactor{2};
        static constexpr double loadFactor{TABLE_LOAD_FACTOR};

        EKV* entries{nullptr};
        size_type count{0};             // Number of pairs in the table.
        size_type used{0};              // Number of non-empty slots in the table.
        size_type capacity{0};          // Number of total slots in the table.
        size_type maxIndex{size_max};   // Highest index with non-empty slot in the table.

        LinearTable(size_type size) :
            entries{alloc(size)}, capacity{size} {}

        static EKV* alloc(size_type count)
        {
            EKV* mem{allocator.allocate(count)};
            std::uninitialized_fill_n(mem, count, EKV{});
            return mem;
        }

        static void dealloc(EKV* mem, size_type count)
        {
            if (mem == nullptr) return;

            std::destroy_n(mem, count);
            allocator.deallocate(mem, count);
        }

        void init()
        {
            entries = alloc(defaultSize);
            capacity = defaultSize;
        }

        // Shift cannot be larger than end - start.
        void copy(EKV* dest, size_type start, size_type end, signed_type shift = 0)
        {
            if ((entries == nullptr) || (end - start > count))
                return;

            if constexpr (std::is_trivially_copyable_v<EKV>)
            {
                std::memcpy(
                    dest + start + shift,
                    entries + start,
                    (end - start) * sizeof(EKV)
                );
            }
            else if constexpr (std::is_nothrow_move_assignable_v<EKV>)
            {
                for (size_type i{start}; i < end; i++)
                    dest[i + shift] = std::move(entries[i]);
            }
            else
            {
                for (size_type i{start}; i < end; i++)
                    dest[i + shift] = entries[i];
            }
        }

        void grow()
        {
            dealloc(this->entries, capacity);
            capacity *= growFactor;
            EKV* newEntries{alloc(capacity)};
            this->entries = newEntries;
        }

        void rehash()
        {
            size_type newCapacity{capacity * growFactor};
            LinearTable<Key, Value, HashFunc> newTable{newCapacity};
            for (size_type i{0}; i <= maxIndex; i++)
            {
                EKV& entry{entries[i]};
                if (entry.state != EntryState::Valid) continue;
                newTable.add(entry.key, entry.value);
            }

            *this = std::move(newTable);
        }

        void resize()
        {
            if ((capacity * loadFactor) < used + 1)
                count == 0 ? grow() : rehash();
        }

        void increaseCount()
        {
            count++;
            used++;
        }

        // Searches for existing key.
        // Returns reference to available slot if not found.
        EKV& findSlot(param_key_type key, size_type* pos = nullptr)
        {
            Hash hash{getHash(key)};
            size_type bitmask{capacity - 1};
            size_type index{hash & bitmask};

            EKV* tombstone{nullptr};
            EKV* entry{&entries[index]};

            if (pos != nullptr)
                *pos = index;
            while (entry->state != EntryState::Empty)
            {
                if (pos != nullptr)
                    *pos = index;

                if (entry->key == key)
                    return *entry;

                if (entry->state == EntryState::Tombstone)
                    tombstone = entry;

                index = (index + 1) & bitmask;
                if (pos != nullptr)
                    *pos = index;

                entry = &entries[index];
            }

            // We didn't find an entry with the key.
            // If we found a tombstone, return it (to reuse it).
            // Otherwise, return the first empty slot we found.
            return (tombstone == nullptr ? *entry : *tombstone);
        }

        const EKV& findSlot(param_key_type key, size_type* pos = nullptr) const
        {
            Hash hash{getHash(key)};
            size_type bitmask{capacity - 1};
            size_type index{hash & bitmask};

            const EKV* tombstone{nullptr};
            const EKV* entry{&entries[index]};

            if (pos != nullptr)
                *pos = index;
            while (entry->state != EntryState::Empty)
            {
                if (pos != nullptr)
                    *pos = index;

                if (entry->key == key)
                    return *entry;

                if (entry->state == EntryState::Tombstone)
                    tombstone = entry;

                index = (index + 1) & bitmask;
                if (pos != nullptr)
                    *pos = index;

                entry = &entries[index];
            }

            // We didn't find an entry with the key.
            // If we found a tombstone, return it (to reuse it).
            // Otherwise, return the first empty slot we found.
            return (tombstone == nullptr ? *entry : *tombstone);
        }

        // Adds a key with no value.
        EKV& emptyAdd(param_key_type key)
        {
            // This method is only called internally,
            // so we can skip checks for the key existing
            // while being careful to use the method properly.

            resize(); // Grow if needed.

            Hash hash{getHash(key)};
            size_type index{};
            EKV& newEntry{findSlot(key, &index)};

            if (newEntry.state != EntryState::Tombstone) increaseCount();
            newEntry = EKV::hashEntry(key, hash);

            if (maxIndex == size_max)
                maxIndex = index;
            else
                maxIndex = (maxIndex > index ? maxIndex : index);

            return newEntry;
        }

    public:
        LinearTable() : LinearTable{defaultSize} {}

        LinearTable(const LinearTable& other) : LinearTable{defaultSize}
        {
            merge(other);
        }

        LinearTable& operator=(const LinearTable& other)
        {
            if (this != &other)
            {
                clear();
                init();
                merge(other);
            }

            return *this;
        }

        LinearTable(LinearTable&& other) :
            entries{other.entries}, count{other.count}, used{other.used},
            capacity{other.capacity}, maxIndex{other.maxIndex}
        {
            other.entries = nullptr;
            other.count = 0;
            other.used = 0;
            other.capacity = 0;
            other.maxIndex = 0;
        }

        LinearTable& operator=(LinearTable&& other)
        {
            if (this != &other)
            {
                clear();

                this->entries = other.entries;
                this->count = other.count;
                this->used = other.used;
                this->capacity = other.capacity;
                this->maxIndex = other.maxIndex;

                other.entries = nullptr;
                other.count = 0;
                other.used = 0;
                other.capacity = 0;
                other.maxIndex = 0;
            }

            return *this;
        }

        ~LinearTable()
        {
            clear();
        }

        Value& operator[](param_key_type key)
        {
            EKV& existEntry{findSlot(key)};
            if (existEntry.state == EntryState::Valid) // Key already exists.
                return existEntry.value;
            return emptyAdd(key).value;
        }

        const Value& operator[](param_key_type key) const
        {
            const EKV& existEntry{findSlot(key)};
            if (existEntry.state == EntryState::Valid)
                return existEntry.value;
            throw EntryNotFoundError{};
        }

        void add(param_key_type key, param_value_type value)
        {
            EKV& existEntry = findSlot(key);
            if (existEntry.state == EntryState::Valid) // Key already exists.
            {
                existEntry.value = value;
                return;
            }

            resize(); // Grow size if needed.
            Hash hash{getHash(key)};
            size_type index{};
            EKV& newEntry{findSlot(key, &index)};

            if (newEntry.state != EntryState::Tombstone) increaseCount();
            newEntry = Entry{key, value, hash};

            if (maxIndex == size_max)
                maxIndex = index;
            else
                maxIndex = (maxIndex > index ? maxIndex : index);
        }

        Value* get(param_key_type key)
        {
            if (count == 0) return nullptr;

            EKV& entry{findSlot(key)};
            if (entry.state != EntryState::Valid)
                return nullptr;
            else
                return &(entry.value);
        }

        const Value* get(param_key_type key) const
        {
            if (count == 0) return nullptr;

            const EKV& entry{findSlot(key)};
            if (entry.state != EntryState::Valid)
                return nullptr;
            else
                return &(entry.value);
        }

        void set(param_key_type key, param_value_type value)
        {
            EKV& entry{findSlot(key)};
            if (entry.state != EntryState::Valid)
                add(key, value);
            else
                entry.value = value;
        }

        bool contains(param_key_type key) const
        {
            if (count == 0) return false;

            const EKV& entry{findSlot(key)};
            return (entry.state == EntryState::Valid);
        }

        void remove(param_key_type key)
        {
            EKV& entry{findSlot(key)};
            if (entry.state == EntryState::Valid) // Leave it if it's already empty.
            {
                entry.state = EntryState::Tombstone;
                count--;
            }
        }

        void merge(const LinearTable& other)
        {
            for (const auto& [key, value] : other)
                this->add(key, value);
        }

        bool empty() const
        {
            return (count == 0);
        }

        size_type size() const
        {
            return count;
        }

        void clear()
        {
            dealloc(this->entries, capacity);
            entries = nullptr;
            count = 0;
            used = 0;
            capacity = 0;
            maxIndex = size_max;
        }

        // For debugging.
        void printTable()
        {
            for (size_type i = 0; i < capacity; i++)
            {
                std::cout << "Slot " << i << ": ";
                EKV& entry{entries[i]};
                if (entry.state == EntryState::Empty)
                    std::cout << "EMPTY\n";
                else
                {
                    std::cout << "(" << entry.key << ", "
                        << entry.value << ")";
                    if (entry.state == EntryState::Tombstone)
                        std::cout << " (TOMB)\n";
                    else
                        std::cout << '\n';
                }
            }
        }

        class iterator
        {
            using IterPair = Pair<Key, Value, EKV>;

            private:
                EKV* ptr{};
                EKV* end{};
                IterPair pair{};

            public:
                iterator() = default;

                iterator(EKV* ptr, EKV* end) :
                    ptr{ptr}, end{end}, pair{ptr} {}

                iterator(const iterator& other) :
                    ptr{other.ptr}, end{other.end}, pair{other.pair} {}

                iterator& operator=(const iterator& other)
                {
                    if (this != &other)
                    {
                        this->ptr = other.ptr;
                        this->end = other.end;
                        this->pair = other.pair;
                    }

                    return *this;
                }

                IterPair& operator*()
                {
                    return pair;
                }

                IterPair* operator->()
                {
                    return &pair;
                }

                iterator& operator++()
                {
                    ++ptr;
                    while ((ptr != end) && (ptr->state != EntryState::Valid))
                        ++ptr;
                    pair = IterPair{ptr};
                    return *this;
                }

                iterator& operator++(int)
                {
                    ptr++;
                    while ((ptr != end) && (ptr->state != EntryState::Valid))
                        ptr++;
                    pair = IterPair{ptr};
                    return *this;
                }

                bool operator==(const iterator& other) const
                {
                    return (this->ptr == other.ptr);
                }

                bool operator!=(const iterator& other) const
                {
                    return (this->ptr != other.ptr);
                }
        };

        class const_iterator
        {
            using IterPair = Pair<Key, Value, const EKV>;

            private:
                const EKV* ptr{};
                const EKV* end{};
                IterPair pair{};

            public:
                const_iterator() = default;

                const_iterator(const EKV* ptr, const EKV* end) :
                    ptr{ptr}, end{end}, pair{ptr} {}

                const_iterator(const const_iterator& other) :
                    ptr{other.ptr}, end{other.end}, pair{other.pair} {}

                const_iterator& operator=(const const_iterator& other)
                {
                    if (this != &other)
                    {
                        this->ptr = other.ptr;
                        this->end = other.end;
                        this->pair = other.pair;
                    }

                    return *this;
                }

                const IterPair& operator*() const
                {
                    return pair;
                }

                const IterPair* operator->() const
                {
                    return &pair;
                }

                const_iterator& operator++()
                {
                    ++ptr;
                    while ((ptr != end) && (ptr->state != EntryState::Valid))
                        ++ptr;
                    pair = IterPair{ptr};
                    return *this;
                }

                const_iterator& operator++(int)
                {
                    ptr++;
                    while ((ptr != end) && (ptr->state != EntryState::Valid))
                        ptr++;
                    pair = IterPair{ptr};
                    return *this;
                }

                bool operator==(const const_iterator& other) const
                {
                    return (this->ptr == other.ptr);
                }

                bool operator!=(const const_iterator& other) const
                {
                    return (this->ptr != other.ptr);
                }
        };

        [[nodiscard]] iterator begin() noexcept
        {
            EKV* ptr{entries};
            EKV* end{entries + capacity};

            while ((ptr != end) && (ptr->state != EntryState::Valid))
                ptr++;
            return iterator{ptr, end};
        }

        [[nodiscard]] iterator end() noexcept
        {
            EKV* end{entries + capacity};
            return iterator{end, end};
        }

        [[nodiscard]] const_iterator begin() const noexcept
        {
            return cbegin();
        }

        [[nodiscard]] const_iterator end() const noexcept
        {
            return cend();
        }

        [[nodiscard]] const_iterator cbegin() const noexcept
        {
            const EKV* ptr{entries};
            const EKV* end{entries + capacity};

            while ((ptr != end) && (ptr->state != EntryState::Valid))
                ptr++;
            return const_iterator{ptr, end};
        }

        [[nodiscard]] const_iterator cend() const noexcept
        {
            const EKV* end{entries + capacity};
            return const_iterator{end, end};
        }
};

template<typename Key, typename Value, typename HashFunc, typename Alloc>
Alloc LinearTable<Key, Value, HashFunc, Alloc>::allocator{};

template<typename Key, typename Value, typename HashFunc, typename Alloc>
HashFunc LinearTable<Key, Value, HashFunc, Alloc>::getHash{};