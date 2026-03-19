#if CH_USE_ALLOC && defined(CH_LINEAR_ALLOC)

#pragma once
#include "array.h"
#include "common.h"
#include "gen_alloc.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <utility>

class LinearAlloc
{
    private:
        using Arena = void;

        Arena* start;
        Arena* arena;
        size_t used;
        size_t cap;
        Array<AllocPair> allocs;
    
    public:
        LinearAlloc(size_t size = BASE_SIZE);
        ~LinearAlloc();

        LinearAlloc(const LinearAlloc&) = delete;
        LinearAlloc& operator=(const LinearAlloc&) = delete;
        LinearAlloc(LinearAlloc&&) = delete;
        LinearAlloc& operator=(LinearAlloc&&) = delete;

        template<typename ObjT, typename Dealloc = DefaultDealloc,
        typename... Args>
        ObjT* alloc(Args&&... args) noexcept;
};

template<typename ObjT, typename Dealloc, typename... Args>
ObjT* LinearAlloc::alloc(Args&&... args) noexcept
{
    ObjT* obj = static_cast<ObjT*>(
        alignMem(AS_VOID(AS_BYTES(arena) + used), alignof(ObjT))
    );
    CH_ASSERT_MEM(
        (AS_BYTES(obj) < AS_BYTES(start) + cap),
        "Ran out of memory",
        start
    );
    new (obj) ObjT(std::forward<Args>(args)...);
    used = (AS_BYTES(obj) + sizeof(ObjT)) - AS_BYTES(arena);
    if constexpr (!std::is_trivially_destructible_v<ObjT>)
        allocs.push(AllocPair{AS_VOID(obj), Dealloc()});
    return obj;
}

#endif