#include <cstddef>
#include <iostream>

template <size_t N>
class alignas(std::max_align_t) StackStorage {
private:
    char storage[N];
    char* start = storage;

    size_t change(size_t _n) {
        return ((_n - 1) / alignof(std::max_align_t) + 1)  * alignof(std::max_align_t);
    }

public:

    StackStorage(const StackStorage& stack_storage) = delete;

    StackStorage() = default;

    char* allocate(size_t _n) {
        start += change(_n);
        return start - change(_n);
    }

    void deallocate() {}

};

template <typename T, size_t N>
class StackAllocator {
private:
    StackStorage<N>* stackStorage;

    void swap(StackAllocator& cpy) {
        std::swap(cpy.stackStorage, stackStorage);
    }

public:


    template <typename U, size_t C>
    friend class StackAllocator;

    using value_type = T;

    StackAllocator() = default;

    StackAllocator(StackStorage<N>& _stackStorage) : stackStorage(&_stackStorage) {}

    StackAllocator(const StackAllocator& stackAllocator) : stackStorage(stackAllocator.stackStorage) {}

    StackAllocator operator==(const StackAllocator& stackAllocator) {
        return stackStorage == stackAllocator.stackStorage;
    }

    StackAllocator& operator=(const StackAllocator& stackAllocator) = default;

    template <typename U>
    StackAllocator(const StackAllocator<U, N>& stackAllocator) : stackStorage(stackAllocator.stackStorage) {}


    T* allocate(size_t _n) {
        return reinterpret_cast<T*>(stackStorage->allocate(sizeof(T)*_n));
    }

    void deallocate(const T*, size_t) {}

    template <typename U>
    struct rebind {
        using other = StackAllocator<U, N>;
    };



};

template <typename T, typename Allocator = std::allocator<T>>
class List {
private:
    struct BasicNode {
        BasicNode* prev;
        BasicNode* next;
        BasicNode() = default;
        BasicNode(BasicNode* _prev, BasicNode* _next) : prev(_prev), next(_next) {}
    };

    struct Node : BasicNode {
        T value;
        Node() = default;
        Node(BasicNode* _prev, BasicNode* _next, const T& _value) : BasicNode(_prev, _next), value(_value) {}
        Node(BasicNode* _prev, BasicNode* _next) : BasicNode(_prev, _next) {}
        Node(BasicNode* _prev, BasicNode* _next, bool) : BasicNode(_prev, _next), value(T()) {}
    };

    BasicNode head;
    size_t sz = 0;
    using AllocTraits = std::allocator_traits<typename std::allocator_traits<Allocator>::template rebind_alloc<Node>>;
    typename AllocTraits::allocator_type alloc;
    Allocator standardAlloc;



    void swap(List& lst) {
        std::swap(sz, lst.sz);
        std::swap(head, lst.head);
        std::swap(head.next->prev, lst.head.next->prev);
        std::swap(head.prev->next, lst.head.prev->next);
        std::swap(alloc, lst.alloc);
        std::swap(standardAlloc, lst.standardAlloc);
    }

    template <typename... Args>
    void initialization(size_t _sz, const Args&... args) {
        size_t i = 0;
        BasicNode* cur;
        try {
            for (; i < _sz; ++i) {
                cur = static_cast<BasicNode*>(AllocTraits::allocate(alloc, 1));
                (head.prev)->next = cur;
                AllocTraits::construct(alloc, static_cast<Node*>(cur), head.prev, &head, args...);
                head.prev = cur;
            }
        } catch (...) {
            for (size_t j = 0; j < i; ++j) {
                pop_front();
            }
            AllocTraits::deallocate(alloc, static_cast<Node*>(cur), 1);
            throw;
        }
    }


public:

    template <bool isConst>
    class Iter {
    private:
        BasicNode* ptr;
    public:

        using difference_type = std::ptrdiff_t;
        using value_type = std::conditional_t<isConst, const T, T>;
        using pointer = std::conditional_t<isConst, const T*, T*>;
        using reference = std::conditional_t<isConst, const T&, T&>;
        using iterator_category = std::bidirectional_iterator_tag;

        Iter(const BasicNode* _ptr) : ptr(const_cast<BasicNode*>(_ptr)) {}

        Iter(const Iter<false>& it) : ptr(it.ptr) {}

        reference operator*() const {
            return static_cast<Node*>(ptr)->value;
        }

        pointer operator->() const {
            return ptr;
        }

        Iter& operator++() {
            ptr = ptr->next;
            return *this;
        }

