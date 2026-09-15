#pragma once
#include "common.h"
#include "generics.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <type_traits>

template<typename T, typename Alloc = std::allocator<T>>
class Array
{
    // static_assert(
    //     std::is_default_constructible_v<T>
    //     && std::is_nothrow_default_constructible_v<T>,
    //     "Array element type must be default-constructible without throwing."
    // );

    // static_assert(
    //     std::is_copy_assignable_v<T>
    //     && std::is_nothrow_copy_assignable_v<T>,
    //     "Array element type must be copy-assignable without throwing."
    // );

    // static_assert(
    //     std::is_default_constructible_v<Alloc>
    //     && std::is_nothrow_default_constructible_v<Alloc>,
    //     "Allocator type must be default-constructible without throwing."
    // );

    private:
        using signed_type = std::int64_t;
        using param_type = std::conditional_t<
            sizeof(T) <= 8,
            T,
            const T&
        >;

        static Alloc allocator;
        T* entries{nullptr};
        size_type _count{0};
        size_type _capacity{0};

        static constexpr size_type defaultSize{8};
        static constexpr size_type growFactor{2};

        static T* alloc(size_type count)
        {
            T* mem{allocator.allocate(count)};
            std::uninitialized_fill_n(mem, count, T());
            return mem;
        }

        static void dealloc(T* mem, size_type count)
        {
            std::destroy_n(mem, count);
            allocator.deallocate(mem, count);
        }

