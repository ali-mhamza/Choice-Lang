#include "../../../include/core.h"
#include "../../../include/error.h"

#define METHOD_LIST \
    X(count)        \
    X(index)        \
    X(rev)

/* Forward declarations. */

#define METHOD(name) Object name(Object instance, iter it, u8 args)

namespace Core::Methods::Range
{
    #define X(_) +1
    const u8 methodCount{METHOD_LIST};
    #undef X

    #define X(name) METHOD(name);
    METHOD_LIST
    #undef X
};

#undef METHOD

namespace RangeMethods = Core::Methods::Range;


/* Utility arrays. */

const Core::sv
RangeMethods::names[RangeMethods::methodCount]{
    #define X(name) #name,
    METHOD_LIST
    #undef X
};

const Core::Callable::Method
RangeMethods::impls[RangeMethods::methodCount]{
    #define X(name) name,
    METHOD_LIST
    #undef X
};

[[maybe_unused]] static u8 index_{0};
const HashTable<Core::sv, u8>
RangeMethods::search{
    #define X(name) {#name, index_++},
    METHOD_LIST
    #undef X
};


/* Implementations. */

#define METHOD(name) \
	Object RangeMethods::name(Object instance, iter it, u8 args)

// For methods we want to define but have yet to do so.
#define METHOD_STUB(name) \
	METHOD(name) { (void) instance; (void) it; (void) args; return Object{}; }

METHOD(count)
{
    checkArity(1, 1, true, args);
    if (!IS_INT(it[0]))
        throw RuntimeError(WRONG_ARG_TYPE, "argument must be an integer");

    if (AS_RANGE(instance)->contains(AS_INT(it[0])))
        return i64(1);
    return i64(0);
}

METHOD(index)
{
    checkArity(1, 1, true, args);
    if (!IS_INT(it[0]))
        throw RuntimeError(WRONG_ARG_TYPE, "argument must be an integer");

    const auto* range{AS_RANGE(instance)};
    i64 value{AS_INT(it[0])};

    if (!range->contains(value))
        return Object{}; // TODO: Report error.
    return static_cast<i64>((value - range->start) / range->step);
}

METHOD(rev)
{
    (void) it;
    checkArity(0, 0, true, args);

    const auto* range{AS_RANGE(instance)};
    std::array<i64, 3> nums{range->stop, range->start, range->step * -1};
    ::Range::validateRange(nums);
    return CH_ALLOC_RANGE(nums);
}

#undef METHOD_LIST
#undef METHOD
#undef METHOD_STUB