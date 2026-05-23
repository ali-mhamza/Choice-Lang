#include "../include/object.h"
#include "../include/bytecode.h"
#include "../include/bytes.h"
#include "../include/common.h"
#include "../include/diagnostic.h"
#include "../include/error.h"
#include "../include/linear_alloc.h"
#include "../include/natives.h"
#include <personal/hashFunctions.h>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <ios>
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
    "Void", "Iterable"
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
        case OBJ_TABLE:     return *(AS_TABLE(*this)) == *(AS_TABLE(other));
        case OBJ_VOID:      return true;
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
        return AS_RANGE(other)->contains(AS_INT(obj));
    else if (IS_LIST(other))
        return AS_LIST(other)->contains(obj);
    else if (IS_TABLE(other))
        return AS_TABLE(other)->contains(obj);

    else if (!IS_STRING(obj) && !IS_RANGE(other))
        throw reportCollection(OBJ_NOT_ITERABLE, other, obj);
    else
        throw reportCollection(OBJ_WRONG_ITER_TYPE, other, obj);
}

Object Object::getIndex(const Object& index)
{
    CH_ASSERT(IS_COLLECTION(*this), "Incorrect object type for index operator.");

    switch (this->type)
    {
        case OBJ_STRING:    return AS_STRING(*this)->getIndex(index);
        case OBJ_LIST:      return AS_LIST(*this)->getIndex(index);
        case OBJ_TABLE:     return AS_TABLE(*this)->getIndex(index);
        default: CH_UNREACHABLE();
    }
}

void Object::setIndex(const Object& index, const Object& value)
{
    CH_ASSERT(IS_COLLECTION(*this), "Incorrect object type for index operator.");

    switch (this->type)
    {
        case OBJ_STRING:    return AS_STRING(*this)->setIndex(index, value);
        case OBJ_LIST:      return AS_LIST(*this)->setIndex(index, value);
        case OBJ_TABLE:     return AS_TABLE(*this)->setIndex(index, value);
        default: CH_UNREACHABLE();
    }
}

[[nodiscard]] static std::string doubleToStr(double d)
{
    auto output{CH_STR("{:.6f}", d)};

    while (output.back() == '0')
        output.pop_back();

    // Remove the '.' if no decimals to print.
    if (output.back() == '.')
        output.pop_back();

    return output;
}

template<typename T>
[[nodiscard]] static Hash hashPointer(T* ptr)
{
    const u8* temp{reinterpret_cast<const u8*>(ptr)};
    return hashBytes(temp, sizeof(T));
}

Hash Object::hash() const
{
    switch (type)
    {
        case OBJ_INT:       return hashKey(AS_INT(*this));
        case OBJ_DEC:       return hashKey(AS_DEC(*this));
        case OBJ_BOOL:      return hashKey(AS_BOOL(*this));
        case OBJ_NULL:      return 0;
        case OBJ_TYPE:      return hashKey(static_cast<u8>(AS_TYPE(*this)));
        case OBJ_NATIVE:    return hashKey(static_cast<u8>(AS_NATIVE(*this)));
        case OBJ_FUNC:      return hashPointer(AS_FUNC(*this));
        case OBJ_CLOSURE:   return hashPointer(AS_CLOSURE(*this));
        case OBJ_LAMBDA:    return hashPointer(AS_FUNC(*this));
        case OBJ_STRING:    return hashKey(AS_STRING(*this)->str);
        case OBJ_RANGE:
        {
            const Range* range{AS_RANGE(*this)};
            return hashKey(range->start) + hashKey(range->stop)
                + hashKey(range->step);
        }
        // For now, at least.
        case OBJ_LIST:      return hashPointer(AS_LIST(*this));
        case OBJ_TABLE:     return hashPointer(AS_TABLE(*this));
        case OBJ_REF:       return AS_REF(*this)->location->hash();
        case OBJ_VOID:      return 0;
        default: CH_UNREACHABLE();
    }
}

// Need to support internal types in this function as well
// since this is used for register printing in debug builds.
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
        case OBJ_TABLE:     return AS_TABLE(*this)->printVal();
        case OBJ_REF:       return CH_STR("*({})", AS_REF(*this)->location->printVal());
        case OBJ_VOID:      return "()";
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

