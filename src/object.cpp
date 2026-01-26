#include "../include/object.h"
#include <cstdio> // For stderr.
#include <cstring>
#include <string_view>
#include <type_traits>

/* HeapObj. */

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

std::string HeapObj::printVal() const
{
    if (IS_STRING(this))
        return AS_CONST_STRING(this).str;
    else if (IS_RANGE(this))
    {
        const Range& range = AS_CONST_RANGE(this);
        auto retStr = FORMAT_STR("{}..{}", range.start,
            range.stop);
        if (range.step != 1)
            retStr += FORMAT_STR("..{}", range.step);
        return retStr;
    }
    return ""; // Temporary.
}

std::string HeapObj::printType() const
{
    switch (type)
    {
        case OBJ_BIGINT:    return "BIGINT";    break;
        case OBJ_BIGDEC:    return "BIGDEC";    break;
        case OBJ_STRING:    return "STRING";    break;
        case OBJ_RANGE:     return "RANGE";     break;
        case OBJ_LIST:      return "LIST";      break;
        case OBJ_TABLE:     return "TABLE";     break;
        default:            return "";
    }
}

void HeapObj::emit(std::ofstream& os)
{
    os.put(static_cast<char>(type));
    
    if (IS_STRING(this))
    {
        const String& temp = AS_STRING(this);
        os.write(temp.str.data(), temp.str.size());
        os.put('\0');
    }
    else if (IS_RANGE(this))
    {
        const Range& temp = AS_RANGE(this);
        os.write(reinterpret_cast<const char*>(&temp.start), sizeof(i64));
        os.write(reinterpret_cast<const char*>(&temp.stop), sizeof(i64));
        os.write(reinterpret_cast<const char*>(&temp.step), sizeof(i64));
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

String::String(const char* str, size_t len) :
    HeapObj(OBJ_STRING)
{
    if (len == -1) len = strlen(str);
    this->str = std::string(str, len);
}

Range::Range(const std::array<i64, 3>& limits) :
    HeapObj(OBJ_RANGE), start(limits[0]), stop(limits[1]),
    step(limits[2]) {}

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
    else if (IS_ITER(*this))
        delete AS_ITER(*this); // We never copy iterators, so no refcount.
}

Object::Object(const Object& other) :
    type(other.type), as(other.as)
{
    ASSERT(!IS_ITER(other), "Copying an iterator is not allowed.");
    
    if (IS_HEAP_OBJ(*this))
        AS_HEAP_PTR(*this)->refCount++;
}

Object& Object::operator=(const Object& other)
{
    ASSERT(!IS_ITER(other), "Copying an iterator is not allowed.");
    
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
        case OBJ_DEC:   return std::to_string(AS_DEC(*this));
        case OBJ_BOOL:  return (AS_BOOL(*this) ? "true" : "false");
        case OBJ_NULL:  return "null";
        default:
        {
            if (IS_HEAP_OBJ(*this))
                return AS_HEAP_PTR(*this)->printVal();
            else
            {
                const auto& iter = AS_ITER(*this)->iter;
                std::string ret;
                std::visit([&ret](auto&& iter) {
                    ret = "->" + iter.obj->printVal();
                }, iter);

                return ret;
            }
        }
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

ObjIter* Object::makeIter()
{
    if (!IS_ITERABLE(*this)) return nullptr;
    return new ObjIter(*this);
}


/* Object iterator struct types. */

StringIter::StringIter() :
    obj(nullptr), iter(nullptr), begin(nullptr) {}

StringIter::StringIter(String* obj) :
    obj(obj), iter(nullptr), begin(obj->str.c_str())
{
    obj->refCount++;
}

StringIter::StringIter(StringIter&& other) :
    obj(other.obj), iter(other.iter), begin(other.begin)
{
    other.obj = other.iter = nullptr;
    other.begin = nullptr; // Must split; pointers are of different types.
}

StringIter& StringIter::operator=(StringIter&& other)
{
    if (this != &other)
    {
        this->obj = other.obj;
        this->iter = other.iter;
        this->begin = other.begin;

        other.obj = other.iter = nullptr;
        other.begin = nullptr;
    }

    return *this;
}

StringIter::~StringIter()
{
    if (obj != nullptr)
    {
        obj->refCount--;
        if (obj->refCount == 0) delete obj;
    }
    if (iter != nullptr)
    {
        iter->refCount--;
        if (iter->refCount == 0) delete iter;
    }
}

bool StringIter::start(Object& var)
{
    if (obj->str.size() == 0) return false;
    iter = new String(begin, 1);
    iter->refCount++;
    var = Object(static_cast<HeapObj*>(iter));
    return true;
}

bool StringIter::next(Object& var)
{
    begin++;
    if ((iter->str[0] = *begin) == '\0') return false;
    return true;
}

RangeIter::RangeIter() :
    obj(nullptr), val(0) {}

RangeIter::RangeIter(Range* obj) :
    obj(obj), val(obj->start)
{
    obj->refCount++;
}

RangeIter::RangeIter(RangeIter&& other) :
    obj(other.obj), val(other.val)
{
    other.obj = nullptr;
}

RangeIter& RangeIter::operator=(RangeIter&& other)
{
    if (this != &other)
    {
        this->obj = other.obj;
        this->val = other.val;

        other.obj = nullptr;
    }

    return *this;
}

RangeIter::~RangeIter()
{
    if (obj != nullptr)
    {
        obj->refCount--;
        if (obj->refCount == 0) delete obj;
    }
}

bool RangeIter::start(Object& var)
{
    if (obj->start > obj->stop)
        return false;
    var = Object(obj->start);
    return true;
}

bool RangeIter::next(Object& var)
{
    val += obj->step;
    if (val > obj->stop) return false;
    AS_INT(var) = val;
    return true;
}

ObjIter::ObjIter(Object& obj)
{
    switch (obj.type)
    {
        case OBJ_STRING:
            // Use emplace instead of assignment so we construct the
            // iterator in-place with no intermediate temporary object
            // (otherwise the temporary's destructor will mess with
            // the refcount).
            iter.emplace<StringIter>(static_cast<String*>(AS_HEAP_PTR(obj)));
            break;
        case OBJ_RANGE:
            iter.emplace<RangeIter>(static_cast<Range*>(AS_HEAP_PTR(obj)));
            break;
        default: break;
    }
}

bool ObjIter::start(Object& var)
{
    bool ret;
    std::visit([&var, &ret](auto&& iter) {
        ret = iter.start(var);
    }, iter);

    return ret;
}

bool ObjIter::next(Object& var)
{
    bool ret;
    std::visit([&var, &ret](auto&& iter) {
        ret = iter.next(var);
    }, iter);

    return ret;
}


/* Type Mismatch Error Class.*/

TypeMismatch::TypeMismatch(const std::string& message, ObjType expect,
    ObjType actual) :
    message(message), expect(expect), actual(actual) {}

static std::string_view objTypes[] = {
    "int", "dec", "bool", "null", "bigint", "bigdec",
    "string", "range", "list", "table", "num", "iterable"
};

void TypeMismatch::report()
{    
    FORMAT_PRINT(stderr,
        "Type mismatch: Expected type ({}) but found ({}) instead.\n",
        objTypes[expect], objTypes[actual]
    );
    FORMAT_PRINT(stderr, "{:>15}{}\n", "", message);
}