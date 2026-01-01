#include "../include/object.h"
#include <cstring>
#include <iomanip>
#include <iostream>
#include <type_traits>
#include <variant>

/* Heap obj. */

HeapObj::HeapObj() :
    type(HEAP_INVALID), refCount(0) {}

HeapObj::HeapObj(HeapType type) :
    type(type), refCount(0) {}

bool HeapObj::operator==(const HeapObj& other) const
{
    HeapObj* obj = const_cast<HeapObj*>(&other);

    if (this->type != obj->type) return false;
    
    switch (type)
    {
        case HEAP_STRING:
            return AS_CONST_STRING(this).str == AS_CONST_STRING(obj).str;
        default:            UNREACHABLE();
    }
}

std::string HeapObj::printVal()
{
    if (IS_STRING(this))
        return AS_STRING(this).str;
    return ""; // Temporary.
}

std::string HeapObj::printType()
{
    switch (type)
    {
        case HEAP_BIGINT:   return "BIGINT";    break;
        case HEAP_BIGDEC:   return "BIGDEC";    break;
        case HEAP_STRING:   return "STRING";    break;
        case HEAP_LIST:     return "LIST";      break;
        case HEAP_TABLE:    return "TABLE";     break;
        default:            return "";
    }
}

void HeapObj::emit(std::ofstream& os)
{
    os.put(static_cast<char>(type));
    
    if (IS_STRING(this))
    {
        String temp = AS_STRING(this);
        os.write(temp.str.data(), temp.str.size());
        os.put('\0');
    }
    else if (IS_LIST(this))
    {
        // List* temp = static_cast<List*>(this);
        // ...
    }
    else if (IS_TABLE(this))
    {
        // Table* temp = static_cast<Table*>(this);
    }
}

String::String(const std::string& str) :
    HeapObj(HEAP_STRING), str(str) {}

String::String(const std::string_view& view) :
    HeapObj(HEAP_STRING), str(view) {}


/* Object. */

Object::Object() :
    type(OBJ_INVALID)
{
    AS_INT(*this) = 0;
}

void Object::clean()
{
    if (this->type == OBJ_HEAP)
    {
        HeapObj* temp = AS_HEAP_PTR(*this);
        temp->refCount--;
        if (temp->refCount == 0)
            delete temp;
    }
}

Object::Object(const Object& other) :
    type(other.type), as(other.as)
{
    if (IS_HEAP_OBJ(*this))
        AS_HEAP_PTR(*this)->refCount++;
}

Object& Object::operator=(const Object& other)
{
    if (this != &other)
    {
        clean();
        
        this->type = other.type;
        this->as = other.as;

        if (IS_HEAP_OBJ(*this))
            AS_HEAP_PTR(*this)->refCount++;
    }

    return *this;
}

Object::Object(Object&& other) noexcept :
    type(other.type), as(other.as)
{
    other.type = OBJ_INVALID; // To prevent deallocation when it is destroyed.
}

Object& Object::operator=(Object&& other) noexcept
{
    if (this != &other)
    {
        clean();
        
        this->type = other.type;
        this->as = other.as;

        other.type = OBJ_INVALID;
    }

    return *this;
}

Object::~Object()
{
    clean();
}

bool Object::operator==(const Object& other) const
{
    if (this->type != other.type) return false;
    
    switch (this->type)
    {
        case OBJ_INT:   return AS_INT(*this) == AS_INT(other);
        case OBJ_DEC:   return AS_DEC(*this) == AS_DEC(other);
        case OBJ_BOOL:  return AS_BOOL(*this) == AS_BOOL(other);
        case OBJ_NULL:  return true;
        case OBJ_HEAP:  return AS_HEAP_VAL(*this) == AS_HEAP_VAL(other);
        default:        UNREACHABLE();
    }
}

bool Object::operator>(const Object& other) const
{
    if (IS_NUM(*this) && IS_NUM(other))
        return AS_NUM(*this) > AS_NUM(other);
    return false;
}

bool Object::operator<(const Object& other) const
{
    if (IS_NUM(*this) && IS_NUM(other))
        return AS_NUM(*this) < AS_NUM(other);
    return false;
}

std::string Object::printVal() const
{
    switch (type)
    {
        case OBJ_INT:   return std::to_string(AS_INT(*this));
        case OBJ_DEC:   return std::to_string(AS_DEC(*this));
        case OBJ_BOOL:  return (AS_BOOL(*this) ? "true" : "false");
        case OBJ_NULL:  return "null";
        case OBJ_HEAP:  return AS_HEAP_PTR(*this)->printVal();
        default:        UNREACHABLE();
    }
}

std::string Object::printType() const
{
    switch (type)
    {
        case OBJ_INT:   return "INT";
        case OBJ_DEC:   return "DEC";
        case OBJ_BOOL:  return "BOOL";
        case OBJ_NULL:  return "NULL";
        case OBJ_HEAP:  return AS_HEAP_PTR(*this)->printType();
        default:        UNREACHABLE();
    }
}

template<typename T>
static void emitBytes(std::ofstream& os, ObjType type, T value)
{
    os.put(static_cast<char>(type));
    char bytes[sizeof(value)];
    std::memcpy(&bytes[0], &value, sizeof(value));
    os.write(bytes, sizeof(value));
}

void Object::emit(std::ofstream& os) const
{   
    switch (type)
    {
        case OBJ_INT:   emitBytes(os, OBJ_INT, AS_INT(*this));  break;
        case OBJ_DEC:   emitBytes(os, OBJ_DEC, AS_DEC(*this));  break;
        case OBJ_HEAP:
        {
            os.put(static_cast<char>(OBJ_HEAP));
            AS_HEAP_PTR(*this)->emit(os);
            break;
        }
        default: break;
    }
}


/* Type Mismatch Error Class.*/

TypeMismatch::TypeMismatch(const std::string& message, varType expect,
    varType actual) :
        message(message), expect(expect), actual(actual) {}

static std::string objTypes[] = {
    "int", "dec", "bool", "null", "num"
};

static std::string heapTypes[] = {
    "bigint", "bigdec", "string", "list",
    "table"
};

void TypeMismatch::report()
{
    std::cerr << "Type mismatch: Expected type (";
    if (std::holds_alternative<ObjType>(expect))
        std::cerr << objTypes[GETV(expect, ObjType)];
    else
        std::cerr << heapTypes[GETV(expect, HeapType)];
    std::cerr << ") but found (";
    if (std::holds_alternative<ObjType>(actual))
        std::cerr << objTypes[GETV(actual, ObjType)];
    else
        std::cerr << heapTypes[GETV(actual, HeapType)];
    std::cerr << ") instead.\n";
    std::cerr << std::setw(15) << "";
    std::cerr << message << '\n';
}