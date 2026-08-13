#include "../../../include/core.h"

/* Forward declarations. */

#define METHOD(name) Object name(Object instance, iter it, u8 argCount);

namespace Core::Methods::List
{

};

#undef METHOD

namespace ListMethods = Core::Methods::List;


/* Utility arrays. */

const std::array<Core::sv, ListMethods::methodCount>
ListMethods::names{

};

const std::array<Core::Callable::Method, ListMethods::methodCount>
ListMethods::impls{

};

const std::unordered_map<Core::sv, u8>
ListMethods::search{

};


/* Implementations. */
