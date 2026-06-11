#if CH_USE_ALLOC && defined(CH_LINEAR_ALLOC)

#pragma once
#include "common.h"
#include "gen_alloc.h"
#include <personal/array.h>
#include <cstddef>
#include <type_traits>
#include <utility>

class LinearAlloc
{
    private:
        using Arena = void;

        Arena* const start{};
        Arena* arena{};
        size_t used{0};
        [[maybe_unused]] const size_t cap{}; // Only used in debug builds.
        Array<AllocPair> allocs{};

    public:
        LinearAlloc(size_t size = BASE_SIZE);
        ~LinearAlloc();

        LinearAlloc(const LinearAlloc&) = delete;
        LinearAlloc& operator=(const LinearAlloc&) = delete;
        LinearAlloc(LinearAlloc&&) = delete;
        LinearAlloc& operator=(LinearAlloc&&) = delete;

        template<typename ObjT, typename Dealloc = DefaultDealloc,
        typename... Args>
        [[nodiscard]] ObjT* alloc(Args&&... args) noexcept;

        #if defined(DEBUG)
            [[nodiscard]] size_t allocatedMemory();
        #endif
};

template<typename ObjT, typename Dealloc, typename... Args>
ObjT* LinearAlloc::alloc(Args&&... args) noexcept
{
    ObjT* obj = static_cast<ObjT*>(
        alignMem(AS_VOID(AS_BYTES(arena) + used), alignof(ObjT))
    );
    // Increment first before constructing the object, in case
    // the constructor allocates memory using the allocator
    // (since otherwise both would be sharing the same address).
    used = (AS_BYTES(obj) + sizeof(ObjT)) - AS_BYTES(arena);

    CH_ASSERT_MEM(
        (AS_BYTES(obj) < AS_BYTES(start) + cap),
        "Ran out of memory",
        start
    );

    new (obj) ObjT(std::forward<Args>(args)...);
    if constexpr (!std::is_trivially_destructible_v<ObjT>)
        allocs.push(AllocPair{AS_VOID(obj), Dealloc()});
    return obj;
}

#endif