#include "../include/object.h"
#include <cstdio> // For stderr.
#include <cstring>
#include <string_view>
#include <type_traits>

/* Heap obj. */

HeapObj::HeapObj() :
    type(OBJ_INVALID), refCount(0) {}

HeapObj::HeapObj(ObjType type) :
    type(type), refCount(0) {}

bool HeapObj::operator==(const HeapObj& other) const
{
    if (this->type != other.type) return false;
    
    switch (type)
    {
        case OBJ_STRING:
            return AS_CONST_STRING(this).str == AS_CONST_STRING(&other).str;
        case OBJ_RANGE:
            return AS_CONST_RANGE(this) == AS_CONST_RANGE(&other);
        default:            UNREACHABLE();
    }
}

std::string HeapObj::printVal()
{
    if (IS_STRING(this))
        return AS_STRING(this).str;
    else if (IS_RANGE(this))
    {
        const Range& range = AS_RANGE(this);
        return FORMAT_STR("{}..{}, {}", range.start,
            range.stop, range.step);
    }
    return ""; // Temporary.
}

std::string HeapObj::printType()
{
    switch (type)
    {
        case OBJ_BIGINT:   return "BIGINT";    break;
        case OBJ_BIGDEC:   return "BIGDEC";    break;
        case OBJ_STRING:   return "STRING";    break;
        case OBJ_RANGE:    return "RANGE";     break;
        case OBJ_LIST:     return "LIST";      break;
        case OBJ_TABLE:    return "TABLE";     break;
        default:           return "";
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
    HeapObj(OBJ_STRING), str(str) {}

String::String(const std::string_view& view) :
    HeapObj(OBJ_STRING), str(view) {}

Range::Range(i32 start, i32 stop, i32 step) :
    HeapObj(OBJ_RANGE), start(start), stop(stop),
    step(step) {}

bool Range::operator==(const Range& other) const
{
    return ((this->start == other.start)
            && (this->stop == other.stop)
            && (this->step == other.step));
}


/* Object. */

Object::Object() :
    type(OBJ_INVALID)
{
    AS_INT(*this) = 0;
}

void Object::clean()
{
    if (IS_HEAP_OBJ(*this))
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
        // Heap object.
        default:        return AS_HEAP_VAL(*this) == AS_HEAP_VAL(other);
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
        case OBJ_DEC:   return std::to_string(AS_DEC(*this)).substr(0, 4);
        case OBJ_BOOL:  return (AS_BOOL(*this) ? "true" : "false");
        case OBJ_NULL:  return "null";
        default:        return AS_HEAP_PTR(*this)->printVal();
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
        default:        return AS_HEAP_PTR(*this)->printType();
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
        default:
        {
            if (IS_HEAP_OBJ(*this))
                AS_HEAP_PTR(*this)->emit(os);
            break;
        }
    }
}


/* Type Mismatch Error Class.*/

TypeMismatch::TypeMismatch(const std::string& message, ObjType expect,
    ObjType actual) :
    message(message), expect(expect), actual(actual) {}

static std::string_view objTypes[] = {
    "int", "dec", "bool", "null", "bigint", "bigdec",
    "string", "range", "list", "table", "num"
};

void TypeMismatch::report()
{    
    FORMAT_PRINT(stderr,
        "Type mismatch: Expected type ({}) but found ({}) instead.\n",
        objTypes[expect], objTypes[actual]
    );
    FORMAT_PRINT(stderr, "{:>15}{}\n", "", message);
}