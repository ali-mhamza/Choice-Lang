#include "../../../include/core.h"

/* Forward declarations. */

#define METHOD(name) Object name(Object instance, iter it, u8 argCount);

namespace Core::Methods::Dec
{

};

#undef METHOD

namespace DecMethods = Core::Methods::Dec;


/* Utility arrays. */

const std::array<Core::sv, DecMethods::methodCount>
DecMethods::names{

};

const std::array<Core::Callable::Method, DecMethods::methodCount>
DecMethods::impls{

};

const std::unordered_map<Core::sv, u8>
DecMethods::search{

};


/* Implementations. */
