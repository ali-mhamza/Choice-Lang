#pragma once
#include <cstddef>
#include <stdexcept>

#define TEMP template<typename T>

TEMP
class Array
{
    private:
        T* entries;
        size_t _count;
        size_t _capacity;

        void shift(int shift, size_t start = 0);

    public:
        // Constructing Array objects.
        Array();
        Array(size_t size);
        Array(const Array& other);
        Array(Array&& other);
        Array& operator=(const Array& other);
        Array& operator=(Array&& other);
        ~Array();

        // Basic operators.
        inline T& operator[](size_t index); // index < count.
        inline const T& operator[](size_t index) const; // index < count.
        bool operator==(const Array& other) const;

        // Utility.
        void grow();
        // This does not reallocate to a larger array.
        // It simply increases the capacity field as *if*
        // we were actually growing the array.
        // Use with care.
        inline void increaseCapacity();

        void push(const T& element);
        int position(const T& element);
        void insert(const T& element, size_t index);
        T erase(size_t index);
        void remove(const T& element);
        inline T pop();
        inline void popn(size_t n);

        inline size_t count() const;
        inline size_t capacity() const;
        inline bool empty() const;

        inline T* front();
        inline const T* front() const;
        inline T* back();
        inline const T* back() const;

        inline T& slot(size_t index); // index < capacity.
        void slotInsert(const T& element, size_t index); // index < capacity.
        void fillArray(const T& element, bool capacity = false);

        class iterator
        {
            private:
                T* ptr;

            public:
                iterator() = default;
                iterator(T* ptr);
                iterator(const iterator& other);
                iterator& operator=(const iterator& other);

                T& operator*() const;
                T* operator->() const;
                iterator& operator++();
                iterator operator++(int);
                iterator& operator--();
                iterator operator--(int);
                bool operator==(const iterator& other);
                bool operator!=(const iterator& other);
        };

        class const_iterator
        {
            private:
                const T* ptr;

            public:
                const_iterator() = default;
                const_iterator(const T* ptr);
                const_iterator(const const_iterator& other);
                const_iterator& operator=(const const_iterator& other);

                const T& operator*() const;
                const T* operator->() const;
                const_iterator& operator++();
                const_iterator operator++(int);
                const_iterator& operator--();
                const_iterator operator--(int);
                bool operator==(const const_iterator& other) const;
                bool operator!=(const const_iterator& other) const;
        };

        iterator begin() noexcept;
        iterator end() noexcept;
        const_iterator begin() const noexcept;
        const_iterator end() const noexcept;
        const_iterator cbegin() const noexcept;
        const_iterator cend() const noexcept;
};

TEMP
Array<T>::Array() :
    entries(nullptr), _count(0),
    _capacity(0) {}

TEMP
Array<T>::Array(size_t size)
{
    entries = new T[size];
    _count = 0; // No elements used at time of construction.
    _capacity = size;
}

TEMP
Array<T>::Array(const Array<T>& other) :
    entries(new T[other._capacity]), _count(other._count),
    _capacity(other._capacity)
{
    for (size_t i = 0; i < other._capacity; i++)
        this->entries[i] = other.entries[i];
}

TEMP
Array<T>::Array(Array<T>&& other) :
    entries(other.entries), _count(other._count),
    _capacity(other._capacity)
{
    other.entries = nullptr;
    other._count = 0;
    other._capacity = 0;
}

TEMP
Array<T>& Array<T>::operator=(const Array<T>& other)
{
    if (this != &other)
    {
        delete[] this->entries;
        this->entries = new T[other._capacity];

        for (size_t i = 0; i < other._capacity; i++)
            this->entries[i] = other.entries[i];

        this->_count = other._count;
        this->_capacity = other._capacity;
    }

    return *this;
}

TEMP
Array<T>& Array<T>::operator=(Array<T>&& other)
{
    if (this != &other)
    {
        delete[] this->entries;

        this->entries = other.entries;
        this->_count = other._count;
        this->_capacity = other._capacity;

        other.entries = nullptr;
        other._count = 0;
        other._capacity = 0;
    }

    return *this;
}

