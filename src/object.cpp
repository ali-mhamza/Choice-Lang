#include "../include/object.h"
#include "../include/linear_alloc.h"
#include "../include/natives.h"
#include "../include/token.h"
#include <climits> // For CHAR_BIT.
#include <cstdio> // For stderr.
#include <cstring>
#include <string_view>
#include <type_traits>
using Natives::funcNames;

static std::string_view objTypes[] = {
    "int", "dec", "bool", "null", "type", "builtin function",
    "user function", "bigint", "bigdec", "string", "range",
    "list", "table", "tuple", "num", "iterable"
};

/* Object. */

Object::Object() :
    type(OBJ_INVALID)
{
    AS_(int, *this) = 0;
}

void Object::clean()
{
    #if !USE_ALLOC
        if (IS_HEAP_OBJ(*this))
        {
            HeapObj* temp = AS_HEAP_PTR(*this);
            ASSERT(temp != nullptr, "NULL object pointer");

            ASSERT(temp->refCount != 0, "Zero object refcount");

            temp->refCount--;
            if (temp->refCount == 0) delete temp;
        }
        else if (IS_(ITER, *this))
        {
            ObjIter* iter = AS_(iter, *this);
            ASSERT(iter != nullptr, "NULL iterator pointer");
            delete iter; // We never copy iterators, so no refcount.
        }
    #endif
}

Object::Object(const Object& other) noexcept :
    type(other.type), as(other.as)
{
    ASSERT(!IS_(ITER, other), "Copying an iterator is not allowed");

    #if !USE_ALLOC
        if (IS_HEAP_OBJ(*this))
            AS_HEAP_PTR(*this)->refCount++;
    #endif
}

Object& Object::operator=(const Object& other) noexcept
{
    ASSERT(!IS_(ITER, other), "Copying an iterator is not allowed");
    
    if (this != &other)
    {
        clean();
        
        this->type = other.type;
        this->as = other.as;

        #if !USE_ALLOC
            if (IS_HEAP_OBJ(*this))
                AS_HEAP_PTR(*this)->refCount++;
        #endif
    }

    return *this;
}

Object::Object(Object&& other) noexcept :
    type(other.type), as(other.as)
{
    other.type = OBJ_INVALID; // To prevent deallocation when it is destroyed.
    AS_(int, other) = 0;
}

Object& Object::operator=(Object&& other) noexcept
{
    if (this != &other)
    {
        clean();
        
        this->type = other.type;
        this->as = other.as;

        other.type = OBJ_INVALID;
        AS_(int, other) = 0;
    }

    return *this;
}

Object::~Object()
{
    clean();
}

bool Object::operator==(const Object& other) const
{
    if (IS_NUM(*this) && IS_NUM(other))
        return double(AS_NUM(*this)) == double(AS_NUM(other));
    if (this->type != other.type) return false;

    switch (this->type)
    {
        case OBJ_BOOL:      return AS_(bool, *this) == AS_(bool, other);
        case OBJ_NULL:      return true;
        case OBJ_TYPE:      return AS_(type, *this) == AS_(type, other);
        case OBJ_NATIVE:    return AS_(native, *this) == AS_(native, other);
        case OBJ_FUNC:      return AS_(func, *this) == AS_(func, other);
        case OBJ_STRING:    return AS_(string, *this) == AS_(string, other);
        case OBJ_RANGE:     return AS_(range, *this) == AS_(range, other);
        case OBJ_LIST:      return AS_(list, *this) == AS_(list, other);
        // TODO: Tuples shouldn't be comparable.
        default: UNREACHABLE();
    }
}

bool Object::operator>(const Object& other) const
{
    ASSERT(IS_NUM(*this) && IS_NUM(other),
        "Invalid operand types passed to operator.");
    return AS_NUM(*this) > AS_NUM(other);
}

bool Object::operator<(const Object& other) const
{
    ASSERT(IS_NUM(*this) && IS_NUM(other),
        "Invalid operand types passed to operator.");
    return AS_NUM(*this) < AS_NUM(other);
}

std::string Object::printVal() const
{
    switch (type)
    {
        case OBJ_INT:       return std::to_string(AS_(int, *this));
        case OBJ_DEC:       return std::to_string(AS_(dec, *this));
        case OBJ_BOOL:      return (AS_(bool, *this) ? "true" : "false");
        case OBJ_NULL:      return "null";
        case OBJ_TYPE:      return std::string(objTypes[AS_(type, *this)]);
        case OBJ_NATIVE:    return FORMAT_STR("<builtin {}>", funcNames[AS_(native, *this)]);
        case OBJ_FUNC:
        {
            Function* func = AS_(func, *this);
            if (func->lambda)
                return "<lambda>";
            return FORMAT_STR("<func {}>", func->name);
        }
        case OBJ_STRING:    return AS_(string, *this)->printVal();
        case OBJ_RANGE:     return AS_(range, *this)->printVal();
        case OBJ_LIST:      return AS_(list, *this)->printVal();
        case OBJ_TUPLE:     return AS_(tuple, *this)->printVal();
        case OBJ_ITER:
        {
                const auto& iter = AS_(iter, *this)->iter;
                std::string ret;
                std::visit([&ret](auto&& iter) {
                    ret = "->" + iter.obj->printVal();
                }, iter);

                return ret;
        }
        default: UNREACHABLE();
    }
}

