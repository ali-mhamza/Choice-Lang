#include "../../../include/core.h"

/* Forward declarations. */

#define METHOD(name) Object name(Object instance, iter it, u8 argCount);

namespace Core::Methods::Bool
{

};

#undef METHOD

namespace BoolMethods = Core::Methods::Bool;


/* Utility arrays. */

const std::array<Core::sv, BoolMethods::methodCount>
BoolMethods::names{

};

const std::array<Core::Callable::Method, BoolMethods::methodCount>
BoolMethods::impls{

};

const std::unordered_map<Core::sv, u8>
BoolMethods::search{

};


/* Implementations. */
