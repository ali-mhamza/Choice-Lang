#include "../include/object.h"
#include "../include/bytecode.h"
#include "../include/common.h"
#include "../include/config.h"
#include "../include/linear_alloc.h"
#include "../include/natives.h"
#include <array>
#include <climits>  // For CHAR_BIT, SIZE_MAX.
#include <cstddef>  // For size_t.
#include <cstdio>   // For stderr.
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <variant>
using Natives::funcNames;

#ifndef SIZE_MAX
    #include <limits>
    #define SIZE_MAX std::numeric_limits<std::size_t>::max()
#endif

constexpr std::array<std::string_view, NUM_TYPES> objTypes{
    "Int", "Dec", "Boolean", "Null", "Type", "Builtin",
    "Function", "Function", "Lambda", "BigInt",
    "BigDec", "String", "Range", "List", "Table",
    "", // References take the type of the contained object.
    "Tuple", "Iterable", "Num", "Comparable"
};

/* Object. */

Object::Object() :
    type{OBJ_INVALID}
{
    AS_INT(*this) = 0;
}

#if !CH_USE_ALLOC

void Object::clean()
{
    #if !CH_USE_ALLOC
        if (IS_HEAP_OBJ(*this))
        {
            HeapObj* temp{AS_HEAP_PTR(*this)};
            CH_ASSERT(temp != nullptr, "NULL object pointer.");

            CH_ASSERT(temp->refCount != 0, "Zero object refcount.");

            temp->refCount--;
            if (temp->refCount == 0) delete temp;
        }
        else if (IS_ITER(*this))
        {
            ObjIter* iter{AS_ITER(*this)};
            CH_ASSERT(iter != nullptr, "NULL iterator pointer.");
            delete iter; // We never copy iterators, so no refcount.
        }
    #endif
}

Object::Object(const Object& other) noexcept :
    type{other.type}, as{other.as}
{
    CH_ASSERT(!IS_ITER(other), "Copying an iterator is not allowed.");

    #if !CH_USE_ALLOC
        if (IS_HEAP_OBJ(*this))
            AS_HEAP_PTR(*this)->refCount++;
    #endif
}

Object& Object::operator=(const Object& other) noexcept
{
    CH_ASSERT(!IS_ITER(other), "Copying an iterator is not allowed.");

    if (this != &other)
    {
        clean();

        this->type = other.type;
        this->as = other.as;

        #if !CH_USE_ALLOC
            if (IS_HEAP_OBJ(*this))
                AS_HEAP_PTR(*this)->refCount++;
        #endif
    }

    return *this;
}

Object::Object(Object&& other) noexcept :
    type{other.type}, as{other.as}
{
    other.type = OBJ_INVALID; // To prevent deallocation when it is destroyed.
    AS_INT(other) = 0;
}

Object& Object::operator=(Object&& other) noexcept
{
    if (this != &other)
    {
        clean();
        
        this->type = other.type;
        this->as = other.as;

        other.type = OBJ_INVALID;
        AS_INT(other) = 0;
    }

    return *this;
}

Object::~Object()
{
    clean();
}

#endif

bool Object::operator==(const Object& other) const
{
    if (IS_NUM(*this) && IS_NUM(other))
        return double(AS_NUM(*this)) == double(AS_NUM(other));
    if (this->type != other.type) return false;

    switch (this->type)
    {
        case OBJ_BOOL:      return AS_BOOL(*this) == AS_BOOL(other);
        case OBJ_NULL:      return true;
        case OBJ_TYPE:      return AS_TYPE(*this) == AS_TYPE(other);
        case OBJ_NATIVE:    return AS_NATIVE(*this) == AS_NATIVE(other);
        case OBJ_FUNC:
        case OBJ_LAMBDA:    return *(AS_FUNC(*this)) == *(AS_FUNC(other));
        case OBJ_CLOSURE:   return *(AS_CLOSURE(*this)) == *(AS_CLOSURE(other));
        case OBJ_STRING:    return *(AS_STRING(*this)) == *(AS_STRING(other));
        case OBJ_RANGE:     return *(AS_RANGE(*this)) == *(AS_RANGE(other));
        case OBJ_LIST:      return *(AS_LIST(*this)) == *(AS_LIST(other));
        // TODO: Tuples shouldn't be comparable.
        default: CH_UNREACHABLE();
    }
}

bool Object::operator>(const Object& other) const
{
    if (IS_NUM(*this) && IS_NUM(other))
        return AS_NUM(*this) > AS_NUM(other);
    else if (IS_STRING(*this) && IS_STRING(other))
    {
        const auto& str1{AS_STRING(*this)->str};
        const auto& str2{AS_STRING(other)->str};
        return (str1.compare(str2) > 0);
    }

    CH_ASSERT(false, "Invalid operand types passed to operator.");
    CH_UNREACHABLE(); // Remains in release builds.
}