std::string_view Object::printType() const
{
    if (IS_(FUNC, *this) && AS_(func, *this)->lambda)
        return "lambda";
    return objTypes[type];
}

template<typename T>
static void emitBytes(std::ofstream& os, ObjType type, T value)
{
    if (type != OBJ_INVALID)
        os.put(static_cast<char>(type));
    constexpr size_t size = sizeof(T);
    ui64* asBytes = reinterpret_cast<ui64*>(&value);
    char bytes[size];
    for (size_t i = 0; i < size; i++)
        bytes[i] = (*asBytes >> ((size - 1 - i) * CHAR_BIT)) & 0xff;
    os.write(&bytes[0], size);
}

void Object::emit(std::ofstream& os) const
{
    switch (type)
    {
        case OBJ_INT:       emitBytes(os, OBJ_INT, AS_(int, *this));    break;
        case OBJ_DEC:       emitBytes(os, OBJ_DEC, AS_(dec, *this));    break;
        case OBJ_FUNC:      AS_(func, *this)->emit(os);                 break;
        case OBJ_STRING:    AS_(string, *this)->emit(os);               break;
        case OBJ_RANGE:     AS_(range, *this)->emit(os);                break;
        default: break;
    }
}

ObjIter* Object::makeIter()
{
    if (!IS_ITERABLE(*this)) return nullptr;
    return ALLOC(ObjIter, ObjDealloc<ObjIter>, *this);
}


/* HeapObj. */

HeapObj::HeapObj() :
    type(OBJ_INVALID), refCount(0) {}

HeapObj::HeapObj(ObjType type) :
    type(type), refCount(0) {}

Function::Function(const ByteCode& code, ui8 argCount) :
    HeapObj(OBJ_FUNC),
    name(""), code(code), argCount(argCount), lambda(true) {}

#if defined(__GNUC__) || defined(__clang__)
    #define STRDUP strdup
#elif defined(_MSC_VER)
    #define STRDUP _strdup
#endif

Function::Function(const std::string& name, const ByteCode& code,
    ui8 argCount) :
    HeapObj(OBJ_FUNC), name(STRDUP(name.c_str())), code(code),
    argCount(argCount), lambda(false) {}

bool Function::operator==(const Function& other) const
{
    return (this == &other);
}

