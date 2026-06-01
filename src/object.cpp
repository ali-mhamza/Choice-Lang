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
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <variant>
using Natives::funcNames;

constexpr std::array<std::string_view, NUM_TYPES> objTypes{
    "Int", "Dec", "Boolean", "Null", "Type", "Builtin",
    "Function", "Function", "Lambda", "BigInt",
    "BigDec", "String", "Range", "List", "Table",
    "", // References take the type of the contained object.
    "Void", "Iterable"
};

/* Object. */

Object::Object() noexcept :
    type_{OBJ_INVALID}
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
    type_{other.type_}, as{other.as}
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

        this->type_ = other.type_;
        this->as = other.as;

        #if !CH_USE_ALLOC
            if (IS_HEAP_OBJ(*this))
                AS_HEAP_PTR(*this)->refCount++;
        #endif
    }

    return *this;
}

Object::Object(Object&& other) noexcept :
    type_{other.type_}, as{other.as}
{
    other.type_ = OBJ_INVALID; // To prevent deallocation when it is destroyed.
    AS_INT(other) = 0;
}

Object& Object::operator=(Object&& other) noexcept
{
    if (this != &other)
    {
        clean();

        this->type_ = other.type_;
        this->as = other.as;

        other.type_ = OBJ_INVALID;
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
    if (this->type() != other.type()) return false;

    switch (this->type())
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

bool Object::operator!=(const Object& other) const
{
    return !(*this == other);
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

    else if (!IS_STRING(other) && !IS_RANGE(other))
        throw reportCollection(OBJ_NOT_ITERABLE, other);
    else
        throw reportCollection(OBJ_WRONG_ITER_TYPE, obj, other);
}

Object Object::getIndex(const Object& index)
{
    CH_ASSERT(IS_COLLECTION(*this), "Incorrect object type for index operator.");

    switch (this->type())
    {
        case OBJ_STRING:    return AS_STRING(*this)->getIndex(index);
        case OBJ_RANGE:     return AS_RANGE(*this)->getIndex(index);
        case OBJ_LIST:      return AS_LIST(*this)->getIndex(index);
        case OBJ_TABLE:     return AS_TABLE(*this)->getIndex(index);
        default: CH_UNREACHABLE();
    }
}

void Object::setIndex(const Object& index, const Object& value)
{
    CH_ASSERT(IS_COLLECTION(*this), "Incorrect object type for index operator.");

    if (IS_IMMUT(*this))
    {
        throw RuntimeError(MOD_IMMUT_VALUE,
            CH_STR("immutable ({}) being modified here", this->printType())
        );
    }

    switch (this->type())
    {
        case OBJ_STRING:    return AS_STRING(*this)->setIndex(index, value);
        case OBJ_RANGE:     return AS_RANGE(*this)->setIndex(index, value);
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
    const u8* temp{reinterpret_cast<const u8*>(&ptr)};
    return hashBytes(temp, sizeof(T*));
}

Hash Object::hash() const
{
    switch (type())
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
        case OBJ_LIST:      return AS_LIST(*this)->hash();
        // For now, at least.
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
    switch (type())
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
    return objTypes[type()];
}

void Object::emit(std::ofstream& os) const
{
    os.put(static_cast<char>(type()));

    switch (type())
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

/* Object structs. */

Cell::Cell(Object* location) noexcept:
    location{location} {}

void Cell::close()
{
    obj = *location;
    location = &obj;
}

Function::Function(const ByteCode& code, const u8 argCount) noexcept:
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
) noexcept:
    name{choiceStrdup(name.c_str())}, code{code}, argCount{argCount},
    lambda{false} {}

Function::~Function() noexcept
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

Closure::Closure(Function* function) noexcept:
    function{function}
{
    #if !CH_USE_ALLOC
        function->refCount++;
    #endif
}

Closure::~Closure() noexcept
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

String::String(const std::string& str) noexcept:
    str{str} {}

String::String(const std::string_view& view) noexcept:
    str{view} {}

String::String(const char* str, size_t len) noexcept
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

    // For now.
    if (AS_INT(index) < 0)
        throw RuntimeError(INDEX_OUT_OF_BOUNDS, "index cannot be negative");
    if (AS_UINT(index) >= str.size())
    {
        throw RuntimeError(INDEX_OUT_OF_BOUNDS,
            CH_STR(
                "index is {}, while string has length {}", AS_INT(index), str.size()
            )
        );
    }

    return CH_ALLOC(String, str.data() + AS_INT(index), 1);
}

void String::setIndex(const Object& index, const Object& value)
{
    if (!IS_INT(index))
        throw reportCollection(OBJ_NOT_INDEX, this, index);

    // For now.
    if (AS_INT(index) < 0)
        throw RuntimeError(INDEX_OUT_OF_BOUNDS, "index cannot be negative");
    if (AS_UINT(index) >= str.size())
    {
        throw RuntimeError(INDEX_OUT_OF_BOUNDS,
            CH_STR(
                "index is {}, while string has length {}", AS_INT(index), str.size()
            )
        );
    }

    if (!IS_STRING(value))
    {
        throw RuntimeError(WRONG_ELEM_TYPE,
            CH_STR("cannot store ({}) in a string", value.printType())
        );
    }

    const auto& insert{AS_STRING(value)->str};
    if (insert.size() > 1)
    {
        throw RuntimeError(WRONG_ELEM_TYPE,
            "cannot store more than one character at a single index");
    }

    str[0] = insert[0];
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

Range::Range(const std::array<i64, 3>& nums) noexcept:
    start{nums[0]}, stop{nums[1]}, step{nums[2]} {}

void Range::validateRange(const std::array<i64, 3>& nums)
{
    // Checks:

    // 1. Step size cannot be zero (unless start == stop).
    // Checked by range!() function.

    // 2. Step cannot be negative if stop > start, and
    // cannot be positive if stop < start.

    const i64 start{nums[0]}, stop{nums[1]}, step{nums[2]};

    if (((stop > start) && (step < 0)) || ((stop < start) && (step > 0)))
        throw RuntimeError(INVALID_RANGE_STEP, "range has infinite size");
}

bool Range::operator==(const Range& other) const
{
    return ((this->start == other.start)
            && (this->stop == other.stop)
            && (this->step == other.step));
}

bool Range::contains(const i64 num) const
{
    if (step == 0) return (num == start); // Equivalent to num == stop.

    if ((step > 0) && ((num < start) || (num > stop))) return false;
    if ((step < 0) && ((num > start) || (num < stop))) return false;

    return ((num - start) % step == 0);
}

i64 Range::length() const
{
    if ((step == 0) ||(step == 1)) return std::abs(stop - start) + 1;
    return std::abs((stop - start) / step) + 1;
}

Object Range::getIndex(const Object& index)
{
    if (!IS_INT(index))
        throw reportCollection(OBJ_NOT_INDEX, this, index);

    i64 indexValue{AS_INT(index)};
    bool reverse{start > stop};

    // For now.
    if (indexValue < 0)
        throw RuntimeError(INDEX_OUT_OF_BOUNDS, "index cannot be negative");
    if ((!reverse && (start + indexValue * step > stop))
        || (reverse && (start + indexValue * step < stop)))
    {
        throw RuntimeError(INDEX_OUT_OF_BOUNDS, "index value too large");
    }

    return (start + indexValue * step);
}

void Range::setIndex(const Object& index, const Object& value)
{
    (void) index; (void) value;
    throw RuntimeError(OBJ_NO_ELEM_ASSIGN);
}

std::string Range::printVal() const
{
    auto str{CH_STR("{}..{}", start, stop)};
    if (step != 1)
        str += CH_STR("..{}", step);
    return str;
}

List::List(u32 size) noexcept:
    array{size} {}

bool List::operator==(const List& other) const
{
    if (this->array.count() != other.array.count()) return false;

    const size_t size{this->array.count()};
    for (size_t i{0}; i < size; i++)
    {
        if (this->array[i] != other.array[i])
            return false;
    }

    return true;
}

Object List::getIndex(const Object& index)
{
    if (!IS_INT(index))
        throw reportCollection(OBJ_NOT_INDEX, this, index);

    // For now.
    if (AS_INT(index) < 0)
        throw RuntimeError(INDEX_OUT_OF_BOUNDS, "index cannot be negative");
    if (AS_UINT(index) >= array.count())
    {
        throw RuntimeError(INDEX_OUT_OF_BOUNDS,
            CH_STR(
                "index is {}, while list has length {}", AS_INT(index), array.count()
            )
        );
    }

    return array[AS_INT(index)];
}

void List::setIndex(const Object& index, const Object& value)
{
    if (!IS_INT(index))
        throw reportCollection(OBJ_NOT_INDEX, this, index);

    // For now.
    if (AS_INT(index) < 0)
        throw RuntimeError(INDEX_OUT_OF_BOUNDS, "index cannot be negative");
    if (AS_UINT(index) >= array.count())
    {
        throw RuntimeError(INDEX_OUT_OF_BOUNDS,
            CH_STR(
                "index is {}, while list has length {}", AS_INT(index), array.count()
            )
        );
    }

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

Hash List::hash() const
{
    Hash hash{0};

    for (const Object& obj : array)
        hash += obj.hash();

    return hash;
}

std::string List::printVal() const
{
    std::string ret{"["};
    size_t size{array.count()};
    for (size_t i{0}; i < size; i++)
    {
        ret += getElementText(array[i]);
        if (i != size - 1)
            ret += ", ";
    }

    ret += "]";
    return ret;
}

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
        ret += "(" + getElementText(key) + ", " + getElementText(value) + "), ";

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


/* Object iterator struct types. */

StringIter::StringIter(Object& obj) noexcept:
    obj{AS_STRING(obj)}, pos{0}, flags{getMutFlags(obj)}
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
    setMutFlags(var, flags);

    return true;
}

bool StringIter::next(Object& var)
{
    if (++pos == obj->str.size())
        return false;

    var = Object{CH_ALLOC(String, &(obj->str[pos]), 1)};
    setMutFlags(var, flags);

    return true;
}

RangeIter::RangeIter(Object& obj) noexcept:
    obj{AS_RANGE(obj)}
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
    // Step size of 0 can only allow a single iteration.
    if (obj->step == 0) return false;

    val += obj->step;
    if (((obj->step > 0) && (val > obj->stop))
        || ((obj->step < 0) && (val < obj->stop)))
    {
        return false;
    }

    AS_INT(var) = val;
    return true;
}

ListIter::ListIter(Object& obj) noexcept:
    obj{AS_LIST(obj)}, flags{getMutFlags(obj)}
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
    setMutFlags(var, flags);

    return true;
}

bool ListIter::next(Object& var)
{
    if (++it == obj->array.end())
        return false;

    var = *it;
    setMutFlags(var, flags);

    return true;
}

ObjIter::ObjIter(Object& obj) noexcept
{
    // Use emplace instead of assignment so we construct the
    // iterator in-place with no intermediate temporary object
    // (otherwise the temporary's destructor will mess with
    // the potential refcount).

    switch (obj.type())
    {
        case OBJ_STRING:    iter.emplace<StringIter>(obj);  break;
        case OBJ_RANGE:     iter.emplace<RangeIter>(obj);   break;
        case OBJ_LIST:      iter.emplace<ListIter>(obj);    break;
        default: CH_UNREACHABLE();
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