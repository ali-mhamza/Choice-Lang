#include "../../../include/core.h"
#include "../../../include/error.h"

#define METHOD_LIST \
    X(append)       \
    X(clear)        \
    X(count)        \
    X(empty)        \
    X(index)        \
    X(pop)

/* Forward declarations. */

#define METHOD(name) Object name(Object instance, iter it, u8 args)

namespace Core::Methods::List
{
    #define X(_) +1
    const u8 methodCount{METHOD_LIST};
    #undef X

    #define X(name) METHOD(name);
    METHOD_LIST
    #undef X
};

#undef METHOD

namespace ListMethods = Core::Methods::List;


/* Utility arrays. */

const Core::sv
ListMethods::names[ListMethods::methodCount]{
    #define X(name) #name,
    METHOD_LIST
    #undef X
};

const Core::Callable::Method
ListMethods::impls[ListMethods::methodCount]{
    #define X(name) name,
    METHOD_LIST
    #undef X
};

static u8 index_{0};
const HashTable<Core::sv, u8>
ListMethods::search{
    #define X(name) {#name, index_++},
    METHOD_LIST
    #undef X
};


/* Implementations. */

#define METHOD(name) \
    Object ListMethods::name(Object instance, iter it, u8 args)

// For methods we want to define but have yet to do so.
#define METHOD_STUB(name) \
	METHOD(name) { (void) instance; (void) it; (void) args; return Object{}; }

METHOD(append)
{
    checkArity(1, 1, true, args);
    if (IS_IMMUT(instance))
        throw RuntimeError(MOD_IMMUT_VALUE);

    AS_LIST(instance)->array.push(it[0]);
    return Object::typed(ObjType::Void);
}

METHOD(clear)
{
    (void) it;
    checkArity(0, 0, true, args);
    if (IS_IMMUT(instance))
        throw RuntimeError(MOD_IMMUT_VALUE);

    AS_LIST(instance)->array.clear();
    return Object::typed(ObjType::Void);
}

METHOD(count)
{
    checkArity(1, 1, true, args);

    i64 count{0};
    for (const auto& entry : AS_LIST(instance)->array)
    {
        if (entry == it[0])
            count++;
    }

    return count;
}

METHOD(empty)
{
    (void) it;
    checkArity(0, 0, true, args);

    return (AS_LIST(instance)->array.count() == 0);
}

METHOD(index)
{
    checkArity(1, 1, true, args);

    i64 index{-1};
    const auto& array{AS_LIST(instance)->array};
    auto size{array.count()};

    for (u64 i{0}; i < size; i++)
    {
        if (array[i] == it[0])
        {
            index = i;
            break;
        }
    }

    if (index == -1)
    {
        // Report error.
    }

    return index;
}

METHOD(pop)
{
    (void) it;
    checkArity(0, 0, true, args);
    if (IS_IMMUT(instance))
        throw RuntimeError(MOD_IMMUT_VALUE);

    auto& array{AS_LIST(instance)->array};
    if (array.empty())
    {
        // Report error.
    }

    return array.pop();
}

#undef METHOD_LIST
#undef METHOD
#undef METHOD_STUB