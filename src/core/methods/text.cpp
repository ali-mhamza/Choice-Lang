#include "../../../include/core.h"

/* Forward declarations. */

#define METHOD(name) Object name(Object instance, iter it, u8 argCount);

namespace Core::Methods::Text
{

};

#undef METHOD

namespace TextMethods = Core::Methods::Text;


/* Utility arrays. */

const std::array<Core::sv, TextMethods::methodCount>
TextMethods::names{

};

const std::array<Core::Callable::Method, TextMethods::methodCount>
TextMethods::impls{

};

const std::unordered_map<Core::sv, u8>
TextMethods::search{

};


/* Implementations. */