TEMP
Array<T>::~Array()
{
    _count = 0;
    _capacity = 0;
    delete[] entries;
}

TEMP
inline T& Array<T>::operator[](size_t index)
{
    if (index >= _count)
        throw std::out_of_range("Index out of range.");

    return entries[index];
}

TEMP
inline const T& Array<T>::operator[](size_t index) const
{
    if (index >= _count)
        throw std::out_of_range("Index out of range.");

    return entries[index];
}

TEMP
bool Array<T>::operator==(const Array<T>& other) const
{
    if (this->_count != other._count) return false;

    for (size_t i = 0; i < this->_count; i++)
    {
        if (this->entries[i] != other.entries[i])
            return false;
    }

    return true;
}

TEMP
void Array<T>::grow()
{
    _capacity = (_capacity == 0 ? 8 : _capacity * 2);
    T* newEntries = new T[_capacity];
    for (size_t i = 0; i < _count; i++)
        newEntries[i] = std::move(entries[i]);
    delete[] entries;
    this->entries = newEntries;
    // this->count does not change.
    // newEntries goes out of scope here.
}

TEMP
inline void Array<T>::increaseCapacity()
{
    _capacity = (_capacity == 0 ? 8 : _capacity * 2);
}

TEMP
void Array<T>::push(const T& element)
{
    if (_capacity <= _count)
        grow();
    entries[_count++] = element;
}

TEMP
int Array<T>::position(const T& element)
{
    for (size_t i = 0; i < _count; i++)
    {
        if (entries[i] == element)
            return (int) i;
    }

    return -1;
}

// When using this function, any unshifted
// buffer area in the array must be replaced/
// initialized immediately to maintain
// the contiguous storage of elements in the array.
TEMP
void Array<T>::shift(int shift, size_t start)
{
    if ((shift == 0) || (_count == 0)) // Nothing to do.
        return;

    // We allow the shift to be negative
    // for element removal.
    // Just need to be careful when using
    // that internally.

    // Can't be too negative, though.
    if (shift < (-1 * static_cast<int>(_count)))
        return; // Throw error?

    if (start >= _count)
        return; // Throw error?

    // Shift might be large, so we
    // repeatedly grow until capacity is enough.
    while (_capacity < _count + shift)
        grow();
    
    // To avoid data corruption, we make a
    // new internal array.
    // Note: we don't use an Array<T> local
    // variable since its destructor (called when
    // it goes out of scope by the end of this function)
    // will deallocate the memory we just "filled up".

    T* newEntries = new T[_capacity];
    for (int i = 0; i < start; i++)
        newEntries[i] = this->entries[i];
    
    for (int i = start; i < _count; i++)
        newEntries[i + shift] = this->entries[i];
    delete[] this->entries;
    this->entries = newEntries;
    // Only increment count when we fill the
    // empty spots.
}

TEMP
void Array<T>::insert(const T& element, size_t index)
{
    if (index >= _count)
        throw std::out_of_range("Index out of range.");
    
    if (_capacity <= _count)
        grow();

    shift(1, index);
    entries[index] = element;
    _count++;
}

TEMP
T Array<T>::erase(size_t index)
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

TEMP
void Array<T>::remove(const T& element)
{
    int index = position(element);
    if (index == -1)
        return;

    erase(index);
}

TEMP
inline T Array<T>::pop()
{
    _count--;
    return entries[_count];
}

TEMP
inline void Array<T>::popn(size_t n)
{
    _count -= n;
}

TEMP
inline size_t Array<T>::count() const
{
    return _count;
}

TEMP
inline size_t Array<T>::capacity() const
{
    return _capacity;
}

TEMP
inline bool Array<T>::empty() const
{
    return _count == 0;
}

TEMP
inline T* Array<T>::front()
{
    return entries;
}

TEMP
inline const T* Array<T>::front() const
{
    return entries;
}

TEMP
inline T* Array<T>::back()
{
    return entries + _count;
}

