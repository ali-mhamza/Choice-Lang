#if CH_USE_ALLOC && defined(CH_LINEAR_ALLOC)

#include "../include/linear_alloc.h"

LinearAlloc::LinearAlloc(size_t size) :
    start(malloc(size)), used(0), cap(size)
{
    CH_ASSERT_MEM(
        (start != nullptr),
        "Allocation failure.",
        start
    );
    arena = reinterpret_cast<Arena*>(alignMem(start, MAX_ALIGN));
    CH_ASSERT_MEM(
        (AS_BYTES(arena) < AS_BYTES(start) + size),
        "Arena is too small.",
        start
    );
}

LinearAlloc::~LinearAlloc()
{
    for (AllocPair& pair : allocs)
        pair.clean();
    free(start);
}

#endif