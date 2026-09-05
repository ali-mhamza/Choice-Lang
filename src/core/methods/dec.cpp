#include "../../../include/core.h"
#include "../../../include/error.h"
#include <cmath>

#define METHOD_LIST \
    X(is_integer)

/* Forward declarations. */

#define METHOD(name) Object name(Object instance, iter it, u8 args)

namespace Core::Methods::Dec
{
    #define X(_) +1
    const u8 methodCount{METHOD_LIST};
    #undef X

    #define X(name) METHOD(name);
    METHOD_LIST
    #undef X
};

#undef METHOD

namespace DecMethods = Core::Methods::Dec;


/* Utility arrays. */

const Core::sv
DecMethods::names[DecMethods::methodCount]{
    #define X(name) #name,
    METHOD_LIST
    #undef X
};

const Core::Callable::Method
DecMethods::impls[DecMethods::methodCount]{
    #define X(name) name,
    METHOD_LIST
    #undef X
};

[[maybe_unused]] static u8 index_{0};
const std::unordered_map<Core::sv, u8>
DecMethods::search{
    #define X(name) {#name, index_++},
    METHOD_LIST
    #undef X
};


/* Implementations. */

#define METHOD(name) \
	Object DecMethods::name(Object instance, iter it, u8 args)

// For methods we want to define but have yet to do so.
#define METHOD_STUB(name) \
	METHOD(name) { (void) instance; (void) it; (void) args; return Object{}; }

METHOD(is_integer)
{
    (void) it;
    checkArity(0, 0, true, args);

    return (fmod(AS_DEC(instance), 1.0) == 0.0);
}

#undef METHOD_LIST
#undef METHOD
#undef METHOD_STUB