#include "../../../include/core.h"

/* Forward declarations. */

#define METHOD(name) Object name(Object instance, iter it, u8 argCount);

namespace Core::Methods::Int
{

};

#undef METHOD

namespace IntMethods = Core::Methods::Int;


/* Utility arrays. */

const std::array<Core::sv, IntMethods::methodCount>
IntMethods::names{

};

const std::array<Core::Callable::Method, IntMethods::methodCount>
IntMethods::impls{

};

const std::unordered_map<Core::sv, u8>
IntMethods::search{

};


/* Implementations. */
