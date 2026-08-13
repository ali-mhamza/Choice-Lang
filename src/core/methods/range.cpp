#include "../../../include/core.h"

/* Forward declarations. */

#define METHOD(name) Object name(Object instance, iter it, u8 argCount);

namespace Core::Methods::Range
{

};

#undef METHOD

namespace RangeMethods = Core::Methods::Range;


/* Utility arrays. */

const std::array<Core::sv, RangeMethods::methodCount>
RangeMethods::names{

};

const std::array<Core::Callable::Method, RangeMethods::methodCount>
RangeMethods::impls{

};

const std::unordered_map<Core::sv, u8>
RangeMethods::search{

};


/* Implementations. */