        Iter operator++(int) {
            Iter new_ptr(ptr);
            ptr = ptr->next;
            return new_ptr;
        }

        Iter& operator--() {
            ptr = ptr->prev;
            return *this;
        }

        Iter operator--(int) {
            Iter new_ptr(ptr);
            ptr = ptr->prev;
            return new_ptr;
        }

        template <bool isEqConst>
        bool operator==(const Iter<isEqConst>& it) const {
            return it.ptr == ptr;
        }

        template <bool isUnEqConst>
        bool operator!=(const Iter<isUnEqConst>& it) const {
            return it.ptr != ptr;
        }

        friend class List;

    };



    using iterator = Iter<false>;
    using const_iterator = Iter<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    List(const List& lst) : alloc(AllocTraits::select_on_container_copy_construction(lst.standardAlloc)),
                                              standardAlloc(AllocTraits::select_on_container_copy_construction(lst.standardAlloc)) {
        head.next = head.prev = &head;
        BasicNode* lstBasicNode = lst.head.next;
        size_t i = 0;
        try {
            for (; i < lst.sz; ++i) {
                insert(end(), static_cast<Node*>(lstBasicNode)->value);
                if (i == lst.sz - 1)
                    break;
                lstBasicNode = lstBasicNode->next;
            }
        } catch (...) {
            for (size_t j = 0; j < i; ++j) {
                pop_front();
            }
            throw;
        }
    }

    List(const Allocator& _alloc = Allocator()) : alloc(_alloc), standardAlloc(_alloc) {
        head.next = head.prev = &head;
    }

    List(size_t _sz, const Allocator& _alloc = Allocator()) : sz(_sz), alloc(_alloc), standardAlloc(_alloc) {
        head.next = head.prev = &head;
        initialization(_sz, true);
    }

    List(size_t _sz, const T& _value, const Allocator& _alloc = Allocator()) : alloc(_alloc), standardAlloc(_alloc) {
        initialization(_sz, _value);
    }


    List& operator=(const List& lst) {
        if (AllocTraits::propagate_on_container_copy_assignment::value)
            standardAlloc = lst.standardAlloc;
        alloc = standardAlloc;
        List cpy(alloc);
        for (const_iterator it = lst.begin(); it != lst.end(); ++it) {
            cpy.push_back(*it);
        }
        swap(cpy);
        return *this;
    }


    size_t size() const {
        return sz;
    }

    Allocator get_allocator() const {
        return standardAlloc;
    }

    void insert(const_iterator it, const T& value) {
        ++sz;
        BasicNode* newBasicNode = static_cast<BasicNode*>(AllocTraits::allocate(alloc, 1));
        try {
            AllocTraits::construct(alloc, static_cast<Node*>(newBasicNode), it.ptr->prev, it.ptr, value);
        } catch (...) {
            --sz;
            AllocTraits::deallocate(alloc, static_cast<Node*>(newBasicNode), 1);
            throw;
        }
        it.ptr->prev->next = newBasicNode;
        it.ptr->prev = newBasicNode;
    }

    void erase(const_iterator it) {
        --sz;
        it.ptr->next->prev = it.ptr->prev;
        it.ptr->prev->next = it.ptr->next;
        AllocTraits::destroy(alloc, static_cast<Node*>(it.ptr));
        AllocTraits::deallocate(alloc, static_cast<Node*>(it.ptr), 1);
    }

    void push_back(const T& value) {
        insert(end(), value);
    }

    void push_front(const T& value) {
        insert(begin(), value);
    }

    void pop_front() {
        erase(begin());
    }

    void pop_back() {
        erase(--end());
    }

    iterator begin() {
        return iterator(head.next);
    }

    const_iterator begin() const {
        return const_iterator(head.next);
    }

    const_iterator cbegin() const {
        return const_iterator(head.next);
    }

    iterator end() {
        return iterator(&head);
    }

    const_iterator end() const {
        return const_iterator(&head);
    }

    const_iterator cend() const {
        return const_iterator(&head);
    }

    reverse_iterator rbegin() {
        return reverse_iterator(&head);
    }

    const_reverse_iterator rbegin() const {
        return const_reverse_iterator(&head);
    }

    const_reverse_iterator crbegin() const {
        return const_reverse_iterator(&head);
    }

    reverse_iterator rend() {
        return reverse_iterator(head.next);
    }

    const_reverse_iterator rend() const {
        return const_reverse_iterator(head.next);
    }

    const_reverse_iterator crend() const {
        return const_reverse_iterator(head.next);
    }

    ~List() {
        while(sz != 0) {
            pop_back();
        }
    }

};