        // Shift cannot be larger than end - start.
        void copy(T* dest, size_type start, size_type end, signed_type shift = 0)
        {
            if ((entries == nullptr) || (end - start > _count))
                return;

            if constexpr (std::is_trivially_copyable_v<T>)
            {
                std::memcpy(
                    dest + start + shift,
                    entries + start,
                    (end - start) * sizeof(T)
                );
            }
            else if constexpr (std::is_nothrow_move_assignable_v<T>)
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

        // When using this function, any unshifted buffer area
        // in the array must be replaced/initialized immediately
        // to maintain the contiguous storage of elements in the array.
        void shift(signed_type shift, size_type start = 0)
        {
            if ((shift == 0) || (_count == 0)) // Nothing to do.
                return;

            // We allow the shift to be negative for element removal.
            // Just need to be careful when using that internally.

            // Can't be too negative, though.
            if (shift < (-1 * static_cast<signed_type>(_count)))
                return; // Throw error?

            if (start >= _count)
                return; // Throw error?

            size_type oldCapacity{_capacity};
            resize(_count + shift);

            // To avoid data corruption, we make a new internal array.
            // Note: we don't use an Array<T, alloc> local variable
            // since its destructor (called when it goes out of scope
            // by the end of this function) will deallocate the memory
            // we just "filled up".

            T* newEntries{alloc(_capacity)};
            copy(newEntries, 0, start);
            copy(newEntries, start, _count, shift);
            dealloc(this->entries, oldCapacity);
            this->entries = newEntries;

            // Only increment count when we fill the empty spots.
        }

        void grow()
        {
            size_type oldCapacity{_capacity};
            _capacity = (_capacity == 0 ? defaultSize : _capacity * growFactor);
            T* newEntries{alloc(_capacity)};
            copy(newEntries, 0, _count);
            dealloc(entries, oldCapacity);
            entries = newEntries;
        }

    public:
        /* Constructors and assignment operators. */

        Array() = default;

        Array(size_type size) :
            entries{alloc(size)}, _capacity{size} {}

        Array(const Array& other) noexcept :
            entries{alloc(other._capacity)}, _count{other._count},
            _capacity{other._capacity}
        {
            for (size_type i{0}; i < other._count; i++)
                this->entries[i] = other.entries[i];
        }

        Array(Array&& other) noexcept :
            entries{other.entries}, _count{other._count},
            _capacity{other._capacity}
        {
            other.entries = nullptr;
            other._count = 0;
            other._capacity = 0;
        }

        Array& operator=(const Array& other) noexcept
        {
            if (this != &other)
            {
                dealloc(this->entries, _capacity);
                this->entries = alloc(other._capacity);

                for (size_type i{0}; i < other._capacity; i++)
                    this->entries[i] = other.entries[i];

                this->_count = other._count;
                this->_capacity = other._capacity;
            }

            return *this;
        }

        Array& operator=(Array&& other) noexcept
        {
            if (this != &other)
            {
                dealloc(this->entries, _capacity);

                this->entries = other.entries;
                this->_count = other._count;
                this->_capacity = other._capacity;

                other.entries = nullptr;
                other._count = 0;
                other._capacity = 0;
            }

            return *this;
        }

        ~Array()
        {
            clear();
        }


        /* Basic operators. */

        inline T& operator[](size_type index)
        {
            if (index >= _count)
                throw std::out_of_range("Index out of range.");

            return entries[index];
        }

        inline const T& operator[](size_type index) const
        {
            if (index >= _count)
                throw std::out_of_range("Index out of range.");

            return entries[index];
        }

        bool operator==(const Array& other) const
        {
            if (this->_count != other._count) return false;

            for (size_type i{0}; i < this->_count; i++)
            {
                if (this->entries[i] != other.entries[i])
                    return false;
            }

            return true;
        }


        /* Modification. */

        void resize(size_type size)
        {
            while (_capacity < size)
                grow();
        }

        void push(param_type element)
        {
            if (_capacity <= _count)
                grow();
            entries[_count++] = element;
        }

        template<
            typename... Args,
            typename = std::enable_if_t<(std::is_same_v<T, Args> || ...)>
        >
        void push_range(Args... args)
        {
            for (auto arg : {args...})
                push(arg);
        }

        template<typename... Args>
        void emplace(Args... args)
        {
            static_assert(
                std::is_constructible_v<T, Args...>,
                "Object cannot be constructed from given argument list."
            );

            if (_capacity <= _count)
                grow();
            entries[_count++] = T(std::forward<T>(args)...);
        }

        void insert(param_type element, size_type index)
        {
            if (index >= _count)
                throw std::out_of_range("Index out of range.");

            if (_capacity <= _count)
                grow();

            shift(1, index);
            entries[index] = element;
            _count++;
        }

        T erase(size_type index)
        {
            if (index >= _count)
                throw std::out_of_range("Index out of range.");

            T element = entries[index];
            // Shift begins at the index we pass to shift().
            // We want to move every element *after*
            // the parameter index (here) back, so we add 1.
            shift(-1, index + 1);
            _count--;
            return element;
        }

        void remove(param_type element)
        {
            signed_type index{find(element)};
            if (index == -1) return;
            erase(index);
        }

        inline T pop()
        {
            _count--;
            return entries[_count];
        }

        inline void popn(size_type n)
        {
            _count -= n;
        }

        inline void clear()
        {
            _count = 0;
            _capacity = 0;
            dealloc(this->entries, _capacity);
            entries = nullptr; // In case clear() is called again.
        }


        /* Search. */

        signed_type find(param_type element, bool sorted = false) const
        {
            static_assert(has_equal_v<T>, "Type is not comparable.");
            auto defaultEquality = [&](param_type other) -> bool {
                return element == other;
            };

            if constexpr (can_compare_v<T>)
            {
                if (sorted)
                {
                    if (_count == 0) return -1;

                    size_type min{0}, max{_count - 1};
                    while (min <= max)
                    {
                        size_type mid{min + (max - min) / 2};
                        if (entries[mid] == element)
                            return static_cast<signed_type>(mid);
                        else if (entries[mid] < element)
                            min = mid + 1;
                        else
                            max = mid - 1;
                    }

                    return -1;
                }
                else
                    return find_first_if(defaultEquality);
            }
            else
                return find_first_if(defaultEquality);
        }

        template<typename Pred>
        signed_type find_first_if(Pred p) const
        {
            for (size_type i{0}; i < _count; i++)
            {
                if (p(entries[i]))
                    return static_cast<signed_type>(i);
            }

            return -1;
        }

        template<typename Pred>
        signed_type find_last_if(Pred p) const
        {
            for (size_type i{0}; i < _count; i++)
            {
                if (p(entries[_count - i - 1]))
                    return static_cast<signed_type>(i);
            }

            return -1;
        }

        void fill(param_type object, size_type count = size_max)
        {
            if (count == size_max) count = _count;

            for (size_type i{0}; i < count; i++)
                entries[i] = object;
        }


        /* Getters. */

        [[nodiscard]] inline size_type count() const
        {
            return _count;
        }

        [[nodiscard]] inline size_type capacity() const
        {
            return _capacity;
        }

        [[nodiscard]] inline bool empty() const
        {
            return (_count == 0);
        }

        [[nodiscard]] inline T& front()
        {
            return entries[0];
        }

        [[nodiscard]] inline const T& front() const
        {
            return entries[0];
        }

        [[nodiscard]] inline T& back()
        {
            return entries[_count - 1];
        }

        [[nodiscard]] inline const T& back() const
        {
            return entries[_count - 1];
        }


        /* Iterators. */

        using iter_diff_type = int64_t;

        class iterator
        {
            private:
                T* ptr;

            public:
                iterator() = default;
                iterator(T* ptr) : ptr{ptr} {}
                iterator(const iterator& other) : ptr{other.ptr} {}

                iterator& operator=(const iterator& other)
                {
                    if (this != &other)
                        this->ptr = other.ptr;
                    return *this;
                }

                T& operator*() const
                {
                    return *ptr;
                }

                T* operator->() const
                {
                    return ptr;
                }

                iterator operator+(iter_diff_type n)
                {
                    return iterator{ptr + n};
                }

                iterator operator-(iter_diff_type n)
                {
                    return iterator{ptr - n};
                }

                iterator& operator+=(iter_diff_type n)
                {
                    ptr += n;
                    return *this;
                }

                iterator& operator-=(iter_diff_type n)
                {
                    ptr -= n;
                    return *this;
                }

                iterator& operator++()
                {
                    ++ptr;
                    return *this;
                }

                iterator operator++(int)
                {
                    iterator temp{*this};
                    ptr++;
                    return temp;
                }

                iterator& operator--()
                {
                    --ptr;
                    return *this;
                }

                iterator operator--(int)
                {
                    iterator temp{*this};
                    ptr--;
                    return temp;
                }

                iter_diff_type operator-(const iterator& iter)
                {
                    return (this->ptr - iter.ptr);
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
            private:
                const T* ptr;

            public:
                const_iterator() = default;
                const_iterator(T* ptr) : ptr{ptr} {}
                const_iterator(const const_iterator& other) : ptr{other.ptr} {}

                const_iterator& operator=(const const_iterator& other)
                {
                    if (this != &other)
                        this->ptr = other.ptr;
                    return *this;
                }

                const T& operator*() const
                {
                    return *ptr;
                }

                const T* operator->() const
                {
                    return ptr;
                }

                const_iterator operator+(iter_diff_type n)
                {
                    return const_iterator{ptr + n};
                }

                const_iterator operator-(iter_diff_type n)
                {
                    return const_iterator{ptr - n};
                }

                const_iterator& operator+=(iter_diff_type n)
                {
                    ptr += n;
                    return *this;
                }

                const_iterator& operator-=(iter_diff_type n)
                {
                    ptr -= n;
                    return *this;
                }

                const_iterator& operator++()
                {
                    ++ptr;
                    return *this;
                }

                const_iterator operator++(int)
                {
                    const_iterator temp{*this};
                    ptr++;
                    return temp;
                }

                const_iterator& operator--()
                {
                    --ptr;
                    return *this;
                }

                const_iterator operator--(int)
                {
                    const_iterator temp{*this};
                    ptr--;
                    return temp;
                }

                iter_diff_type operator-(const const_iterator& iter)
                {
                    return (this->ptr - iter.ptr);
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


        /* Iterator factory methods. */

        [[nodiscard]] iterator begin() noexcept
        {
            return iterator{entries};
        }

        [[nodiscard]] iterator end() noexcept
        {
            return iterator{entries + _count};
        }

        [[nodiscard]] const_iterator begin() const noexcept
        {
            return const_iterator{entries};
        }

        [[nodiscard]] const_iterator end() const noexcept
        {
            return const_iterator{entries + _count};
        }

        [[nodiscard]] const_iterator cbegin() const noexcept
        {
            return const_iterator{entries};
        }

        [[nodiscard]] const_iterator cend() const noexcept
        {
            return const_iterator{entries + _count};
        }
};

template<typename T, typename Alloc>
Alloc Array<T, Alloc>::allocator{};