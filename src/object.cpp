/*
 * All constructors, methods, operators, etc. for the generic Object
 * class, as well as all child object types (including iterators).
 */

#include "../include/object.h"
#include "../include/bytecode.h"
#include "../include/bytes.h"
#include "../include/common.h"
#include "../include/debug.h"
#include "../include/diagnostic.h"
#include "../include/error.h"
#include "../include/linear_alloc.h"
#include "../include/natives.h"
#include <personal/hash_functions.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

using Natives::funcNames;

/* Object. */

#if !CH_USE_ALLOC

#define CLEAR_DATA(obj) memset(&((obj).as), 0, sizeof((obj).as));

void Object::clean()
{
    #if !CH_USE_ALLOC
        if (IS_HEAP_OBJ(*this))
        {
            HeapObj* temp{heapPointer()};
            CH_ASSERT(temp != nullptr, "NULL object pointer.");
            CH_ASSERT(temp->refCount > 0, "Refcount zero or negative.");

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

    if (IS_HEAP_OBJ(*this))
        heapPointer()->refCount++;
}

Object& Object::operator=(const Object& other) noexcept
{
    CH_ASSERT(!IS_ITER(other), "Copying an iterator is not allowed.");

    if (this != &other)
    {
        clean();

        this->type_ = other.type_;
        this->as = other.as;

        if (IS_HEAP_OBJ(*this))
            heapPointer()->refCount++;
    }

    return *this;
}

Object::Object(Object&& other) noexcept :
    type_{other.type_}, as{other.as}
{
    // To prevent deallocation when it is destroyed.
    other.type_ = static_cast<u8>(ObjType::Invalid);
    CLEAR_DATA(other);
}

Object& Object::operator=(Object&& other) noexcept
{
    if (this != &other)
    {
        clean();

        this->type_ = other.type_;
        this->as = other.as;

        other.type_ = static_cast<u8>(ObjType::Invalid);
        CLEAR_DATA(other);
    }

    return *this;
}

Object::~Object()
{
    clean();
}

#undef CLEAR_DATA

#endif

HeapObj* Object::heapPointer() const
{
    switch (type())
    {
        case ObjType::Module:   return static_cast<HeapObj*>(as.moduleVal);
        case ObjType::UserType: return static_cast<HeapObj*>(as.userTypeVal);
        case ObjType::Instance: return static_cast<HeapObj*>(as.instanceVal);
        case ObjType::UserFunc:
        case ObjType::Lambda:   return static_cast<HeapObj*>(as.userFuncVal);
        case ObjType::Closure:  return static_cast<HeapObj*>(as.closureVal);
        case ObjType::Method:   return static_cast<HeapObj*>(as.methodVal);
        case ObjType::Text:     return static_cast<HeapObj*>(as.textVal);
        case ObjType::String:   return static_cast<HeapObj*>(as.stringVal);
        case ObjType::Range:    return static_cast<HeapObj*>(as.rangeVal);
        case ObjType::List:     return static_cast<HeapObj*>(as.listVal);
        case ObjType::Table:    return static_cast<HeapObj*>(as.tableVal);
        case ObjType::Ref:      return static_cast<HeapObj*>(as.refVal);
        default: CH_UNREACHABLE();
    }
}

bool Object::operator==(const Object& other) const
{
    if (IS_NUM(*this) && IS_NUM(other))
        return double(AS_NUM(*this)) == double(AS_NUM(other));
    else if (IS_STRING_LIKE(*this) && IS_STRING_LIKE(other))
        return this->getObjectText() == other.getObjectText();
    if (this->type() != other.type()) return false;

    switch (this->type())
    {
        case ObjType::Bool:     return AS_BOOL(*this) == AS_BOOL(other);
        case ObjType::Null:     return true;
        case ObjType::Module:   return *(AS_MODULE(*this)) == *(AS_MODULE(other));
        case ObjType::CoreType: return AS_CORE_TYPE(*this) == AS_CORE_TYPE(other);
        case ObjType::UserType: return AS_USER_TYPE(*this) == AS_USER_TYPE(other);
        case ObjType::Instance: return *(AS_INSTANCE(*this)) == *(AS_INSTANCE(other));
        case ObjType::CoreFunc: return AS_CORE_FUNC(*this) == AS_CORE_FUNC(other);
        case ObjType::UserFunc:
        case ObjType::Lambda:   return AS_USER_FUNC(*this) == AS_USER_FUNC(other);
        case ObjType::Closure:  return AS_CLOSURE(*this) == AS_CLOSURE(other);
        case ObjType::Method:   return *(AS_METHOD(*this)) == *(AS_METHOD(*this));
        case ObjType::Range:    return *(AS_RANGE(*this)) == *(AS_RANGE(other));
        case ObjType::List:     return *(AS_LIST(*this)) == *(AS_LIST(other));
        case ObjType::Table:    return *(AS_TABLE(*this)) == *(AS_TABLE(other));
        case ObjType::Void:     return true;
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
    else if (IS_STRING_LIKE(*this) && IS_STRING_LIKE(other))
    {
        auto str1{this->getObjectText()};
        auto str2{other.getObjectText()};
        return (str1.compare(str2) > 0);
    }

    CH_ASSERT(false, "Invalid operand types passed to operator.");
    CH_UNREACHABLE(); // Remains in release builds.
}

bool Object::operator<(const Object& other) const
{
    if (IS_NUM(*this) && IS_NUM(other))
        return AS_NUM(*this) < AS_NUM(other);
    else if (IS_STRING_LIKE(*this) && IS_STRING_LIKE(other))
    {
        auto str1{this->getObjectText()};
        auto str2{other.getObjectText()};
        return (str1.compare(str2) < 0);
    }

    CH_ASSERT(false, "Invalid operand types passed to operator.");
    CH_UNREACHABLE();
}

bool Object::in(const Object& other) const
{
    const Object& obj{*this};

    if (IS_STRING_LIKE(obj) && IS_STRING_LIKE(other))
    {
        auto str1{other.getObjectText()};
        auto str2{obj.getObjectText()};
        return (str1.find(str2) != std::string_view::npos);
    }
    else if (IS_INT(obj) && IS_RANGE(other))
        return AS_RANGE(other)->contains(AS_INT(obj));
    else if (IS_LIST(other))
        return AS_LIST(other)->contains(obj);
    else if (IS_TABLE(other))
        return AS_TABLE(other)->contains(obj);

    else if (!IS_STRING_LIKE(other) && !IS_RANGE(other))
        throw reportCollection(OBJ_NOT_ITERABLE, other);
    else
        throw reportCollection(OBJ_WRONG_ITER_TYPE, obj, other);
}

bool Object::isTruthy() const
{
    switch (type())
    {
        case ObjType::Int:      return (AS_INT(*this) != 0);
        case ObjType::Dec:      return (AS_DEC(*this) != 0.0);
        case ObjType::Bool:     return AS_BOOL(*this);
        case ObjType::Null:     return false;
        case ObjType::Text:     return (AS_TEXT(*this)->len != 0);
        case ObjType::String:   return (AS_STRING(*this)->str.size() != 0);
        case ObjType::List:     return (AS_LIST(*this)->array.count() != 0);
        case ObjType::Table:    return (AS_TABLE(*this)->table.size() != 0);
        case ObjType::Void:     return false;
        // Rest are always truthy.
        default:            return true;
    }
}

Object Object::getIndex(const Object& index) const
{
    CH_ASSERT(IS_COLLECTION(*this), "Incorrect object type for index operator.");

    switch (this->type())
    {
        case ObjType::Text:     return AS_TEXT(*this)->getIndex(index);
        case ObjType::String:   return AS_STRING(*this)->getIndex(index);
        case ObjType::Range:    return AS_RANGE(*this)->getIndex(index);
        case ObjType::List:     return AS_LIST(*this)->getIndex(index);
        case ObjType::Table:    return AS_TABLE(*this)->getIndex(index);
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
        case ObjType::Text:     return AS_TEXT(*this)->setIndex(index, value);
        case ObjType::String:   return AS_STRING(*this)->setIndex(index, value);
        case ObjType::Range:    return AS_RANGE(*this)->setIndex(index, value);
        case ObjType::List:     return AS_LIST(*this)->setIndex(index, value);
        case ObjType::Table:    return AS_TABLE(*this)->setIndex(index, value);
        default: CH_UNREACHABLE();
    }
}

std::string_view Object::getObjectText() const
{
    CH_ASSERT(IS_STRING_LIKE(*this), "Text retrieved from non-string.");

    if (IS_TEXT(*this)) return *(AS_TEXT(*this));
    return AS_STRING(*this)->str;
}

Cell* Object::indexRef(const Object& index)
{
    CH_ASSERT(IS_COLLECTION(*this), "Incorrect object type for index reference.");

    if (!IS_TABLE(*this))
    {
        if (!IS_INT(index))
            throw reportCollection(OBJ_NOT_INDEX, *this, index);

        // For now.
        if (AS_INT(index) < 0)
            throw RuntimeError(INDEX_OUT_OF_BOUNDS, "index cannot be negative");

        u64 size{collectionSize()};
        if (AS_INT(index) >= static_cast<i64>(size))
        {
            throw RuntimeError(INDEX_OUT_OF_BOUNDS,
                CH_STR(
                    "index is {}, while referenced value has size {}",
                    AS_INT(index), size
                )
            );
        }
    }
    else
    {
        if (!AS_TABLE(*this)->contains(index))
        {
            throw RuntimeError(TABLE_KEY_NOT_FOUND,
                CH_STR("table does not contain key: {}", getElementText(index)));
        }
    }

    Cell* cell{CH_ALLOC(Cell, this, index)};
    // Since we replace it immediately in the VM.
    cell->close();
    return cell;
}

Object& Object::deref()
{
    CH_ASSERT(IS_REF(*this), "deref() called on non-reference object.");
    return *(AS_REF(*this)->location);
}

u64 Object::collectionSize() const
{
    CH_ASSERT(IS_COLLECTION(*this),
        "collectionSize() called on non-collection object.");

    switch (type())
    {
        case ObjType::Text:     return AS_TEXT(*this)->len;
        case ObjType::String:   return AS_STRING(*this)->str.size();
        case ObjType::Range:    return AS_RANGE(*this)->length();
        case ObjType::List:     return AS_LIST(*this)->array.count();
        case ObjType::Table:    return AS_TABLE(*this)->table.size();
        default: CH_UNREACHABLE();
    }
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
        case ObjType::Int:      return hashKey(AS_INT(*this));
        case ObjType::Dec:      return hashKey(AS_DEC(*this));
        case ObjType::Bool:     return hashKey(AS_BOOL(*this));
        case ObjType::Null:     return 0;
        case ObjType::Module:   return hashKey(AS_MODULE(*this)->name);
        case ObjType::CoreType: return hashKey(static_cast<u8>(AS_CORE_TYPE(*this)));
        case ObjType::UserType: return hashPointer(AS_USER_TYPE(*this));
        case ObjType::Instance: return AS_INSTANCE(*this)->hash();
        case ObjType::CoreFunc: return hashKey(static_cast<u8>(AS_CORE_FUNC(*this)));
        case ObjType::UserFunc:
        case ObjType::Lambda:   return hashPointer(AS_USER_FUNC(*this));
        case ObjType::Closure:  return hashPointer(AS_CLOSURE(*this));
        case ObjType::Method:   return AS_METHOD(*this)->hash();
        case ObjType::Text:     return hashKey(AS_TEXT(*this)->str, AS_TEXT(*this)->len);
        case ObjType::String:   return hashKey(AS_STRING(*this)->str);
        case ObjType::Range:
        {
            const Range* range{AS_RANGE(*this)};
            return hashKey(range->start) + hashKey(range->stop)
                + hashKey(range->step);
        }
        case ObjType::List:     return AS_LIST(*this)->hash();
        // For now, at least.
        case ObjType::Table:    return hashPointer(AS_TABLE(*this));
        case ObjType::Ref:      return AS_REF(*this)->location->hash();
        case ObjType::Void:     return 0;
        default: CH_UNREACHABLE();
    }
}

static std::unordered_map<const HeapObj*, u64> printedCollections{};

#define PRINTING_ENTER(obj)                                                                 \
    do {                                                                                    \
        if (IS_COLLECTION(*(obj)))                                                          \
        {                                                                                   \
            nested = (                                                                      \
                printedCollections.find((obj)->heapPointer()) != printedCollections.end()   \
            );                                                                              \
            printedCollections[(obj)->heapPointer()]++;                                     \
        }                                                                                   \
    } while (false)

#define PRINTING_EXIT(obj) \
    do {                                                            \
        if (IS_COLLECTION(*(obj)))                                  \
        {                                                           \
            if ((--printedCollections[(obj)->heapPointer()]) == 0)  \
                printedCollections.erase((obj)->heapPointer());     \
        }                                                           \
    } while (false)

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

// Need to support internal types in this function as well
// since this is used for register printing in debug builds.
std::string Object::printVal() const
{
    std::string ret{};
    bool nested{false};
    PRINTING_ENTER(this);

    switch (type())
    {
        case ObjType::Int:      ret = std::to_string(AS_INT(*this));                            break;
        case ObjType::Dec:      ret = doubleToStr(AS_DEC(*this));                               break;
        case ObjType::Bool:     ret = (AS_BOOL(*this) ? "true" : "false");                      break;
        case ObjType::Null:     ret = "null";                                                   break;
        case ObjType::Module:   ret = CH_STR("<module {}>", AS_MODULE(*this)->name);            break;
        case ObjType::CoreType: ret = objTypes[static_cast<u8>(AS_CORE_TYPE(*this))];           break;
        case ObjType::UserType: ret = CH_STR("<type {}>", AS_USER_TYPE(*this)->name);           break;
        case ObjType::Instance: ret = AS_INSTANCE(*this)->printVal();                           break;
        case ObjType::CoreFunc: ret = CH_STR("<builtin {}>", funcNames[AS_CORE_FUNC(*this)]);   break;
        case ObjType::UserFunc: ret = CH_STR("<func {}>", AS_USER_FUNC(*this)->name);           break;
        case ObjType::Lambda:   ret = "<lambda>";                                               break;
        case ObjType::Closure:
        {
            Closure* closure{AS_CLOSURE(*this)};
            if (closure->function->name == nullptr)
                ret = "lambda";
            else
                ret = CH_STR("<func {}>", closure->function->name);
            break;
        }
        case ObjType::Method:
        {
            Method* method{AS_METHOD(*this)};
            Function* func{};
            if (IS_USER_FUNC(method->funcObj))
                func = AS_USER_FUNC(method->funcObj);
            else if (IS_CLOSURE(method->funcObj))
                func = AS_CLOSURE(method->funcObj)->function;
            ret = CH_STR("<method {}>", func->name);
            break;
        }
        // Pass nesting status to possibly nesting collection types.
        // Only lists and tables actually need this.
        case ObjType::Text:     ret = AS_TEXT(*this)->printVal(nested);                         break;
        case ObjType::String:   ret = AS_STRING(*this)->printVal(nested);                       break;
        case ObjType::Range:    ret = AS_RANGE(*this)->printVal(nested);                        break;
        case ObjType::List:     ret = AS_LIST(*this)->printVal(nested);                         break;
        case ObjType::Table:    ret = AS_TABLE(*this)->printVal(nested);                        break;
        case ObjType::Ref:      ret = CH_STR("*({})", AS_REF(*this)->location->printVal());     break;
        case ObjType::Void:     ret = "()";                                                     break;
        case ObjType::Iter:
        {
            const auto& iter{AS_ITER(*this)->iter};
            std::visit([&ret, nested](auto&& iter) {
                ret = "->" + iter.obj->printVal(nested);
            }, iter);
            break;
        }
        default: CH_UNREACHABLE();
    }

    PRINTING_EXIT(this);
    return ret;
}

std::string_view Object::printType() const
{
    if (IS_INSTANCE(*this))
        return AS_INSTANCE(*this)->type->name;
    return objTypes[static_cast<u8>(type())];
}

void Object::emit(std::ofstream& os) const
{
    os.put(static_cast<char>(type()));

    switch (type())
    {
        case ObjType::Int:      Bytes::encodeValue(os, AS_INT(*this));  break;
        case ObjType::Dec:      Bytes::encodeValue(os, AS_DEC(*this));  break;
        case ObjType::Module:   AS_MODULE(*this)->emit(os);             break;
        case ObjType::UserType: AS_USER_TYPE(*this)->emit(os);          break;
        case ObjType::UserFunc:
        case ObjType::Lambda:   AS_USER_FUNC(*this)->emit(os);          break;
        case ObjType::Text:     AS_TEXT(*this)->emit(os);               break;
        case ObjType::String:   AS_STRING(*this)->emit(os);             break;
        default: CH_UNREACHABLE();
    }
}

void Object::emitMetadata(std::ofstream& os) const
{
    switch (type())
    {
        case ObjType::UserType: AS_USER_TYPE(*this)->emitMetadata(os);  break;
        case ObjType::UserFunc:
        case ObjType::Lambda:   AS_USER_FUNC(*this)->emitMetadata(os);  break;
        default: break;
    }
}

ObjIter* Object::makeIter()
{
    if (!IS_ITERABLE(*this)) return nullptr;
    return CH_ALLOC(ObjIter, *this);
}

/* Object structs. */

// strdup is not a standard C++ function, but is instead from POSIX.
[[nodiscard]] static char* choiceStrdup(const char* str)
{
    auto size{strlen(str) + 1};
    char* ret{new char[size]};
    memcpy(ret, str, size); // Includes null byte.
    return ret;
}

[[nodiscard]] static u64 chunkDataSize(const ByteCode& chunk)
{
    // Code size and pool size values, as well as the
    // actual sizes of the code and pool.
    return (2 * sizeof(u64) + chunk.codeSize() + chunk.countPool());
}

[[nodiscard]] static u64 chunkMetadataSize(const ByteCode& chunk)
{
    // Metadata + metadata size value (8 bytes).
    return (chunk.metadataSize() * sizeof(DebugRange) + sizeof(u64));
}

Module::Module(const std::string& name) noexcept:
    name{choiceStrdup(name.c_str())} {}

Module::~Module() noexcept
{
    delete[] name;
}

bool Module::operator==(const Module& other)
{
    return (strcmp(this->name, other.name) == 0);
}

Object Module::getEntry(const std::string& name) const
{
    const Object* obj{entries.get(name)};
    if (obj == nullptr)
    {
        throw RuntimeError(ENTRY_NOT_DEFINED,
            CH_STR("module '{}' has no entry '{}'", this->name, name)
        );
    }
    return *obj;
}

void Module::emit(std::ofstream& os) const
{
    u64 len{strlen(name)};
    os.put(static_cast<char>(len));
    os.write(name, static_cast<std::streamsize>(len));
}

u64 Module::byteSize() const
{
    // Added type byte (1) and name length byte (1).
    return sizeof(u8) * 2 + strlen(name);
}

Type::Type(
    const std::string& name,
    std::vector<FieldPair>& fields,
    const ByteCode* inits
) noexcept:
    name{choiceStrdup(name.c_str())}, fieldCode{inits}
{
    u8 count{0};
    for (const auto& field : fields)
    {
        this->fields.emplace_back(field.first, field.second);
        this->fieldTable.add(field.first, count++);
    }
}

Type::~Type() noexcept
{
    delete[] name;
    delete[] fieldCode;
}

void Type::addMethod(const Object& method)
{
    const char* name{};
    if (IS_USER_FUNC(method))
        name = AS_USER_FUNC(method)->name;
    else if (IS_CLOSURE(method))
        name = AS_CLOSURE(method)->function->name;
    methods.add(name, Object{CH_ALLOC(Method, method)});
}

bool Type::defines(const std::string& method) const
{
    return methods.contains(method);
}

void Type::emit(std::ofstream& os) const
{
    auto emitName = [&os](const std::string& name) {
        u64 len{name.size()};
        os.put(static_cast<char>(len));
        os.write(name.data(), static_cast<std::streamsize>(len));
    };

    emitName(name);
    u8 fieldCount{static_cast<u8>(fields.size())};
    os.put(static_cast<char>(fieldCount));
    for (const auto& field : fields)
    {
        emitName(field.first);
        os.put(static_cast<char>(field.second));
    }

    for (u8 i{0}; i < fieldCount; i++)
    {
        fieldCode[i].encodeData(os);
        if (debugInfoState == DebugInfoState::Combined)
            fieldCode[i].encodeMetadata(os);
    }
}

void Type::emitMetadata(std::ofstream& os) const
{
    u8 fieldCount{static_cast<u8>(fields.size())};
    for (u8 i{0}; i < fieldCount; i++)
        fieldCode[i].encodeMetadata(os);
}

u64 Type::byteSize() const
{
    // Added type byte (1).
    u64 size{sizeof(u8)};
    auto countName = [&size](const std::string& name) {
        // Added name length byte (1).
        size += sizeof(u8) + name.size();
    };

    countName(name);
    u8 fieldCount{static_cast<u8>(fields.size())};
    // Added field count byte (1).
    size += sizeof(fieldCount);
    for (const auto& field : fields)
    {
        countName(field.first);
        // Added Boolean mutability byte (1).
        size += sizeof(field.second);
    }

    for (u8 i{0}; i < fieldCount; i++)
    {
        size += chunkDataSize(fieldCode[i]);
        if (debugInfoState == DebugInfoState::Combined)
            size += chunkMetadataSize(fieldCode[i]);
    }

    return size;
}

Instance::Instance(const Type* type) noexcept:
    type{type}
{
    for (const auto& field : type->fields)
        this->fields.add(field.first, Object{});
}

bool Instance::operator==(const Instance& other) const
{
    // For now.
    return (this->type == other.type);
}

Object* Instance::findField(const std::string& name)
{
    Object* location{fields.get(name)};
    // Since this function is only used for field writes,
    // we don't search methods.
    if (location == nullptr)
    {
        throw RuntimeError(FIELD_NOT_DEFINED,
            CH_STR("type '{}' has no field '{}'", type->name, name)
        );
    }

   return location;
}

const Object* Instance::findField(const std::string& name) const
{
    const Object* location{fields.get(name)};
    if (location == nullptr)
    {
        location = type->methods.get(name);
        if (location != nullptr)
            AS_METHOD(*location)->boundInstance = this;
        else
        {
            throw RuntimeError(FIELD_NOT_DEFINED,
                CH_STR("type '{}' has no field or method '{}'", type->name, name)
            );
        }
    }

   return location;
}

Object Instance::getField(const std::string& name) const
{
    const Object* location{findField(name)};
    if (!IS_VALID(*location))
        throw RuntimeError(FIELD_UNINIT_READ);
    return *location;
}

void Instance::setField(const std::string& name, const Object& value)
{
    Object* location{findField(name)};
    // Field is fixed, but the flag is placed on the value itself.
    if (IS_FIXED(*location))
        throw RuntimeError(MOD_FIXED_FIELD);
    *location = value;
}

void Instance::initField(const std::string& name, const Object& value)
{
    Object* location{findField(name)};
    *location = value;

    // Field must exist if we have reached this point.
    u8 position{*(type->fieldTable.get(name))};
    bool fix{type->fields[position].second};

    if (fix)
    {
        MAKE_FIXED(*location);
        if (!IS_MUT(*location)) MAKE_IMMUT(*location);
    }
}

Hash Instance::hash() const
{
    Hash hash{hashPointer(type)};
    for (const auto& [_, val] : fields)
        hash += val.hash();
    return hash;
}

std::string Instance::printVal() const
{
    if (fields.empty())
        return std::string{type->name} + "{}";

    std::string ret{type->name};
    ret += " {\n";
    for (const auto& field : type->fields)
    {
        const Object& value{*(fields.get(field.first))};
        ret += "  " + field.first + ": " + value.printVal() + ",\n";
    }

    ret.pop_back(); ret.pop_back();
    ret += "\n}";
    return ret;
}

Function::Function(const ByteCode& code, u8 arityMin, u8 arityMax) noexcept:
    code{code}, arityMin{arityMin}, arityMax{arityMax} {}

Function::Function(
    const std::string& name,
    const ByteCode& code,
    u8 arityMin,
    u8 arityMax
) noexcept:
    name{choiceStrdup(name.c_str())}, code{code}, arityMin{arityMin},
    arityMax{arityMax} {}

Function::~Function() noexcept
{
    delete[] name;
    delete[] defaultArgs;
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

    os.put(static_cast<char>(arityMin));
    os.put(static_cast<char>(arityMax));
    os.put(static_cast<char>(variadic));

    code.encodeData(os);
    if (debugInfoState == DebugInfoState::Combined)
        code.encodeMetadata(os);

    // Can be computed from cached bytes with arityMax - arityMin.
    u8 defaultCount{static_cast<u8>(arityMax - arityMin)};
    for (u8 i{0}; i < defaultCount; i++)
    {
        defaultArgs[i].encodeData(os);
        if (debugInfoState == DebugInfoState::Combined)
            defaultArgs[i].encodeMetadata(os);
    }
}

void Function::emitMetadata(std::ofstream& os) const
{
    code.encodeMetadata(os);
    u8 defaultCount{static_cast<u8>(arityMax - arityMin)};
    for (u8 i{0}; i < defaultCount; i++)
        defaultArgs[i].encodeMetadata(os);
}

u64 Function::byteSize() const
{
    u64 size{0};
    if (name != nullptr) size += strlen(name);

    // Added type byte (1) and name length byte (1)
    // and arity bytes (3).
    size += 5 * sizeof(u8);

    size += chunkDataSize(code);
    // Same as above for default arguments.
    u8 defaultCount{static_cast<u8>(arityMax - arityMin)};
    for (u8 i{0}; i < defaultCount; i++)
        size += chunkDataSize(defaultArgs[i]);

    if (debugInfoState == DebugInfoState::Combined)
    {
        size += chunkMetadataSize(code);
        // Same as above for default arguments.
        for (u8 i{0}; i < defaultCount; i++)
            size += chunkMetadataSize(defaultArgs[i]);
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

void Closure::addCell(Cell* cell)
{
    #if !CH_USE_ALLOC
        cell->refCount++;
    #endif
    cells.push(cell);
}

Method::Method(const Object& funcObj) noexcept:
    funcObj{funcObj} {}

bool Method::operator==(const Method& other) const
{
    // Should be the same method and bound to the same
    // instance.
    return ((this->funcObj == other.funcObj)
            && (this->boundInstance == other.boundInstance));
}

Hash Method::hash() const
{
    return funcObj.hash() + boundInstance->hash();
}

Text::Text(const std::string_view& view) noexcept:
    str{choiceStrdup(view.data())}, len{view.length()} {}

Text::Text(const char* str, size_t len) noexcept :
    str{choiceStrdup(str)}, len{len} {}

Text::~Text()
{
    delete[] str;
}

Object Text::getIndex(const Object& index) const
{
    if (!IS_INT(index))
        throw reportCollection(OBJ_NOT_INDEX, this, index);

    // For now.
    if (AS_INT(index) < 0)
        throw RuntimeError(INDEX_OUT_OF_BOUNDS, "index cannot be negative");
    if (AS_UINT(index) >= len)
    {
        throw RuntimeError(INDEX_OUT_OF_BOUNDS,
            CH_STR(
                "index is {}, while string has length {}", AS_INT(index), len
            )
        );
    }

    return CH_ALLOC(Text, str + AS_INT(index), 1);
}

void Text::setIndex(const Object& index, const Object& value)
{
    (void) index; (void) value;
    throw RuntimeError(OBJ_NO_ELEM_ASSIGN);
}

void Text::reset(const std::string_view& view)
{
    delete[] str;
    str = choiceStrdup(view.data());
    len = view.length();
}

Text::operator std::string_view()
{
    return std::string_view{str, len};
}

std::string Text::printVal(bool nested) const
{
    (void) nested;
    return std::string{str, len};
}

void Text::emit(std::ofstream& os) const
{
    Bytes::encodeValue(os, len);
    os.write(str, len);
}

u64 Text::byteSize() const
{
    // Added type byte (1) and string length bytes (8).
    return sizeof(u8) + sizeof(u64) + len;
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

Object String::getIndex(const Object& index) const
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

    if (!IS_STRING_LIKE(value))
    {
        throw RuntimeError(WRONG_ELEM_TYPE,
            CH_STR("cannot store ({}) in a string", value.printType())
        );
    }

    auto insert{value.getObjectText()};
    if (insert.size() > 1)
    {
        throw RuntimeError(WRONG_ELEM_TYPE,
            "cannot store more than one character at a single index");
    }

    str[0] = insert[0];
}

std::string String::printVal(bool nested) const
{
    (void) nested;
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

u64 Range::length() const
{
    if ((step == 0) ||(step == 1)) return std::abs(stop - start) + 1;
    return std::abs((stop - start) / step) + 1;
}

Object Range::getIndex(const Object& index) const
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

std::string Range::printVal(bool nested) const
{
    (void) nested;

    auto str{CH_STR("{}..{}", start, stop)};
    if (step != 1)
        str += CH_STR("..{}", step);
    return str;
}

List::List(u64 size) noexcept:
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

Object List::getIndex(const Object& index) const
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
    return (array.find(obj) != -1);
}

Hash List::hash() const
{
    Hash hash{0};

    for (const Object& obj : array)
        hash += obj.hash();

    return hash;
}

std::string List::printVal(bool nested) const
{
    if (nested) return "[...]";

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

Object Table::getIndex(const Object& key) const
{
    const Object* value{table.get(key)};
    if (value != nullptr) return *value;

    throw RuntimeError(TABLE_KEY_NOT_FOUND,
        CH_STR("table does not contain key: {}", getElementText(key)));
}

void Table::setIndex(const Object& key, const Object& value)
{
    table[key] = value;
}

std::string Table::printVal(bool nested) const
{
    if (nested) return "{...}";

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

Cell::Cell(Object* location) noexcept:
    location{location} {}

Cell::Cell(Object* obj, const Object& index) noexcept:
    isElement{true}, location{obj}
{
    CH_ASSERT(IS_INT(index), "Non-integer object used as index.");
    this->index = AS_INT(index);
}

void Cell::close()
{
    obj = *location;
    location = &obj;
}

void Cell::assign(const Object& value)
{
    if (isElement)
        location->setIndex(Object{index}, value);
    else
    {
        if (IS_FIXED(*location))
        {
            throw RuntimeError(MOD_FIXED_VARIABLE,
                CH_STR(
                    "immutable ({}) being implicitly modified "
                    "through a reference here", location->printType()
                )
            );
        }

        *location = value;
    }
}


/* Object iterator struct types. */

TextIter::TextIter(Object& obj) noexcept:
    obj{AS_TEXT(obj)}, pos{0}
{
    #if !CH_USE_ALLOC
        this->obj->refCount++;
    #endif
}

TextIter::TextIter(TextIter&& other) noexcept :
    obj{other.obj}, pos{other.pos}
{
    other.obj = nullptr;
}

TextIter& TextIter::operator=(TextIter&& other) noexcept
{
    if (this != &other)
    {
        this->obj = other.obj;
        this->pos = other.pos;

        other.obj = nullptr;
    }

    return *this;
}

TextIter::~TextIter()
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

bool TextIter::start(Object& var)
{
    if (obj->len == 0) return false;

    var = Object{CH_ALLOC(Text, obj->str + pos, 1)};
    return true;
}

bool TextIter::next(Object& var)
{
    if (++pos == obj->len) return false;

    var = Object{CH_ALLOC(Text, obj->str + pos, 1)};
    return true;
}

StringIter::StringIter(Object& obj) noexcept:
    obj{AS_STRING(obj)}, pos{0}, flags{getMutFlags(obj)}
{
    #if !CH_USE_ALLOC
        this->obj->refCount++;
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
        this->obj->refCount++;
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
        this->obj->refCount++;
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

TableIter::TableIter(Object& obj) noexcept :
    obj{AS_TABLE(obj)}, flags{getMutFlags(obj)}
{
    #if !CH_USE_ALLOC
        this->obj->refCount++;
    #endif
}

TableIter::TableIter(TableIter&& other) noexcept :
    obj{other.obj}, it{other.it}
{
    other.obj = nullptr;
}

TableIter& TableIter::operator=(TableIter&& other) noexcept
{
    if (this != &other)
    {
        this->obj = other.obj;
        this->it = other.it;

        other.obj = nullptr;
    }

    return *this;
}

TableIter::~TableIter()
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

bool TableIter::start(Object& var)
{
    if (obj->table.size() == 0)
        return false;

    it = obj->table.begin();

    List* list{CH_ALLOC(List, 2)};
    list->array.push(*(it->first));
    // So users cannot modify keys directly.
    MAKE_IMMUT(list->array[0]);
    list->array.push(*(it->second));

    var = Object{list};
    setMutFlags(var, flags);

    return true;
}

bool TableIter::next(Object& var)
{
    if (++it == obj->table.end())
        return false;

    List* list{CH_ALLOC(List, 2)};
    list->array.push(*(it->first));
    MAKE_IMMUT(list->array[0]);
    list->array.push(*(it->second));

    var = Object{list};
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
        case ObjType::Text:     iter.emplace<TextIter>(obj);    break;
        case ObjType::String:   iter.emplace<StringIter>(obj);  break;
        case ObjType::Range:    iter.emplace<RangeIter>(obj);   break;
        case ObjType::List:     iter.emplace<ListIter>(obj);    break;
        case ObjType::Table:    iter.emplace<TableIter>(obj);   break;
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