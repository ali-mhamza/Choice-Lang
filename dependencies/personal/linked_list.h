#pragma once
#include "common.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

template<typename T>
struct ListNode
{
    // static_assert(
    //     std::is_default_constructible_v<T>,
    //     "Node object type is not default-constructible."
    // );

    T object{};
    ListNode<T>* next{nullptr};

    ListNode() = default;
    ListNode(const T& object, ListNode<T>* next = nullptr) :
        object{object}, next{next} {}
};

template<
    typename T,
    typename Compare = std::equal_to<T>,
    typename Alloc = std::allocator<ListNode<T>>
>
class LinkedList
{
    private:
        static Compare compare;
        static Alloc allocator;
        ListNode<T>* head{nullptr};
        size_type listLength{0};

        template<typename... Args>
        static ListNode<T>* allocNode(Args... args)
        {
            using traits = std::allocator_traits<Alloc>;
            ListNode<T>* node{allocator.allocate(1)};
            traits::construct(allocator, node, args...);
            return node;
        }

        static void deallocNode(ListNode<T>* node)
        {
            std::destroy_at(node);
            allocator.deallocate(node, 1);
        }

        void swap(ListNode<T>* first, ListNode<T>* second)
        {
            T temp{first->object};
            first->object = second->object;
            second->object = temp;
        }

    public:
        LinkedList() = default;

        LinkedList(const LinkedList& other)
        {
            for (const auto& object : other)
                this->append(object);
        }

        LinkedList& operator=(const LinkedList& other)
        {
            if (this != &other)
            {
                this->clear();
                for (const auto& object : other)
                    this->append(object);
            }

            return *this;
        }

        LinkedList(LinkedList&& other)
        {
            this->head = other.head;
            this->listLength = other.listLength;

            other.head = nullptr;
            other.listLength = 0;
        }

        LinkedList& operator=(LinkedList&& other)
        {
            this->clear();
            
            this->head = other.head;
            this->listLength = other.listLength;

            other.head = nullptr;
            other.listLength = 0;

            return *this;
        }

        ~LinkedList()
        {
            clear();
        }

        bool operator==(const LinkedList& other) const
        {
            if (this->listLength != other.listLength) return false;

            ListNode<T>* thisTemp{this->head};
            ListNode<T>* otherTemp{other.head};

            while ((thisTemp != nullptr) && (otherTemp != nullptr))
            {
                if (thisTemp->object != otherTemp->object)
                    return false;
                thisTemp = thisTemp->next;
                otherTemp = otherTemp->next;
            }

            return ((thisTemp == nullptr) && (otherTemp == nullptr));
        }

        size_type length() const
        {
            return listLength;
        }

        ListNode<T>* front()
        {
            return head;
        }

        const ListNode<T>* front() const
        {
            return head;
        }

        // Returns pointer to beyond last node.
        ListNode<T>* back()
        {
            // Temporarily.
            // We can mimic std::list and make
            // the last node a dummy sentinel node.
            return nullptr;
        }

        const ListNode<T>* back() const
        {
            return nullptr;
        }

        void clear()
        {
            ListNode<T>* current{head};
            ListNode<T>* temp{nullptr};
            while (current != nullptr)
            {
                temp = current;
                current = current->next;
                deallocNode(temp);
            }

            listLength = 0;
        }

        // Add new node.

        void prepend(const T& object)
        {
            head = allocNode(object, head);
            listLength++;
        }

        void append(const T& object)
        {
            ListNode<T>* newNode{allocNode(object)};
            listLength++;

            if (head == nullptr) // Empty list.
            {
                head = newNode;
                return;
            }
            
            ListNode<T>* temp{head};
            while (temp->next != nullptr)
                temp = temp->next;
            temp->next = newNode;
        }

        void insert(const T& object, size_type position)
        {
            if ((position != 0) && (position >= listLength))
                return; // Put error-handling here.

            // We will allow the user to use insert on position 0
            // for an empty list.
            // No other position accepted if list is empty.

            if (head == nullptr) // Empty list.
            {
                prepend(object);
                return;
            }

            ListNode<T>* newNode{allocNode(object)};
            ListNode<T>* previous{nullptr};
            ListNode<T>* current{head};
            for (size_type i{0}; i < position; i++)
            {
                previous = current;
                current = current->next;
            }

            if (previous == nullptr) // We're adding it at the beginning.
                head = newNode;
            else
                previous->next = newNode;
            
            newNode->next = current;
            listLength++;
        }

        // Find a node.

        size_type position(const T& object, size_type start = 0) const
        {
            if (start >= listLength) return size_max;

            ListNode<T>* temp{head};
            for (size_type i{0}; i < start; i++)
                temp = temp->next;

            // Also handles head being a nullptr.
            size_type position{start};
            while (temp != nullptr)
            {
                if (compare(temp->object, object))
                    return position;
                temp = temp->next;
                position++;
            }

            return size_max;
        }

        // Remove node(s).

        T erase(size_type position)
        {
            if (position >= listLength)
                throw std::out_of_range("Invalid position value.");
            
            // Pointer to element before the one we want.
            ListNode<T>* previous{nullptr};
            // Pointer to the element we want.
            ListNode<T>* current{head};
            for (size_type i{0}; i < position; i++)
            {
                previous = current;
                current = current->next;
            }

            listLength--;

            if (previous == nullptr) // Removing the head.
            {
                ListNode<T>* temp{head};
                T element{head->object};
                head = head->next;
                deallocNode(temp);
                return element;
            }

            previous->next = current->next;
            T element{current->object};
            deallocNode(current);
            return element;
        }

