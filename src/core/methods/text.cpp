#include "../../../include/core.h"
#include "../../../include/error.h"

#define METHOD_LIST \
    X(empty)        \
    X(substr)

/* Forward declarations. */

#define METHOD(name) Object name(Object instance, iter it, u8 args)

namespace Core::Methods::Text
{
    #define X(_) +1
    const u8 methodCount{METHOD_LIST};
    #undef X

    #define X(name) METHOD(name);
    METHOD_LIST
    #undef X
};

#undef METHOD

namespace TextMethods = Core::Methods::Text;


/* Utility arrays. */

const Core::sv
TextMethods::names[TextMethods::methodCount]{
    #define X(name) #name,
    METHOD_LIST
    #undef X
};

const Core::Callable::Method
TextMethods::impls[TextMethods::methodCount]{
    #define X(name) name,
    METHOD_LIST
    #undef X
};

[[maybe_unused]] static u8 index_{0};
const HashTable<Core::sv, u8>
TextMethods::search{
    #define X(name) {#name, index_++},
    METHOD_LIST
    #undef X
};


/* Implementations. */

#define METHOD(name) \
	Object TextMethods::name(Object instance, iter it, u8 args)

// For methods we want to define but have yet to do so.
#define METHOD_STUB(name) \
	METHOD(name) { (void) instance; (void) it; (void) args; return Object{}; }

METHOD(empty)
{
    (void) it;
    checkArity(0, 0, true, args);

    return instance.getObjectText().empty();
}

METHOD(substr)
{
    checkArity(1, 2, true, args);

    auto checkIndex = [&instance](const Object& obj, bool start) {
        if (!IS_INT(obj))
            throw RuntimeError(WRONG_ARG_TYPE, "argument must be an integer");
        if (AS_INT(obj) < 0)
            throw RuntimeError(WRONG_ARG_TYPE, "argument cannot be negative");

        u64 val{static_cast<u64>(AS_INT(obj))};
        u64 size{instance.collectionSize()};
        if ((val > size) || (start && (val == size)))
            throw RuntimeError(WRONG_ARG_TYPE, "argument value too large");
    };

    std::string_view text{instance.getObjectText()};
    std::string_view sub{};

    checkIndex(it[0], true);
    i64 start{AS_INT(it[0])};
    if (args == 1)
        sub = text.substr(start);
    else
    {
        checkIndex(it[1], false);
        i64 count{AS_INT(it[1])};
        sub = text.substr(start, count);
    }

    return CH_ALLOC_TEXT(sub);
}

#undef METHOD_LIST
#undef METHOD
#undef METHOD_STUB