bool Object::operator<(const Object& other) const
{
    if (IS_NUM(*this) && IS_NUM(other))
        return AS_NUM(*this) < AS_NUM(other);
    else if (IS_STRING(*this) && IS_STRING(other))
    {
        const auto& str1{AS_STRING(*this)->str};
        const auto& str2{AS_STRING(other)->str};
        return (str1.compare(str2) < 0);
    }

    CH_ASSERT(false, "Invalid operand types passed to operator.");
    CH_UNREACHABLE();
}

bool Object::in(const Object& other) const
{
    const Object& obj{*this};    

    if (IS_STRING(obj) && IS_STRING(other))
    {
        const String& s1{*(AS_STRING(obj))};
        const String& s2{*(AS_STRING(other))};
        return s2.contains(s1);
    }
    else if (IS_INT(obj) && IS_RANGE(other))
    {
        const Range& range{*(AS_RANGE(other))};
        return range.contains(AS_INT(obj));
    }
    else  if (IS_LIST(other))
    {
        const List& list{*(AS_LIST(other))};
        return list.contains(obj);
    }
    else if (!IS_STRING(obj) && !IS_RANGE(other))
    {
        throw TypeMismatch(
            "Right operand must be an iterable object.",
            OBJ_ITER,
            other.type
        );
    }
    else
    {
        throw TypeMismatch(
            "Left operand not matching member type of iterable object.",
            (other.type == OBJ_STRING ? OBJ_STRING : OBJ_INT),
            obj.type
        );
    }
}

static std::string doubleToStr(double d)
{
    auto output{CH_STR("{:.6f}", d)};

    while (output.back() == '0')
        output.pop_back();

    // Remove the '.' if no decimals to print.
    if (output.back() == '.')
        output.pop_back();

    return output;
}

std::string Object::printVal() const
{
    switch (type)
    {
        case OBJ_INT:       return std::to_string(AS_INT(*this));
        case OBJ_DEC:       return doubleToStr(AS_DEC(*this));
        case OBJ_BOOL:      return (AS_BOOL(*this) ? "true" : "false");
        case OBJ_NULL:      return "null";
        case OBJ_TYPE:      return std::string(objTypes[AS_TYPE(*this)]);
        case OBJ_NATIVE:    return CH_STR("<builtin {}>", funcNames[AS_NATIVE(*this)]);
        case OBJ_FUNC:      return CH_STR("<func {}>", AS_FUNC(*this)->name);
        case OBJ_CLOSURE:   return CH_STR("<func {}>", AS_CLOSURE(*this)->function->name);
        case OBJ_LAMBDA:    return "<lambda>";
        case OBJ_STRING:    return AS_STRING(*this)->printVal();
        case OBJ_RANGE:     return AS_RANGE(*this)->printVal();
        case OBJ_LIST:      return AS_LIST(*this)->printVal();
        case OBJ_TUPLE:     return AS_TUPLE(*this)->printVal();
        case OBJ_ITER:
        {
            const auto& iter{AS_ITER(*this)->iter};
            std::string ret{};
            std::visit([&ret](auto&& iter) {
                ret = "->" + iter.obj->printVal();
            }, iter);

            return ret;
        }
        default: CH_UNREACHABLE();
    }
}

std::string_view Object::printType() const
{
    return objTypes[type];
}

template<typename T>
static void emitBytes(std::ofstream& os, ObjType type, T value)
{
    if (type != OBJ_INVALID)
        os.put(static_cast<char>(type));
    constexpr size_t size{sizeof(T)};
    ui64* asBytes{reinterpret_cast<ui64*>(&value)};
    std::array<char, size> bytes{};
    for (size_t i = 0; i < size; i++)
        bytes[i] = (*asBytes >> ((size - 1 - i) * CHAR_BIT)) & CODE_MAX;
    os.write(bytes.data(), size);
}

void Object::emit(std::ofstream& os) const
{
    switch (type)
    {
        case OBJ_INT:       emitBytes(os, OBJ_INT, AS_INT(*this));    break;
        case OBJ_DEC:       emitBytes(os, OBJ_DEC, AS_DEC(*this));    break;
        case OBJ_FUNC:
        case OBJ_LAMBDA:    AS_FUNC(*this)->emit(os);                 break;
        case OBJ_STRING:    AS_STRING(*this)->emit(os);               break;
        default: break;
    }
}