TEMP
inline const T* Array<T>::back() const
{
    return entries + _count;
}

TEMP
inline T& Array<T>::slot(size_t index)
{
    if (index >= _capacity)
        throw std::out_of_range("Index out of range.");
    return entries[index];
}

TEMP
void Array<T>::slotInsert(const T& element, size_t index)
{
    if (index >= _capacity)
        throw std::out_of_range("Index out of range.");
        
    if (_capacity <= _count)
        grow();

    shift(1, index);
    entries[index] = element;
    _count++;
}

TEMP
void Array<T>::fillArray(const T& object, bool capacity)
{
    for (size_t i = 0; i < _count; i++)
        entries[i] = object;
    if (capacity)
    {
        for (size_t i = _count; i < _capacity; i++)
            entries[i] = object;
    }
}

// Iterator implementation.

#define arrIter Array<T>::iterator

TEMP
arrIter::iterator(T* ptr) :
    ptr(ptr) {}

TEMP
arrIter::iterator(const arrIter& other) :
    ptr(other.ptr) {}

TEMP
typename arrIter& arrIter::operator=(const arrIter& other)
{
    if (this != &other)
        this->ptr = other.ptr;
    return *this;
}

TEMP
T& arrIter::operator*() const
{
    return *ptr;
}

TEMP
T* arrIter::operator->() const
{
    return ptr;
}

TEMP
typename arrIter& arrIter::operator++()
{
    ++ptr;
    return *this;
}

TEMP
typename arrIter arrIter::operator++(int n)
{
    (void) n;

    arrIter temp = *this;
    ptr++;
    return temp;
}

TEMP
typename arrIter& arrIter::operator--()
{
    --ptr;
    return *this;
}

TEMP
typename arrIter arrIter::operator--(int n)
{
    (void) n;

    arrIter temp = *this;
    ptr--;
    return temp;
}

TEMP
bool arrIter::operator==(const arrIter& other)
{
    return (this->ptr == other.ptr);
}

TEMP
bool arrIter::operator!=(const arrIter& other)
{
    return (this->ptr != other.ptr);
}

TEMP
typename arrIter Array<T>::begin() noexcept
{
    return iterator(front());
}

TEMP
typename arrIter Array<T>::end() noexcept
{
    return iterator(back());
}

// Const iterator implementation.

#define constArrIter Array<T>::const_iterator

TEMP
constArrIter::const_iterator(const T* ptr) :
    ptr(ptr) {}

TEMP
constArrIter::const_iterator(const const_iterator& other) :
    ptr(other.ptr) {}

TEMP
typename constArrIter& constArrIter::operator=(const const_iterator& other)
{
    this->ptr = other.ptr;
    return *this;
}

TEMP
const T& constArrIter::operator*() const
{
    return *ptr;
}

TEMP
const T* constArrIter::operator->() const
{
    return ptr;
}

TEMP
typename constArrIter& constArrIter::operator++()
{
    ++ptr;
    return *this;
}

TEMP
typename constArrIter constArrIter::operator++(int n)
{
    (void) n;

    constArrIter temp = *this;
    ptr++;
    return temp;
}

TEMP
typename constArrIter& constArrIter::operator--()
{
    --ptr;
    return *this;
}

TEMP
typename constArrIter constArrIter::operator--(int n)
{
    (void) n;

    constArrIter temp = *this;
    ptr--;
    return *this;
}

TEMP
bool constArrIter::operator==(const const_iterator& other) const
{
    return (this->ptr == other.ptr);
}

TEMP
bool constArrIter::operator!=(const const_iterator& other) const
{
    return (this->ptr != other.ptr);
}

TEMP
typename constArrIter Array<T>::begin() const noexcept
{
    return const_iterator(front());
}

TEMP
typename constArrIter Array<T>::end() const noexcept
{
    return const_iterator(back());
}

TEMP
typename constArrIter Array<T>::cbegin() const noexcept
{
    return const_iterator(front());
}

TEMP
typename constArrIter Array<T>::cend() const noexcept
{
    return const_iterator(back());
}