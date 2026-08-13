#include "../../include/core.h"

Object Core::Methods::getMember(ObjType type, const std::string& name)
{
    #define START_CHECK     switch (type) {
    #define X(type, _)      case ObjType::type: return type::getMember(name);
    #define END_CHECK       default: CH_UNREACHABLE(); }

    START_CHECK
        METHOD_TYPE_LIST
    END_CHECK

    #undef START_CHECK
    #undef X
    #undef END_CHECK
}

Object Core::Methods::getMember(const std::string& name)
{
    (void) name;
    return Object{static_cast<Method*>(nullptr)};
}