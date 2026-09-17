#include "../../../include/core.h"
#include "../../../include/error.h"

/* Main driver function. */

Object Core::Methods::getTypeMember(Object& obj, const std::string& name)
{
    #define START_CHECK switch (obj.type()) {
    #define X(type)     case ObjType::type: return type::getMember(obj, name);
    #define END_CHECK   default: return getCommonMember(obj, name); }

    START_CHECK
        METHOD_TYPE_LIST
    END_CHECK

    #undef START_CHECK
    #undef X
    #undef END_CHECK
}


/* Common methods. */

#define METHOD_LIST \
    X(hash)         \
    X(size_of)

Object Core::Methods::getCommonMember(Object& obj, const std::string& name)
{
    auto it{search.find(name)};
    if (it != search.end())
        return CH_ALLOC_CORE_METHOD(obj, names[it->second], impls[it->second]);

    throw RuntimeError(
        CORE_METHOD_NOT_DEFINED,
        CH_STR("method '{}' is not defined for type ({})", name, obj.printType())
    );
}


/* Forward declarations. */

#define METHOD(name) Object name(Object instance, iter it, u8 args)

namespace Core::Methods
{
    #define X(_) +1
    constexpr u8 methodCount{METHOD_LIST};
    #undef X

    #define X(name) METHOD(name);
    METHOD_LIST
    #undef X
};

#undef METHOD


/* Utility arrays. */

const Core::sv
Core::Methods::names[Core::Methods::methodCount]{
    #define X(name) #name,
    METHOD_LIST
    #undef X
};

const Core::Callable::Method
Core::Methods::impls[Core::Methods::methodCount]{
    #define X(name) name,
    METHOD_LIST
    #undef X
};

[[maybe_unused]] static u8 index_{0};
const std::unordered_map<Core::sv, u8>
Core::Methods::search{
    #define X(name) {#name, index_++},
    METHOD_LIST
    #undef X
};


/* Implementations. */

#define METHOD(name) \
	Object Core::Methods::name(Object instance, iter it, u8 args)

// For methods we want to define but have yet to do so.
#define METHOD_STUB(name) \
	METHOD(name) { (void) instance; (void) it; (void) args; return Object{}; }

METHOD(hash)
{
    (void) it;
    checkArity(0, 0, true, args);

    return Object{static_cast<i64>(instance.hash())};
}

METHOD(size_of)
{
    (void) it;
    checkArity(0, 0, true, args);

    u64 size{};
    switch (instance.type())
    {
        case ObjType::Int:      case ObjType::Dec:      case ObjType::Bool:
        case ObjType::Null:     case ObjType::Void:     case ObjType::CoreType:
        case ObjType::CoreFunc:
            size = sizeof(Object);
            break;
        case ObjType::CoreMethod:   size = sizeof(Object) + sizeof(Core::Method);   break;
        case ObjType::Module:       size = sizeof(Object) + sizeof(::Module);       break;
        case ObjType::UserType:     size = sizeof(Object) + sizeof(::Type);         break;
        // Never called.
        case ObjType::Instance:     size = sizeof(Object) + sizeof(::Instance);     break;
        case ObjType::UserFunc:
        case ObjType::Lambda:       size = sizeof(Object) + sizeof(::Function);     break;
        case ObjType::Closure:      size = sizeof(Object) + sizeof(::Closure);      break;
        case ObjType::UserMethod:   size = sizeof(Object) + sizeof(::Method);       break;
        case ObjType::Text:         size = sizeof(Object) + sizeof(::Text);         break;
        case ObjType::String:       size = sizeof(Object) + sizeof(::String);       break;
        case ObjType::Range:        size = sizeof(Object) + sizeof(::Range);        break;
        case ObjType::List:         size = sizeof(Object) + sizeof(::List);         break;
        case ObjType::Table:        size = sizeof(Object) + sizeof(::Table);        break;
        case ObjType::Ref:          size = sizeof(Object) + sizeof(::Cell);         break;
        default: CH_UNREACHABLE();
    }

    return Object{static_cast<i64>(size)};
}

#undef METHOD_LIST
#undef METHOD
#undef METHOD_STUB