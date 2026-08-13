#include "../../../include/core.h"

/* Forward declarations. */

#define METHOD(name) Object name(Object instance, iter it, u8 argCount);

namespace Core::Methods::Table
{

};

#undef METHOD

namespace TableMethods = Core::Methods::Table;


/* Utility arrays. */

const std::array<Core::sv, TableMethods::methodCount>
TableMethods::names{

};

const std::array<Core::Callable::Method, TableMethods::methodCount>
TableMethods::impls{

};

const std::unordered_map<Core::sv, u8>
TableMethods::search{

};


/* Implementations. */
