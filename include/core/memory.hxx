#pragma once

#include <cstddef>
#include <cstdlib>
#include <type_traits>
#include <utility>

namespace CxxServer::Core {

//! Storage for asio handler
/*!
 * Manage the memory for a asio handler. Allocation contains a fast path
 * which can return a pointer to a statically allocated buffer. If the allocation
 * size is too large or the statically allocated buffer is in use the slow path
 * can be taken which involves using dynamic memory (malloc / free)
 * 
 * Not thread safe
 */
template<std::size_t S = 1024>
class HandlerMemory {
public:
    HandlerMemory() noexcept : _used(false) {}
    ~HandlerMemory() noexcept = default;

    HandlerMemory(const HandlerMemory &) = delete;
    HandlerMemory(HandlerMemory &&) = delete;
    HandlerMemory &operator=(const HandlerMemory &) = delete;
    HandlerMemory &operator=(HandlerMemory &&) = delete;

    //! Allocate memory for handler
    /*!
     * \param size - Size to allocate
     * \return Pointer to a memory region which is at least as big as the requested size
     */
    inline void *alloc(size_t size) {
        if (!_used && size < S) {
            _used = true;
            return &_fast_storage;
        }

        return std::malloc(size);
    }

    //! Free memory for handler
    /*!
     * \param ptr - Pointer to the allocated buffer to be free'd
     */
    inline void free(void *ptr) {
        if (ptr == &_fast_storage)
            _used = false;
        else
            std::free(ptr);
    }

private:
    bool _used;
    typename std::aligned_storage<S>::type _fast_storage;
};

//! Allocator for handler using HandlerStorage as backing
/*!
 * Parent allocator for the HandlerMemory.
 * Satisfy C++11 minimal requirements
 * 
 * Not thread safe
 */
template<typename T>
class HandlerStorageAllocator {
public:
    //! Element type
    typedef T value_type;
    //! Pointer to element
    typedef T* pointer;
    //! Reference to element
    typedef T& reference;
    //! Pointer to constant element
    typedef const T* const_pointer;
    //! Reference to constant element
    typedef const T& const_reference;
    //! Quantities of elements
    typedef size_t size_type;
    //! Difference between two pointers
    typedef ptrdiff_t difference_type;

    //! Initialize allocator with storage backing
    /*!
     * \param storage - Memory backing for allocator
     */
    explicit HandlerStorageAllocator(HandlerMemory<> &storage) noexcept : _storage(storage) {}

    template<typename U>
    HandlerStorageAllocator(const HandlerStorageAllocator<U> &alloc) noexcept : _storage(alloc._storage) {}
    HandlerStorageAllocator(HandlerStorageAllocator &&alloc) noexcept = default;
    ~HandlerStorageAllocator() noexcept = default;

    template<typename U>
    HandlerStorageAllocator &operator=(const HandlerStorageAllocator<U> &alloc) noexcept {
        _storage = alloc._storage;
        return *this;
    }

    HandlerStorageAllocator &operator=(HandlerStorageAllocator &&) = default;

    //! Allocate a block of storage which can contain the number of elements
    /*!
     * \param num - Number of elements to be allocated
     * \param hint - Allocation hint (default to 0)
     * \return A pointer to a memory block which can contain the required number of elements
     */
    pointer allocate(size_type num, const void *hint = 0) {
        return _storage.alloc(num * sizeof(T));
    }

    //! Deallocate a previously allocated block
    /*!
     * \param ptr - Pointer to memory block to be deallocated
     * \param num - Number of releasing elements
     */
    void deallocate(pointer p, size_type num) {
        _storage.free(p);
    }

private:
    HandlerMemory<> &_storage;
};

template<typename T>
class HandlerFastMem {
public:
    //! Create the allocate handler with a given memory storage
    /*!
     * \param storage - Memory storage
     * \param handler - Handler to call
     */
    HandlerFastMem(HandlerMemory<> &storage, T handler) noexcept : _storage(storage), _handler(handler) {}
    HandlerFastMem(const HandlerFastMem &) noexcept = default;
    HandlerFastMem(HandlerFastMem &&) noexcept = default;
    ~HandlerFastMem() noexcept = default;

    HandlerFastMem &operator=(const HandlerFastMem &) noexcept = default;
    HandlerFastMem &operator=(HandlerFastMem &&) noexcept = default;

    //! Get the handler allocator
    HandlerStorageAllocator<T> get_allocator() const noexcept { return HandlerStorageAllocator<T>(_storage); }

    //! Wrapper for the handler callback
    template<typename... Args>
    void operator()(Args&&... args) { _handler(std::forward<Args>(args)...); }

private:
    HandlerMemory<> &_storage;
    T _handler;
};
}