ObjIter* Object::makeIter()
{
    if (!IS_ITERABLE(*this)) return nullptr;
    return CH_ALLOC(ObjIter, *this);
}


/* HeapObj. */

HeapObj::HeapObj() :
    type{OBJ_INVALID} {}

HeapObj::HeapObj(ObjType type) :
    type{type} {}

Cell::Cell(Object* location) :
    HeapObj{OBJ_REF},
    location{location} {}

void Cell::close()
{
    obj = *location;
    location = &obj;
}

Function::Function(const ByteCode& code, const ui8 argCount) :
    HeapObj{OBJ_LAMBDA},
    name{nullptr}, code{code}, argCount{argCount}, lambda{true} {}

// strdup is not a standard C++ function, but is instead from POSIX.
static char* choiceStrdup(const char* str)
{
    auto size{strlen(str) + 1};
    char* ret{new char[size]};
    memcpy(ret, str, size); // Includes null byte.
    return ret;
}

Function::Function(
    const std::string& name,
    const ByteCode& code,
    const ui8 argCount
) : HeapObj{OBJ_FUNC},
    name{choiceStrdup(name.c_str())}, code{code}, argCount{argCount},
    lambda{false} {}

Function::~Function()
{   
    delete[] name;
}

bool Function::operator==(const Function& other) const
{
    return (this == &other);
}

void Function::emit(std::ofstream& os) const
{
    os.put(static_cast<char>(type));
    if (name != nullptr) os.write(name, strlen(name));
    os.put('\0');

    os.put(static_cast<char>(argCount));
    os.put(static_cast<char>(lambda));

    const vByte& block{code.block};
    emitBytes<ui64>(os, OBJ_INVALID, block.size());
    emitBytes<ui64>(os, OBJ_INVALID, code.countPool());
    os.write(reinterpret_cast<const char*>(block.data()),
		block.size());

	// Constant pool.
    const vObj& pool{code.pool};
	for (const Object& constant : pool)
		constant.emit(os);
}

Closure::Closure(Function* function) :
    HeapObj{OBJ_CLOSURE},
    function{function}
{
    #if !CH_USE_ALLOC
        function->refCount++;
    #endif
}

Closure::~Closure()
{
    #if !CH_USE_ALLOC
        for (Cell* cell : cells)
        {
            cell->refCount--;
            if (cell->refCount == 0)
                delete cell;
        }

        function->refCount--;
        if (function->refCount == 0)
            delete function;
    #endif
}

bool Closure::operator==(const Closure& other) const
{
    return (this == &other);
}

void Closure::addCell(Cell* cell)
{
    #if !CH_USE_ALLOC
        cell->refCount++;
    #endif
    cells.push(cell);
}

String::String(const std::string& str) :
    HeapObj{OBJ_STRING},
    str{str} {}

String::String(const std::string_view& view) :
    HeapObj{OBJ_STRING},
    str{view} {}

String::String(const char* str, size_t len) :
    HeapObj{OBJ_STRING}
{
    len = (len == SIZE_MAX ? strlen(str) : len);
    this->str = std::string{str, len};
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
    HeapObj{OBJ_RANGE},
    start{limits[0]}, stop{limits[1]}, step{limits[2]} {}

bool Range::operator==(const Range& other) const
{
    return ((this->start == other.start)
            && (this->stop == other.stop)
            && (this->step == other.step));
}

bool Range::contains(const i64 num) const
{
    if (start <= stop)
    {
        for (i64 i{start}; i <= stop; i += step)
        {
            if (num == i)
                return true;
        }
    }
    else
    {
        for (i64 i{start}; i >= stop; i -= step)
        {
            if (num == i)
                return true;
        }
    }

    return false;
}

i64 Range::length() const
{
    if (step == 1)
    {
        if (start <= stop)
            return stop - start + 1;
        else
            return start - stop + 1;
    }

    i64 temp{start};
    i64 len{0};
    if (start <= stop)
    {
        while (temp <= stop)
        {
            len++;
            temp += step;
        }
    }
    else
    {
        while (temp >= stop)
        {
            len++;
            temp -= step;
        }
    }

    return len;
}

std::string Range::printVal() const
{
    auto str{CH_STR("{}..{}", start, stop)};
    if (step != 1)
        str += CH_STR("..{}", step);
    return str;
}

List::List(ui32 size) :
    HeapObj{OBJ_LIST},
    array{size} {}

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
    std::string ret{"["};
    size_t size{array.count()};
    for (size_t i{0}; i < size; i++)
    {
        ret += array[i].printVal();
        if (i != size - 1)
            ret += ", ";
    }

    ret += "]";
    return ret;
}

Tuple::Tuple() :
    HeapObj{OBJ_TUPLE} {}

