#include "../../../include/core.h"
#include "../../../include/error.h"

#define METHOD_LIST \
    X(members)

/* Forward declarations. */

#define METHOD(name) Object name(Object instance, iter it, u8 args)

namespace Core::Methods::Module
{
    #define X(_) +1
    const u8 methodCount{METHOD_LIST};
    #undef X

    #define X(name) METHOD(name);
    METHOD_LIST
    #undef X
};

#undef METHOD

namespace ModuleMethods = Core::Methods::Module;


/* Utility arrays. */

const Core::sv
ModuleMethods::names[ModuleMethods::methodCount]{
    #define X(name) #name,
    METHOD_LIST
    #undef X
};

const Core::Callable::Method
ModuleMethods::impls[ModuleMethods::methodCount]{
    #define X(name) name,
    METHOD_LIST
    #undef X
};

[[maybe_unused]] static u8 index_{0};
const HashTable<Core::sv, u8>
ModuleMethods::search{
    #define X(name) {#name, index_++},
    METHOD_LIST
    #undef X
};


/* Implementations. */

#define METHOD(name) \
	Object ModuleMethods::name(Object instance, iter it, u8 args)

// For methods we want to define but have yet to do so.
#define METHOD_STUB(name) \
	METHOD(name) { (void) instance; (void) it; (void) args; return Object{}; }

METHOD(members)
{
    (void) it;
    checkArity(0, 0, true, args);

    const auto* module{AS_MODULE(instance)};
    auto* list{CH_ALLOC_LIST(module->entries.size())};

    for (const auto& [key, _] : module->entries)
        list->array.push(CH_ALLOC_TEXT(key));

    return list;
}

#undef METHOD_LIST
#undef METHOD
#undef METHOD_STUB