        void remove(const T& object)
        {
            size_type pos{position(object)};
            if (pos != size_max)
                erase(pos);
        }

        T pop()
        {
            // erase() will automatically decrement listLength.
            return erase(listLength - 1);
        }

        void popn(size_type n)
        {
            while (n > 0)
            {
                pop();
                n--;
            }
        }

        // Check for and retrieve nodes.

        bool has(const T& object) const
        {
            return (position(object) != size_max);
        }

        ListNode<T>* at(size_type position) const
        {
            if (position >= listLength)
                return nullptr;

            ListNode<T>* temp{head};
            for (size_type i{0}; i < position; i++)
                temp = temp->next;

            return temp;
        }

        ListNode<T>* get(const T& object) const
        {
            size_type pos{position(object)};
            if (pos == size_max) return nullptr;
            return at(pos);
        }

        // Manage list.

        void sort(bool ascending = true)
        {
            if ((head == nullptr) || (head->next == nullptr))
            {
                // Empty list or only one element; nothing to sort.
                return;
            }

            bool ordered{false};
            while (!ordered)
            {
                ordered = true;
                ListNode<T>* first{head};
                ListNode<T>* second{head->next};

                for (size_type i{0}; i < listLength - 1; i++)
                {
                    if (ascending)
                    {
                        if (first->object > second->object)
                        {
                            swap(first, second);
                            ordered = false;
                        }
                    }
                    else
                    {
                        if (first->object < second->object)
                        {
                            swap(first, second);
                            ordered = false;
                        }
                    }

                    first = second;
                    second = second->next;
                }
            }
        }

        void reverse()
        {
            if (listLength <= 1) return;

            ListNode<T>* first{head};
            ListNode<T>* second{head->next};

            // Disconnect next node from head (the head
            // node will become the last node after we're
            // done reversing).
            head->next = nullptr;

            while (second != nullptr)
            {
                ListNode<T>* temp{second->next};
                second->next = first;
                first = second;
                second = temp;
            }

            // Reassign the head to the new first node.
            head = first;
        }

        void merge(const LinkedList& other)
        {
            // We can't simply connect the end node for this list to
            // the head node of the other, since they would then have
            // duplicate pointers, leading to double-freeing when both
            // list objects' destructors are called.

            for (const auto& object : other)
                this->append(object);
        }

        bool sorted(bool ascending = true) const
        {
            if ((head == nullptr) || (head->next == nullptr))
                return true;
            
            ListNode<T>* first{head};
            ListNode<T>* second{head->next};

            for (size_type i{0}; i < listLength - 1; i++)
            {
                if (ascending && (first->object > second->object))
                    return false;
                else if (!ascending && (first->object < second->object))
                    return false;

                first = second;
                second = second->next;
            }

            return true;
        }

        // Make a deep copy of the list.
        template<typename U>
        friend LinkedList<U> copy(const LinkedList<U>& list);

        class iterator
        {
            private:
                ListNode<T>* ptr{};

            public:
                iterator() = default;

                iterator(ListNode<T>* ptr) :
                    ptr{ptr} {}

                iterator(const iterator& other) :
                    ptr{other.ptr} {}

                iterator& operator=(const iterator& other)
                {
                    if (this != &other)
                        this->ptr = other.ptr;
                    return *this;
                }

                T& operator*()
                {
                    return ptr->object;
                }

                T* operator->()
                {
                    return &(ptr->object);
                }

                iterator& operator++()
                {
                    if (ptr != nullptr) ptr = ptr->next;
                    return *this;
                }

                iterator operator++(int)
                {
                    iterator temp{*this};
                    if (ptr != nullptr) ptr = ptr->next;
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

        class const_iterator
        {
            private:
                const ListNode<T>* ptr{};

            public:
                const_iterator() = default;

                const_iterator(const ListNode<T>* ptr) :
                    ptr{ptr} {}

                const_iterator(const const_iterator& other) :
                    ptr{other.ptr} {}

                const_iterator& operator=(const const_iterator& other)
                {
                    if (this != &other)
                        this->ptr = other.ptr;
                    return *this;
                }

                const T& operator*() const
                {
                    return ptr->object;
                }

                const T* operator->() const
                {
                    return &(ptr->object);
                }

                const_iterator& operator++()
                {
                    if (ptr != nullptr) ptr = ptr->next;
                    return *this;
                }

                const_iterator operator++(int)
                {
                    const_iterator temp{*this};
                    if (ptr != nullptr) ptr = ptr->next;
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

        iterator begin()
        {
            return iterator{front()};
        }

        iterator end()
        {
            return iterator{back()};
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
            return const_iterator{front()};
        }

        const_iterator cend() const
        {
            return const_iterator{back()};
        }
};

template<typename T, typename Compare, typename Alloc>
Compare LinkedList<T, Compare, Alloc>::compare;

template<typename T, typename Compare, typename Alloc>
Alloc LinkedList<T, Compare, Alloc>::allocator;

template<typename U>
LinkedList<U> copy(const LinkedList<U>& list)
{
    LinkedList<U> newList{};
    for (const auto& object : list)
        newList.append(object);
    newList.isSorted = list.isSorted;
    return newList;
}