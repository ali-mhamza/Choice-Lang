/*  This hash table implementation uses a basic separate chaining approach for hash collisions.
*   Each bucket contains a (non-intrusive/regular) linked list (implemented in this project), which is
*   traversed to search for a key match upon a collision occurring.
*/

#pragma once
#include "entry_struct.h"
#include "hash_functions.h"
#include "linked_list.h"
#include "table_error.h"
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>

#define EKVList LinkedList<EKV>

template<typename Key, typename Value>
using ChainDefaultAlloc = std::allocator<EKVList>;

template<
    typename Key,
    typename Value,
    typename HashFunc = Hasher<Key>,
    typename Alloc = ChainDefaultAlloc<Key, Value>
>
class ChainTable
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

        static Alloc allocator;
        static HashFunc getHash;
        static constexpr size_type defaultSize{2};
        static constexpr size_type growFactor{2};
        static constexpr double loadFactor{TABLE_LOAD_FACTOR};

        EKVList* entries{nullptr};
        // Number of key-value pairs we have in the table.
        size_type count{0};
        // Number of total buckets in the array.
        size_type capacity{0};
        // Will mark how far into the array we have entries to copy.
        size_type maxIndex{size_max};

        ChainTable(size_type size) :
            entries{alloc(size)}, capacity{size} {}

        static EKVList* alloc(size_type count)
        {
            EKVList* mem{allocator.allocate(count)};
            std::uninitialized_fill_n(mem, count, EKVList{});
            return mem;
        }

        static void dealloc(EKVList* mem, size_type count)
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

        void grow()
        {
            dealloc(this->entries, capacity);
            capacity *= growFactor;
            EKVList* newEntries{alloc(capacity)};
            this->entries = newEntries;
        }

        void reorder()
        {
            size_type newCapacity{capacity * growFactor};
            // Easier to just construct a new table.
            ChainTable<Key, Value, HashFunc> newTable{newCapacity};
            for (size_type i{0}; i <= maxIndex; i++)
            {
                EKVList& list{entries[i]};
                // Handles empty lists as well.
                for (auto* ptr{list.front()}; ptr != nullptr; ptr = ptr->next)
                {
                    EKV& entry{ptr->object};
                    newTable.add(entry.key, entry.value);
                }
            }

            *this = std::move(newTable);
        }

        void resize()
        {
            if ((capacity * loadFactor) < count + 1)
                count == 0 ? grow() : reorder();
        }

        // Adds a key with no value.
        EKV& emptyAdd(param_key_type key)
        {
            // This method is only called internally, so we can skip
            // checks for the key existing while being careful to use
            // the method properly.

            resize(); // Grow if needed.

            Hash hash{getHash(key)};
            size_type index{hash & (capacity - 1)};
            if (maxIndex == size_max)
                maxIndex = index;
            else
                maxIndex = (index > maxIndex) ? index : maxIndex;

            EKV entry{EKV::hashEntry(key, hash)};
            EKVList& list{entries[index]};
            list.append(entry);
            count++;
            return list.at(list.length() - 1)->object;
        }

        EKV* getEntry(param_key_type key)
        {
            if (count == 0) return nullptr;

            Hash hash{getHash(key)};
            size_type index{hash & (capacity - 1)};
            EKVList& list{entries[index]};

            EKV temp{EKV::hashEntry(key, hash)};
            auto* node{list.get(temp)};
            if (node == nullptr) return nullptr;
            return &(node->object);
        }

        const EKV* getEntry(param_key_type key) const
        {
            if (count == 0) return nullptr;

            Hash hash{getHash(key)};
            size_type index{hash & (capacity - 1)};
            const EKVList& list{entries[index]};

            EKV temp{EKV::hashEntry(key, hash)};
            const auto* node{list.get(temp)};
            if (node == nullptr) return nullptr;
            return &(node->object);
        }

    public:
        ChainTable() : ChainTable{defaultSize} {}

        ChainTable(const ChainTable& other) : ChainTable{defaultSize}
        {
            merge(other);
        }

        ChainTable& operator=(const ChainTable& other)
        {
            if (this != &other)
            {
                clear();
                init();
                merge(other);
            }

            return *this;
        }

        ChainTable(ChainTable&& other) :
            entries{other.entries}, count{other.count},
            capacity{other.capacity}, maxIndex{other.maxIndex}
        {
            other.entries = nullptr;
            other.count = 0;
            other.capacity = 0;
            other.maxIndex = 0;
        }

        ChainTable& operator=(ChainTable&& other)
        {
            if (this != &other)
            {
                clear();

                this->entries = other.entries;
                this->count = other.count;
                this->capacity = other.capacity;
                this->maxIndex = other.maxIndex;

                other.entries = nullptr;
                other.count = 0;
                other.capacity = 0;
                other.maxIndex = 0;
            }

            return *this;
        }

        ~ChainTable()
        {
            clear();
        }

        Value& operator[](param_key_type key)
        {
            EKV* entry{getEntry(key)};
            if (entry != nullptr) return entry->value;
            return emptyAdd(key).value;
        }

        const Value& operator[](param_key_type key) const
        {
            const EKV* entry{getEntry(key)};
            if (entry != nullptr) return entry->value;
            throw EntryNotFoundError{};
        }

        void add(param_key_type key, param_value_type value)
        {
            EKV* temp{getEntry(key)};
            if (temp != nullptr) // Key already exists.
            {
                temp->value = value;
                return;
            }

            resize(); // Grow if needed.

            Hash hash{getHash(key)};
            size_type index{hash & (capacity - 1)};

            if (maxIndex == size_max)
                maxIndex = index;
            else
                maxIndex = (index > maxIndex) ? index : maxIndex;

            EKVList& list{entries[index]};
            EKV entry{key, value, hash};
            list.append(entry);
            count++;
        }

        Value* get(param_key_type key)
        {
            EKV* entry{getEntry(key)};

            if (entry == nullptr) return nullptr;
            return &(entry->value);
        }

        const Value* get(param_key_type key) const
        {
            const EKV* entry{getEntry(key)};

            if (entry == nullptr) return nullptr;
            return &(entry->value);
        }

        void set(param_key_type key, param_value_type value)
        {
            EKV* entry{getEntry(key)};

            if (entry == nullptr) // Key does not exist.
                add(key, value);
            else
                entry->value = value;
        }

        bool contains(param_key_type key) const
        {
            return ((count != 0) && (getEntry(key) != nullptr));
        }

        void remove(param_key_type key)
        {
            if (count == 0) return;

            Hash hash{getHash(key)};
            size_type index{hash & (capacity - 1)};
            EKVList& list{entries[index]};

            EKV temp{EKV::hashEntry(key, hash)};
            if (!list.has(temp)) return;

            list.remove(temp);
            count--;
        }

        void merge(const ChainTable& other)
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
            dealloc(entries, capacity);
            entries = nullptr;
            count = 0;
            maxIndex = size_max;
        }

        void printTable()
        {
            for (size_type i{0}; i < capacity; i++)
            {
                EKVList& list{entries[i]};
                if (list.front() == nullptr)
                    std::cout << "Slot " << i << ": EMPTY\n";
                else
                {
                    std::cout << "Slot " << i << ": ";
                    for (auto& pair : list)
                        std::cout << "(" << pair.key << ", " << pair.value << ")->";
                    std::cout << '\n';
                }
            }
        }

        // Iterates across buckets.
        class iterator
        {
            using IterPair = Pair<Key, Value, EKV>;

            private:
                EKVList* bucket{};
                EKVList* last{};
                ListNode<EKV>* ptr{};
                ListNode<EKV>* end{};
                IterPair pair{};

            public:
                iterator() = default;

                iterator(EKVList* bucket, EKVList* last) :
                    bucket{bucket}, last{last}
                {
                    if (bucket != last)
                    {
                        ptr = bucket->front();
                        end = bucket->back();
                        pair = IterPair{&(ptr->object)};
                    }
                }

                iterator(const iterator& other) :
                    bucket{other.bucket}, last{other.last}, ptr{other.ptr},
                    end{other.end}, pair{other.pair} {}

                iterator& operator=(const iterator& other)
                {
                    if (this != &other)
                    {
                        this->bucket = other.bucket;
                        this->last = other.last;
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
                    if (ptr != nullptr)
                    {
                        ptr = ptr->next;
                        while ((ptr == nullptr) && (bucket != last))
                        {
                            if (++bucket != last)
                                ptr = bucket->front();
                        }

                        if (ptr != nullptr) pair = IterPair{&(ptr->object)};
                    }

                    return *this;
                }

                iterator operator++(int)
                {
                    iterator temp{*this};
                    if (ptr != nullptr)
                    {
                        ptr = ptr->next;
                        while ((ptr == nullptr) && (bucket != last))
                        {
                            if (++bucket != last)
                                ptr = bucket->front();
                        }

                        if (ptr != nullptr) pair = IterPair{&(ptr->object)};
                    }

                    return temp;
                }

                bool operator==(const iterator& other)
                {
                    return (this->ptr == other.ptr);
                }

                bool operator!=(const iterator& other)
                {
                    return (this->ptr != other.ptr);
                }
        };

        // Iterates across elements of a single bucket.
        class local_iterator
        {
            using IterPair = Pair<Key, Value, EKV>;

            private:
                ListNode<EKV>* node{};
                IterPair pair{};

            public:
                local_iterator() = default;

                local_iterator(ListNode<EKV>* node) :
                    node{node}
                {
                    if (node != nullptr)
                        pair = IterPair{&(node->object)};
                }

                local_iterator(const local_iterator& other) :
                    node{other.node}, pair{other.pair} {}

                local_iterator& operator=(const local_iterator& other)
                {
                    if (this != &other)
                    {
                        this->node = other.node;
                        this->pair = other.pair;
                        this->bucketIndex = other.bucketIndex;
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

                local_iterator& operator++()
                {
                    if (node != nullptr)
                    {
                        node = node->next;
                        if (node != nullptr) pair = IterPair{&(node->object)};
                    }

                    return *this;
                }

                local_iterator operator++(int)
                {
                    local_iterator temp{*this};
                    if (node != nullptr)
                    {
                        node = node->next;
                        if (node != nullptr) pair = IterPair{&(node->object)};
                    }

                    return temp;
                }

                bool operator==(const local_iterator& other)
                {
                    return (this->node = other.node);
                }

                bool operator!=(const local_iterator& other)
                {
                    return (this->node != other.node);
                }
        };

        class const_iterator
        {
            using IterPair = Pair<Key, Value, const EKV>;

            private:
                const EKVList* bucket{};
                const EKVList* last{};
                const ListNode<EKV>* ptr{};
                const ListNode<EKV>* end{};
                IterPair pair{};

            public:
                const_iterator(const EKVList* bucket, const EKVList* last) :
                    bucket{bucket}, last{last}
                {
                    if (bucket != last)
                    {
                        ptr = bucket->front();
                        end = bucket->back();
                        pair = IterPair{&(ptr->object)};
                    }
                }

                const_iterator(const const_iterator& other) :
                    bucket{other.bucket}, last{other.last}, ptr{other.ptr},
                    end{other.end}, pair{other.pair} {}

                const_iterator& operator=(const const_iterator& other)
                {
                    if (this != &other)
                    {
                        this->bucket = other.bucket;
                        this->last = other.last;
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
                    if (ptr != nullptr)
                    {
                        ptr = ptr->next;
                        while ((ptr == nullptr) && (bucket != last))
                        {
                            if (++bucket != last)
                                ptr = bucket->front();
                        }

                        if (ptr != nullptr) pair = IterPair{&(ptr->object)};
                    }

                    return *this;
                }

                const_iterator operator++(int)
                {
                    const_iterator temp{*this};
                    if (ptr != nullptr)
                    {
                        ptr = ptr->next;
                        while ((ptr == nullptr) && (bucket != last))
                        {
                            if (++bucket != last)
                                ptr = bucket->front();
                        }

                        if (ptr != nullptr) pair = IterPair{&(ptr->object)};
                    }

                    return temp;
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

        class const_local_iterator
        {
            using IterPair = Pair<Key, Value, const EKV>;

            private:
                const ListNode<EKV>* node{};
                IterPair pair{};

            public:
                const_local_iterator(const ListNode<EKV>* node) :
                    node{node}
                {
                    if (node != nullptr)
                        pair = IterPair{&(node->object)};
                }

                const_local_iterator(const const_local_iterator& other) :
                    node{other.node}, pair{other.pair} {}

                const_local_iterator& operator=(const const_local_iterator& other)
                {
                    if (this != &other)
                    {
                        this->node = other.node;
                        this->pair = other.pair;
                        this->bucketIndex = other.bucketIndex;
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

                const_local_iterator& operator++()
                {
                    if (node != nullptr)
                    {
                        node = node->next;
                        if (node != nullptr) pair = IterPair{&(node->object)};
                    }

                    return *this;
                }

                const_local_iterator operator++(int)
                {
                    const_local_iterator temp{*this};
                    if (node != nullptr)
                    {
                        node = node->next;
                        if (node != nullptr) pair = IterPair{&(node->object)};
                    }

                    return temp;
                }

                bool operator==(const const_local_iterator& other) const
                {
                    return (this->node == other.node);
                }

                bool operator!=(const const_local_iterator& other) const
                {
                    return (this->node != other.node);
                }
        };

        // Iterator to first entry in first non-empty bucket.

        iterator begin()
        {
            EKVList* ptr{entries};
            EKVList* end{entries + capacity};

            while ((ptr != end) && (ptr->front() == nullptr))
                ptr++;
            return iterator{ptr, end};
        }

        iterator end()
        {
            EKVList* end{entries + capacity};
            return iterator{end, end};
        }

        const_iterator begin() const
        {
            return cbegin();
        }

        const_iterator end() const
        {
            return cend();
        }

        const_iterator cbegin() const
        {
            const EKVList* ptr{entries};
            const EKVList* end{entries + capacity};

            while ((ptr != end) && (ptr->front() == nullptr))
                ptr++;
            return const_iterator{ptr, end};
        }

        const_iterator cend() const
        {
            const EKVList* end{entries + capacity};
            return const_iterator{end, end};
        }

        // Iterators within specified bucket.

        local_iterator begin(size_type n)
        {
            if (n >= capacity)
                throw std::out_of_range("Bucket index outside table range.");

            return local_iterator{entries[n].front()};
        }

        local_iterator end(size_type n)
        {
            if (n >= capacity)
                throw std::out_of_range("Bucket index outside table range.");

            return local_iterator{entries[n].back()};
        }

        const_local_iterator begin(size_type n) const
        {
            return cbegin(n);
        }

        const_local_iterator end(size_type n) const
        {
            return cend(n);
        }

        const_local_iterator cbegin(size_type n) const
        {
            if (n >= capacity)
                throw std::out_of_range("Bucket index outside table range.");

            return const_local_iterator{entries[n].front()};
        }

        const_local_iterator cend(size_type n) const
        {
            if (n >= capacity)
                throw std::out_of_range("Bucket index outside table range.");

            return const_local_iterator{entries[n].back()};
        }
};

template<typename Key, typename Value, typename HashFunc, typename Alloc>
Alloc ChainTable<Key, Value, HashFunc, Alloc>::allocator;

template<typename Key, typename Value, typename HashFunc, typename Alloc>
HashFunc ChainTable<Key, Value, HashFunc, Alloc>::getHash;

#undef EKVList