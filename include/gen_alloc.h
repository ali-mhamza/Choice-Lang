#if CH_USE_ALLOC

#pragma once
#include <cstddef>      // For size_t, uintptr_t.
#include <cstdint>      // For uint8_t in AS_BYTES macro.
#include <functional>   // For std::function in AllocPair.

#define KiB(n)  ((n) * (1 << 10))
#define MiB(n)  ((n) * (1 << 20))
#define GiB(n)  ((n) * (1 << 30))

#define AS_BYTES(ptr)   reinterpret_cast<uint8_t*>(ptr)
#define AS_VOID(ptr)    reinterpret_cast<void*>(ptr)

inline constexpr size_t MAX_ALIGN = alignof(std::max_align_t);
inline constexpr size_t BASE_SIZE = 4096;

inline bool isValidAlign(size_t align)
{
    // Check if align is a power of 2.
    return ((align & (align - 1)) == 0);
}

inline void* alignMem(void* mem, size_t align)
{
    if (!isValidAlign(align)) return nullptr;

    std::uintptr_t ptr = reinterpret_cast<std::uintptr_t>(mem);
    ptr = (ptr + align) & ~(align - 1);
    return AS_VOID(ptr);
}

struct AllocPair
{
    using DeallocObj = std::function<void(void*)>;

    void*       obj;
    DeallocObj  func;

    ~AllocPair() = default;
    void clean() { if (obj != nullptr) func(obj); }
};

struct DefaultDealloc
{
    void operator()(void* mem) {}
};

#endif