#include "../../../include/core.h"
#include "../../../include/error.h"

#define METHOD_LIST	\
	X(clear)		\
	X(get)			\
	X(pop)

/* Forward declarations. */

#define METHOD(name) Object name(Object instance, iter it, u8 args)

namespace Core::Methods::Table
{
    #define X(_) +1
    const u8 methodCount{METHOD_LIST};
    #undef X

    #define X(name) METHOD(name);
    METHOD_LIST
    #undef X
};

#undef METHOD

namespace TableMethods = Core::Methods::Table;


/* Utility arrays. */

const Core::sv
TableMethods::names[TableMethods::methodCount]{
    #define X(name) #name,
    METHOD_LIST
    #undef X
};

const Core::Callable::Method
TableMethods::impls[TableMethods::methodCount]{
    #define X(name) name,
    METHOD_LIST
    #undef X
};

static u8 index_{0};
const std::unordered_map<Core::sv, u8>
TableMethods::search{
    #define X(name) {#name, index_++},
    METHOD_LIST
    #undef X
};


/* Implementations. */

#define METHOD(name) \
	Object TableMethods::name(Object instance, iter it, u8 args)

// For methods we want to define but have yet to do so.
#define METHOD_STUB(name) \
	METHOD(name) { (void) instance; (void) it; (void) args; return Object{}; }

METHOD(clear)
{
	(void) it;
    checkArity(0, 0, true, args);

	AS_TABLE(instance)->table.clear();
	return Object::typed(ObjType::Void);
}

METHOD(get)
{
    checkArity(1, 2, true, args);
	const auto* table{AS_TABLE(instance)};
	const auto& key{it[0]};

	if (args == 1)
		return table->getIndex(key);
	else
	{
		if (table->contains(key))
			return table->getIndex(key);
		return it[1];
	}
}

METHOD(pop)
{
    checkArity(1, 2, true, args);
	auto& table{AS_TABLE(instance)->table};
	const auto& key{it[0]};

	if (table.contains(key))
	{
		Object value{table[key]};
		table.remove(key);
		return value;
	}
	else if (args == 2)
		return it[1];
	else
	{
		// TODO: Report error.
		return Object{};
	}
}

#undef METHOD_LIST
#undef METHOD
#undef METHOD_STUB