void Function::emit(std::ofstream& os) const
{
    os.put(static_cast<char>(type));
    os.write(name, strlen(name));
    os.put('\0');

    os.put(static_cast<char>(argCount));
    os.put(static_cast<char>(lambda));

    const vByte& block = code.block;
    emitBytes<ui64>(os, OBJ_INVALID, block.size());
    emitBytes<ui64>(os, OBJ_INVALID, code.countPool());
    os.write(reinterpret_cast<const char*>(block.data()),
		block.size());

	// Constant pool.
    const vObj& pool = code.pool;
	for (const Object& constant : pool)
		constant.emit(os);
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

bool String::operator==(const String& other) const
{
    return (this->str == other.str);
}

bool String::contains(const String& substr) const
{
    return (strstr(this->str.c_str(), substr.str.c_str()) != nullptr);
}

std::string String::printVal() const
{
    return str;
}

void String::emit(std::ofstream& os) const
{
    os.put(static_cast<char>(type));
    os.write(str.data(), str.size());
    os.put('\0');
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

bool Range::contains(const i64 num) const
{
    for (i64 i = start; i <= stop; i += step)
    {
        if (num == i)
            return true;
    }

    return false;
}

std::string Range::printVal() const
{
    auto retStr = FORMAT_STR("{}..{}", start, stop);
    if (step != 1)
        retStr += FORMAT_STR("..{}", step);
    return retStr;
}

void Range::emit(std::ofstream& os) const
{
    os.put(static_cast<char>(type));
    emitBytes(os, OBJ_INVALID, start);
    emitBytes(os, OBJ_INVALID, stop);
    emitBytes(os, OBJ_INVALID, step);
}

List::List(ui32 size) :
    array(size) {}

bool List::operator==(const List& other) const
{
    // For now: comparing identity.
    return (this == &other);
}

bool List::contains(const Object& obj) const
{   
    for (const Object& entry : array)
    {
        if (entry == obj)
            return true;
    }

    return false;
}

std::string List::printVal() const
{
    std::string ret = "[";
    size_t size = array.count();
    for (size_t i = 0; i < size; i++)
    {
        ret += array[i].printVal();
        if (i != size - 1)
            ret += ", ";
    }

    ret += "]";
    return ret;
}

Tuple::Tuple(ui32 size) :
    entries(size) {}

std::string Tuple::printVal() const
{
    std::string ret = "(";
    size_t size = entries.count();
    for (size_t i = 0; i < size; i++)
    {
        ret += entries[i].printVal();
        if (i != size - 1)
            ret += ", ";
    }

    ret += ")";
    return ret;
}


/* Object iterator struct types. */

StringIter::StringIter() :
    obj(nullptr), iter(nullptr), begin(nullptr) {}

StringIter::StringIter(String* obj) :
    obj(obj), iter(nullptr), begin(obj->str.c_str())
{
    #if !USE_ALLOC
        obj->refCount++;
    #endif
}

StringIter::StringIter(StringIter&& other) noexcept :
    obj(other.obj), iter(other.iter), begin(other.begin)
{
    other.obj = other.iter = nullptr;
    other.begin = nullptr; // Must split; pointers are of different types.
}

StringIter& StringIter::operator=(StringIter&& other) noexcept
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
    #if !USE_ALLOC
        if (obj != nullptr)
        {
            ASSERT(obj->refCount != 0, "Zero iterable refcount");
            obj->refCount--;
            if (obj->refCount == 0) delete obj;
        }
        if (iter != nullptr)
        {
            iter->refCount--;
            if (iter->refCount == 0) delete iter;
        }
    #endif
}

bool StringIter::start(Object& var)
{
    if (obj->str.size() == 0) return false;
    iter = ALLOC(String, ObjDealloc<String>, begin, 1);
    #if !USE_ALLOC
        iter->refCount++;
    #endif
    var = Object(iter);
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
    #if !USE_ALLOC
        obj->refCount++;
    #endif
}

RangeIter::RangeIter(RangeIter&& other) noexcept :
    obj(other.obj), val(other.val)
{
    other.obj = nullptr;
}

RangeIter& RangeIter::operator=(RangeIter&& other) noexcept
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
    #if !USE_ALLOC
        if (obj != nullptr)
        {
            ASSERT(obj->refCount != 0, "Zero iterable refcount");
            obj->refCount--;
            if (obj->refCount == 0) delete obj;
        }
    #endif
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
    AS_(int, var) = val;
    return true;
}

ListIter::ListIter() :
    obj(nullptr), it() {}

ListIter::ListIter(List* obj) :
    obj(obj), it()
{   
    #if !USE_ALLOC
        obj->refCount++;
    #endif
}

ListIter::ListIter(ListIter&& other) noexcept :
    obj(other.obj), it(other.it)
{
    other.obj = nullptr;
}

ListIter& ListIter::operator=(ListIter&& other) noexcept
{
    if (this != &other)
    {
        this->obj = other.obj;
        this->it = other.it;

        other.obj = nullptr;
    }

    return *this;
}

ListIter::~ListIter()
{
    #if !USE_ALLOC
        if (obj != nullptr)
        {
            ASSERT(obj->refCount != 0, "Zero iterable refcount");
            obj->refCount--;
            if (obj->refCount == 0) delete obj;
        }
    #endif
}

bool ListIter::start(Object& var)
{
    if (obj->array.count() == 0)
        return false;

    it = obj->array.begin();
    var = *it;
    return true;
}

bool ListIter::next(Object& var)
{
    if (++it == obj->array.end())
        return false;

    var = *it;
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
            iter.emplace<StringIter>(AS_(string, obj));
            break;
        case OBJ_RANGE:
            iter.emplace<RangeIter>(AS_(range, obj));
            break;
        case OBJ_LIST:
            iter.emplace<ListIter>(AS_(list, obj));
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

#if defined(DEBUG)
    #define LENGTH(array) sizeof(array) / sizeof(array[0])
    #define IS_OBJ(type) \
        (((type) >= 0) && ((type) < LENGTH(objTypes)))
#endif

void TypeMismatch::report()
{
    ASSERT(IS_OBJ(expect), "Invalid object type for error reporting");
    ASSERT(IS_OBJ(actual), "Invalid object type for error reporting");
    
    FORMAT_PRINT(stderr,
        "Type mismatch: Expected type ({}) but found ({}) instead.\n",
        objTypes[expect], objTypes[actual]
    );
    FORMAT_PRINT(stderr, "{:>15}{}\n", "", message);
}