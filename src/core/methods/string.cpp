#include "../../../include/core.h"

/* Forward declarations. */

#define METHOD(name) Object name(Object instance, iter it, u8 argCount);

namespace Core::Methods::String
{

};

#undef METHOD

namespace StringMethods = Core::Methods::String;


/* Utility arrays. */

const std::array<Core::sv, StringMethods::methodCount>
StringMethods::names{

};

const std::array<Core::Callable::Method, StringMethods::methodCount>
StringMethods::impls{

};

const std::unordered_map<Core::sv, u8>
StringMethods::search{

};


/* Implementations. */