Tuple::Tuple(ui32 size) :
    HeapObj{OBJ_TUPLE},
    entries{size} {}

std::string Tuple::printVal() const
{
    std::string ret{"("};
    size_t size{entries.count()};
    for (size_t i{0}; i < size; i++)
    {
        ret += entries[i].printVal();
        if (i != size - 1)
            ret += ", ";
    }

    ret += ")";
    return ret;
}


/* Object iterator struct types. */

StringIter::StringIter(String* obj) :
    obj{obj}, begin{obj->str.c_str()}
{
    #if !CH_USE_ALLOC
        obj->refCount++;
    #endif
}

StringIter::StringIter(StringIter&& other) noexcept :
    obj{other.obj}, begin{other.begin}
{
    other.obj = nullptr;
    other.begin = nullptr; // Must split; pointers are of different types.
}

StringIter& StringIter::operator=(StringIter&& other) noexcept
{
    if (this != &other)
    {
        this->obj = other.obj;
        this->begin = other.begin;

        other.obj = nullptr;
        other.begin = nullptr;
    }

    return *this;
}

StringIter::~StringIter()
{
    #if !CH_USE_ALLOC
        if (obj != nullptr)
        {
            CH_ASSERT(obj->refCount != 0, "Zero iterable refcount.");
            obj->refCount--;
            if (obj->refCount == 0) delete obj;
        }
    #endif
}

bool StringIter::start(Object& var)
{
    if (obj->str.size() == 0) return false;
    var = Object{CH_ALLOC(String, begin, 1)};
    return true;
}

bool StringIter::next(Object& var)
{
    begin++;
    if (*begin == '\0')
        return false;
    var = Object{CH_ALLOC(String, begin, 1)};
    return true;
}

RangeIter::RangeIter(Range* obj) :
    obj{obj}
{
    #if !CH_USE_ALLOC
        obj->refCount++;
    #endif
}

RangeIter::RangeIter(RangeIter&& other) noexcept :
    obj{other.obj}, val{other.val}
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
    #if !CH_USE_ALLOC
        if (obj != nullptr)
        {
            CH_ASSERT(obj->refCount != 0, "Zero iterable refcount.");
            obj->refCount--;
            if (obj->refCount == 0) delete obj;
        }
    #endif
}

bool RangeIter::start(Object& var)
{
    val = obj->start;
    var = Object{val};
    return true;
}

bool RangeIter::next(Object& var)
{
    bool reverse{obj->start > obj->stop};
    val += (reverse ? -1 : 1) * obj->step;
    if ((!reverse && (val > obj->stop))
        || (reverse && (val < obj->stop)))
    {
        return false;   
    }
    AS_INT(var) = val;
    return true;
}

ListIter::ListIter(List* obj) :
    obj{obj}
{   
    #if !CH_USE_ALLOC
        obj->refCount++;
    #endif
}

ListIter::ListIter(ListIter&& other) noexcept :
    obj{other.obj}, it{other.it}
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
    #if !CH_USE_ALLOC
        if (obj != nullptr)
        {
            CH_ASSERT(obj->refCount != 0, "Zero iterable refcount.");
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
            iter.emplace<StringIter>(AS_STRING(obj));
            break;
        case OBJ_RANGE:
            iter.emplace<RangeIter>(AS_RANGE(obj));
            break;
        case OBJ_LIST:
            iter.emplace<ListIter>(AS_LIST(obj));
            break;
        default: break;
    }
}

bool ObjIter::start(Object& var)
{
    bool ret{};
    std::visit([&var, &ret](auto&& iter) {
        ret = iter.start(var);
    }, iter);

    return ret;
}

bool ObjIter::next(Object& var)
{
    bool ret{};
    std::visit([&var, &ret](auto&& iter) {
        ret = iter.next(var);
    }, iter);

    return ret;
}

/* Type mismatch error class.*/

TypeMismatch::TypeMismatch(const std::string& message, ObjType expect,
    ObjType actual) :
    message{message}, expect{expect}, actual{actual} {}

#if defined(DEBUG)
    #define LENGTH(array) sizeof(array) / sizeof(array[0])
    #define IS_OBJ(type) \
        (((type) >= 0) && ((type) < LENGTH(objTypes)))
#endif

void TypeMismatch::report()
{
    CH_ASSERT(IS_OBJ(expect) && IS_OBJ(actual),
        "Invalid object type for error reporting.");

    CH_PRINT(stderr,
        "Type mismatch: Expected ({}) but found ({}) instead.\n",
        objTypes[expect], objTypes[actual]
    );
    CH_PRINT(stderr, "{:>15}{}\n", "", message);
}