void Object::emit(std::ofstream& os) const
{
    os.put(static_cast<char>(type));

    switch (type)
    {
        case OBJ_INT:       Bytes::encodeValue(os, AS_INT(*this));  break;
        case OBJ_DEC:       Bytes::encodeValue(os, AS_DEC(*this));  break;
        case OBJ_FUNC:
        case OBJ_LAMBDA:    AS_FUNC(*this)->emit(os);               break;
        case OBJ_STRING:    AS_STRING(*this)->emit(os);             break;
        default: CH_UNREACHABLE();
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

Function::Function(const ByteCode& code, const u8 argCount) :
    HeapObj{OBJ_LAMBDA},
    name{nullptr}, code{code}, argCount{argCount}, lambda{true} {}

// strdup is not a standard C++ function, but is instead from POSIX.
[[nodiscard]] static char* choiceStrdup(const char* str)
{
    auto size{strlen(str) + 1};
    char* ret{new char[size]};
    memcpy(ret, str, size); // Includes null byte.
    return ret;
}

Function::Function(
    const std::string& name,
    const ByteCode& code,
    const u8 argCount
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

const DebugRange& Function::getErrorRange(const u8 *ip) const
{
    CH_ASSERT(ip >= code.block.data(), "Wrong IP passed for error reporting.");

    u64 offset{static_cast<u64>(ip - code.block.data())};
    for (const auto& range : code.metadata)
    {
        if ((offset >= range.byteStart) && (offset <= range.byteEnd))
            return range;
    }

    CH_UNREACHABLE();
}

void Function::emit(std::ofstream& os) const
{
    if (name != nullptr)
    {
        u64 len{strlen(name)};
        os.put(static_cast<char>(len));
        os.write(name, static_cast<std::streamsize>(len));
    }
    else
        os.put(static_cast<char>(0));

    os.put(static_cast<char>(argCount));
    os.put(static_cast<char>(lambda));

    code.encodeData(os);
    if (debugInfoState == DEBUG_COMBINED)
        code.encodeMetadata(os);
}

u64 Function::byteSize() const
{
    u64 size{0};

    if (name != nullptr) size += strlen(name);

    // Added type byte (1) and name length byte (1)
    // and argCount byte (1) and lambda Boolean byte (1).
    size += 4 * sizeof(u8);

    // Added code size and pool size values,
    // as well as the actual sizes of the code and pool.
    size += 2 * sizeof(u64) + code.codeSize() + code.countPool();

    if (debugInfoState == DEBUG_COMBINED)
    {
        // Added metadata + metadata size value (8 bytes).
        size += (code.metadata.size() * sizeof(DebugRange)) + sizeof(u64);
    }

    return size;
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
    return (this->str.find(substr.str) != std::string::npos);
}

Object String::getIndex(const Object& index)
{
    if (!IS_INT(index))
        throw reportCollection(OBJ_NOT_INDEX, this, index);

    // Handle out-of-bound access.
    return CH_ALLOC(String, str.data() + AS_INT(index), 1);
}

void String::setIndex(const Object& index, const Object& value)
{
    (void) index; (void) value;

    // Does nothing for now.
    // Error reporting to be added later.
}

std::string String::printVal() const
{
    return str;
}

void String::emit(std::ofstream& os) const
{
    Bytes::encodeValue(os, static_cast<u64>(str.size()));
    os.write(str.data(), str.size());
}

u64 String::byteSize() const
{
    // Added type byte (1) and string length bytes (8).
    return sizeof(u8) + sizeof(u64) + str.size();
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

List::List(u32 size) :
    HeapObj{OBJ_LIST},
    array{size} {}

bool List::operator==(const List& other) const
{
    // For now: comparing identity.
    return (this == &other);
}

Object List::getIndex(const Object& index)
{
    if (!IS_INT(index))
        throw reportCollection(OBJ_NOT_INDEX, this, index);

    // Handle out-of-bound access.
    return array[AS_INT(index)];
}

void List::setIndex(const Object& index, const Object& value)
{
    if (!IS_INT(index))
        throw reportCollection(OBJ_NOT_INDEX, this, index);

    // Handle out-of-bound access.
    array[AS_INT(index)] = value;
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

Table::Table() :
    HeapObj{OBJ_TABLE}, table{} {}

bool Table::operator==(const Table& other) const
{
    // For now: comparing identity.
    return (this == &other);
}

bool Table::contains(const Object& obj) const
{
    return (table.contains(obj));
}

Object Table::getIndex(const Object& key)
{
    static Object empty{CH_ALLOC(Void)};

    Object* value{table.get(key)};
    if (value == nullptr)
        return empty;
    else
        return *value;
}

void Table::setIndex(const Object& key, const Object& value)
{
    table[key] = value;
}

std::string Table::printVal() const
{
    std::string ret{"{"};

    for (const auto& [key, value] : table)
        ret += "(" + key.printVal() + ", " + value.printVal() + "), ";

    if (table.empty())
        ret += "}";
    else
    {
        // Clear the last ", ".
        ret.pop_back();
        ret.back() = '}';
    }

    return ret;
}

Void::Void() :
    HeapObj{OBJ_VOID} {}


/* Object iterator struct types. */

StringIter::StringIter(String* obj) :
    obj{obj}, pos{0}
{
    #if !CH_USE_ALLOC
        obj->refCount++;
    #endif
}

StringIter::StringIter(StringIter&& other) noexcept :
    obj{other.obj}, pos{other.pos}
{
    other.obj = nullptr;
}

StringIter& StringIter::operator=(StringIter&& other) noexcept
{
    if (this != &other)
    {
        this->obj = other.obj;
        this->pos = other.pos;

        other.obj = nullptr;
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
    var = Object{CH_ALLOC(String, &(obj->str[pos]), 1)};
    return true;
}

bool StringIter::next(Object& var)
{
    if (++pos == obj->str.size())
        return false;
    var = Object{CH_ALLOC(String, &(obj->str[pos]), 1)};
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