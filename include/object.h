#pragma once
#include "bytecode.h"
#include "common.h"
#include "modules.h"
#include "natives.h"
#include <personal/array.h>
#include <personal/hash_table.h>
#include <array>
#include <fstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

using Natives::FuncType;
class Object;
struct ObjectHasher;
using ObjectTable = HashTable<Object, Object, ObjectHasher>;

/* Constants. */

// Variable is fixed/not fixed (bit 7).
inline constexpr u8 FIXED_FLAG  = 0x80;
// Value has an active mutable/immutable flag (bit 6).
inline constexpr u8 INIT_FLAG   = 0x40;
// Value is mutable/immutable (bit 5).
inline constexpr u8 IMMUT_FLAG  = 0x20;
// Remaining bits (max. 32 values).
inline constexpr u8 TYPE_MASK   = 0x1f;

/* Type list macro. */

#pragma push_macro("NULL")
#pragma push_macro("AS_VOID")
#undef NULL
#undef AS_VOID

#define TYPE_LIST                       \
    X(INT, Int, intVal)                 \
    X(DEC, Dec, decVal)                 \
    X(BOOL, Bool, boolVal)              \
    X(NULL, Null, heapVal)              \
    X(VOID, Void, heapVal)              \
    X(CORE_TYPE, CoreType, coreTypeVal) \
    X(CORE_FUNC, CoreFunc, coreFuncVal) \
    X(MODULE, Module, moduleVal)        \
    X(USER_TYPE, UserType, userTypeVal) \
    X(INSTANCE, Instance, instanceVal)  \
    X(USER_FUNC, UserFunc, userFuncVal) \
    X(LAMBDA, Lambda, userFuncVal)      \
    X(CLOSURE, Closure, closureVal)     \
    X(METHOD, Method, methodVal)        \
    X(BIGINT, BigInt, heapVal)          \
    X(BIGDEC, BigDec, heapVal)          \
    X(TEXT, Text, textVal)              \
    X(STRING, String, stringVal)        \
    X(RANGE, Range, rangeVal)           \
    X(LIST, List, listVal)              \
    X(TABLE, Table, tableVal)           \
    X(REF, Ref, refVal)                 \
    /* Used in for-loops. */            \
    X(ITER, Iter, iterVal)              \

/* Type enum. */

#define X(_, name, field) name,

enum class ObjType : u8
{
    TYPE_LIST

    Count,
    Invalid,
};

#undef X


/* Forward declarations. */

struct Module;
struct Type;
struct Instance;
struct Function;
struct Closure;
struct Method;
struct Text;
struct String;
struct Range;
struct List;
struct Table;
struct Cell;
struct HeapObj;
struct ObjIter;


/* Main object class. */

class Object
{
    private:
        #if !CH_USE_ALLOC
            void clean();
        #endif

        template<typename T> decltype(auto) getTypePointer();
        // Marked as 'const', but heap-allocated object payload
        // may still be modified.
        [[nodiscard]] HeapObj* heapPointer() const;

    public:
        u8 type_{};
        union Value {
            i64             intVal;
            double          decVal;
            bool            boolVal;
            ObjType         coreTypeVal;
            FuncType        coreFuncVal;
            Module*         moduleVal;
            Type*           userTypeVal;
            Instance*       instanceVal;
            Function*       userFuncVal;
            Closure*        closureVal;
            Method*         methodVal;
            Text*           textVal;
            String*         stringVal;
            Range*          rangeVal;
            List*           listVal;
            Table*          tableVal;
            Cell*           refVal;
            HeapObj*        heapVal;
            ObjIter*        iterVal;
        } as;

        Object() noexcept : type_{static_cast<u8>(ObjType::Invalid)}, as{0} {}
        explicit Object(ObjType type) : type_{static_cast<u8>(type)}, as{0} {}
        template<typename T> Object(T val) noexcept;

        #if !CH_USE_ALLOC
            Object(const Object& other) noexcept;
            Object& operator=(const Object& other) noexcept;
            Object(Object&& other) noexcept;
            Object& operator=(Object&& other) noexcept;
            ~Object();
        #endif

        [[nodiscard]]
        ObjType type() const { return static_cast<ObjType>(type_ & TYPE_MASK); }

        [[nodiscard]] bool operator==(const Object& other) const;
        [[nodiscard]] bool operator!=(const Object& other) const;
        [[nodiscard]] bool operator>(const Object& other) const;
        [[nodiscard]] bool operator<(const Object& other) const;
        [[nodiscard]] bool in(const Object& other) const;
        [[nodiscard]] bool isTruthy() const;

        [[nodiscard]] Object getIndex(const Object& index) const;
        void setIndex(const Object& index, const Object& value);

        // Only to be used on Text or String objects.
        // Should not be used if the String object may be modified
        // while the view is in use.
        std::string_view getObjectText() const;

        [[nodiscard]] Cell* indexRef(const Object& index);
        // Only to be used on Cell objects.
        // Does not type-check.
        Object& deref();

        // Get the size of the collection object payload.
        // Only valid to call for collections.
        [[nodiscard]] u64 collectionSize() const;

        [[nodiscard]] Hash hash() const;
        [[nodiscard]] std::string printVal() const;
        [[nodiscard]] std::string_view printType() const;

        void emit(std::ofstream& os) const;
        // For objects with bytecode components.
        void emitMetadata(std::ofstream& os) const;

        [[nodiscard]] ObjIter* makeIter();
};

template<typename T>
ObjType getObjectType(T val)
{
    using U = std::remove_const_t<std::remove_pointer_t<T>>;

    if constexpr (std::is_same_v<U, Function>)
    {
        if (val->name == nullptr) return ObjType::Lambda;
        return ObjType::UserFunc;
    }

    if constexpr (std::is_same_v<U, Module>)    return ObjType::Module;
    if constexpr (std::is_same_v<U, Type>)      return ObjType::UserType;
    if constexpr (std::is_same_v<U, Instance>)  return ObjType::Instance;
    if constexpr (std::is_same_v<U, Closure>)   return ObjType::Closure;
    if constexpr (std::is_same_v<U, Method>)    return ObjType::Method;
    if constexpr (std::is_same_v<U, Text>)      return ObjType::Text;
    if constexpr (std::is_same_v<U, String>)    return ObjType::String;
    if constexpr (std::is_same_v<U, Range>)     return ObjType::Range;
    if constexpr (std::is_same_v<U, List>)      return ObjType::List;
    if constexpr (std::is_same_v<U, Table>)     return ObjType::Table;
    if constexpr (std::is_same_v<U, Cell>)      return ObjType::Ref;
}

template<typename T>
decltype(auto) Object::getTypePointer()
{
    using U = std::remove_const_t<std::remove_pointer_t<T>>;

    // Parentheses around return values to return by reference
    // instead of by value.

    if constexpr (std::is_same_v<U, Module>)    return (as.moduleVal);
    if constexpr (std::is_same_v<U, Type>)      return (as.userTypeVal);
    if constexpr (std::is_same_v<U, Instance>)  return (as.instanceVal);
    if constexpr (std::is_same_v<U, Function>)  return (as.userFuncVal);
    if constexpr (std::is_same_v<U, Closure>)   return (as.closureVal);
    if constexpr (std::is_same_v<U, Method>)    return (as.methodVal);
    if constexpr (std::is_same_v<U, Text>)      return (as.textVal);
    if constexpr (std::is_same_v<U, String>)    return (as.stringVal);
    if constexpr (std::is_same_v<U, Range>)     return (as.rangeVal);
    if constexpr (std::is_same_v<U, List>)      return (as.listVal);
    if constexpr (std::is_same_v<U, Table>)     return (as.tableVal);
    if constexpr (std::is_same_v<U, Cell>)      return (as.refVal);
}

template<typename T>
Object::Object(T val) noexcept
{
    if constexpr (std::is_same_v<T, i64>)
    {
        type_ = static_cast<u8>(ObjType::Int);
        as.intVal = val;
    }
    else if constexpr (std::is_same_v<T, double>)
    {
        type_ = static_cast<u8>(ObjType::Dec);
        as.decVal = val;
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
        type_ = static_cast<u8>(ObjType::Bool);
        as.boolVal = val;
    }
    else if constexpr (std::is_same_v<T, std::nullptr_t>)
    {
        type_ = static_cast<u8>(ObjType::Null);
        as.heapVal = val; // Dummy assignment.
    }
    else if constexpr (std::is_same_v<T, ObjType>)
    {
        type_ = static_cast<u8>(ObjType::CoreType);
        as.coreTypeVal = val;
    }
    else if constexpr (std::is_same_v<T, FuncType>)
    {
        type_ = static_cast<u8>(ObjType::CoreFunc);
        as.coreFuncVal = val;
    }
    else if constexpr (std::is_same_v<T, ObjIter*>)
    {
        type_ = static_cast<u8>(ObjType::Iter);
        // Iterators should never be copied, so we
        // don't use a refcount.
        as.iterVal = val;
    }
    else
    {
        type_ = static_cast<u8>(getObjectType(val));

        auto& ptr{getTypePointer<T>()};
        if constexpr (std::is_const_v<std::remove_pointer_t<T>>)
        {
            using U = std::remove_const_t<std::remove_pointer_t<T>>;
            ptr = const_cast<U*>(val);
        }
        else
            ptr = val;

        #if !CH_USE_ALLOC
            ptr->refCount++;
        #endif
    }
}


/* Object type names. */

inline constexpr
std::array<std::string_view, static_cast<u64>(ObjType::Count)> objTypes{
    "Int", "Dec", "Bool", "Null", "Void", "Builtin Type",
    "Builtin Function", "Module", "User Type", "Type Instance",
    "User Function", "Lambda", "User Function", "Type Method",
    "BigInt", "BigDec", "Text", "String", "Range", "List",
    "Table", "", // References take the type of the contained object.
    "Iterable"
};


/* Object helper functions and macros. */

#define X(TYPE, name, field) \
    [[nodiscard]] static inline bool IS_##TYPE(const Object& obj) { \
        return (obj.type() == ObjType::name);                       \
    }                                                               \
    [[nodiscard]] static inline auto AS_##TYPE(const Object& obj) { \
        return obj.as.field;                                        \
    }                                                               \
    [[nodiscard]] static inline auto& AS_##TYPE(Object& obj) {      \
        return obj.as.field;                                        \
    }

TYPE_LIST

#undef X

// Object is a function object.
#define IS_FUNCOBJ(obj) \
    (IS_USER_FUNC(obj) || IS_LAMBDA(obj) || IS_CLOSURE(obj) || IS_METHOD(obj))

// Object can be called.
#define IS_CALLABLE(obj) \
    (IS_CORE_FUNC(obj) || IS_FUNCOBJ(obj) || IS_CORE_TYPE(obj) || IS_USER_TYPE(obj))

// Object is allocated/involves allocation on the heap.
#define IS_HEAP_OBJ(obj) \
    (((obj).type() >= ObjType::Module) && ((obj).type() <= ObjType::Ref))

// Object is a numeric object (Int or Dec).
#define IS_NUM(obj)         (IS_INT(obj) || IS_DEC(obj))

// Object is a Text or String object.
#define IS_STRING_LIKE(obj) (IS_TEXT(obj) || IS_STRING(obj))

// Object is iterable.
#define IS_ITERABLE(obj) \
    (((obj).type() >= ObjType::Text) && ((obj).type() <= ObjType::Table))

// Object is a collection (i.e., implements the [] operator).
#define IS_COLLECTION(obj) \
    (((obj).type() >= ObjType::Text) && ((obj).type() <= ObjType::Table))

// Object can be compared with <, >, or == operators.
#define IS_COMPARABLE(obj)  (IS_NUM(obj) || IS_TEXT(obj) || IS_STRING(obj))

// Object data is stored in-line within the object as a payload.
#define IS_PRIMITIVE(obj)   (!IS_HEAP_OBJ(obj) && !IS_ITER(obj))

// Object is a valid, initialized object.
#define IS_VALID(obj)       ((obj).type() != ObjType::Invalid)

// Object has internal state that may be mutated without rebinding.
#define HAS_MUT_STATE(obj)  (IS_COLLECTION(obj) || IS_INSTANCE(obj))

// Object was declared with "make" (mutable by default, can be reassigned).
#define IS_VAR(obj)         (((obj).type_ & FIXED_FLAG) == 0)

// Object was declared with "fix" (immutable by default, cannot be reassigned).
#define IS_FIXED(obj)       (((obj).type_ & FIXED_FLAG) != 0)

// Object has a mutability status/flag (only relevant for mutable types).
#define IS_INIT(obj)        (HAS_MUT_STATE(obj) && ((obj).type_ & INIT_FLAG) != 0)

// Object is immutable.
#define IS_IMMUT(obj)       (IS_INIT(obj) && (((obj).type_ & IMMUT_FLAG) != 0))

// Object is mutable.
#define IS_MUT(obj)         (IS_INIT(obj) && (((obj).type_ & IMMUT_FLAG) == 0))

#define MAKE_VAR(obj)       ((obj).type_ &= ~FIXED_FLAG)
#define MAKE_FIXED(obj)     ((obj).type_ |= FIXED_FLAG)

#define MAKE_INIT(obj)      ((obj).type_ |= INIT_FLAG)
#define MAKE_IMMUT(obj)     (MAKE_INIT(obj), ((obj).type_ |= IMMUT_FLAG))
#define MAKE_MUT(obj)       (MAKE_INIT(obj), ((obj).type_ &= ~IMMUT_FLAG))

#define AS_NUM(obj)         (IS_INT(obj) ? AS_INT(obj) : AS_DEC(obj))
#define AS_UINT(obj)        (static_cast<u64>(AS_INT(obj)))

inline std::string getElementText(const Object& obj)
{
    if (IS_STRING_LIKE(obj)) return CH_QUOTED(obj.printVal());
    return obj.printVal();
}

static inline u8 getMutFlags(const Object& obj)
{
    return (obj.type_ & (INIT_FLAG | IMMUT_FLAG));
}

static inline void setMutFlags(Object& obj, u8 flags)
{
    // Mutable containers cannot make any immutable
    // elements within them mutable, though the opposite
    // is fine.
    if (IS_IMMUT(obj)) return;

    // Clear flags.
    obj.type_ &= ~(INIT_FLAG | IMMUT_FLAG);
    // Set new flags.
    obj.type_ |= flags;
}


/* Heap-allocated object structs. */

// NOTE:
// All object struct constructors must be marked noexcept.
// This is because the allocation function used to allocate
// objects (when using an allocator) is marked noexcept.
// Any exception use in a constructor will thus result in
// std::terminate being called.

struct HeapObj
{
    #if !CH_USE_ALLOC
        int refCount{0};
    #endif

    HeapObj() noexcept = default;

    #if !CH_USE_ALLOC
        virtual ~HeapObj() = default;
    #endif
};

struct Module : public HeapObj
{
    const char* name{};
    ModuleTable entries{};

    Module(const std::string& name) noexcept;
    ~Module() noexcept;
    bool operator==(const Module& other);

    [[nodiscard]] Object getEntry(const std::string& name) const;

    void emit(std::ofstream& os) const;
    [[nodiscard]] u64 byteSize() const;
};

struct Type : public HeapObj
{
    using FieldPair = std::pair<std::string, bool>;

    const char* name{};
    std::vector<FieldPair> fields{};
    // Field default initializers. Each field has one.
    const ByteCode* fieldCode{nullptr};
    // Field-position table.
    // Position in 'fields' and 'fieldCode' arrays.
    HashTable<std::string, u8> fieldTable{};
    HashTable<std::string, Object> methods{};

    Type(
        const std::string& name,
        std::vector<FieldPair>& fields,
        const ByteCode* inits
    ) noexcept;
    ~Type() noexcept;

    void addMethod(const Object& method);
    bool defines(const std::string& method) const;

    void emit(std::ofstream& os) const;
    // Emits only metadata components.
    void emitMetadata(std::ofstream& os) const;
    [[nodiscard]] u64 byteSize() const;
};

struct Instance : public HeapObj
{
    const Type* type{};
    HashTable<std::string, Object> fields{};

    Instance(const Type* type) noexcept;
    bool operator==(const Instance& other) const;

    [[nodiscard]] Object* findField(const std::string& name);
    [[nodiscard]] const Object* findField(const std::string& name) const;

    [[nodiscard]] Object getField(const std::string& name) const;
    void setField(const std::string& name, const Object& value);
    // Will not perform immutability checks on the field.
    void initField(const std::string& name, const Object& value);

    [[nodiscard]] Hash hash() const;
    [[nodiscard]] std::string printVal() const;
};

struct Function : public HeapObj
{
    const char* name{nullptr};
    const ByteCode code{};
    const ByteCode* defaultArgs{nullptr};
    const u8 arityMin{}, arityMax{};
    bool variadic{false}; // Non-const to allow assignment.

    Function(
        const ByteCode& code,
        u8 arityMin = 0,
        u8 arityMax = 0
    ) noexcept;
    Function(
        const std::string& name,
        const ByteCode& code,
        u8 arityMin = 0,
        u8 arityMax = 0
    ) noexcept;
    ~Function() noexcept;

    void emit(std::ofstream& os) const;
    // Emits only metadata components.
    void emitMetadata(std::ofstream& os) const;
    u64 byteSize() const;
};

struct Closure : public HeapObj
{
    Function* function{};
    Array<Cell*> cells{};

    Closure(Function* function) noexcept;
    ~Closure() noexcept;

    void addCell(Cell* cell);
};

struct Method : public HeapObj
{
    const Object funcObj{}; // Function or closure.
    const Instance* boundInstance{};

    Method(const Object& funcObj) noexcept;
    bool operator==(const Method& other) const;

    [[nodiscard]] Hash hash() const;
};

struct Text : public HeapObj
{
    const char* str{};
    u64 len{};

    Text(const std::string_view& view) noexcept;
    Text(const char* str, size_t len) noexcept;
    ~Text();

    [[nodiscard]] Object getIndex(const Object& index) const;
    void setIndex(const Object& index, const Object& value);

    void reset(const std::string_view& view);
    operator std::string_view();

    // Parameter is not used, but it keeps our iterators
    // consistent.
    [[nodiscard]] std::string printVal(bool nested) const;
    void emit(std::ofstream& os) const;
    u64 byteSize() const;
};

struct String : public HeapObj
{
    std::string str{};

    String(const std::string& str) noexcept;
    String(const std::string_view& view) noexcept;
    String(const char* str, size_t len = -1) noexcept;

    [[nodiscard]] Object getIndex(const Object& index) const;
    void setIndex(const Object& index, const Object& value);

    [[nodiscard]] std::string printVal(bool nested) const;
    void emit(std::ofstream& os) const;
    u64 byteSize() const;
};

struct Range : public HeapObj
{
    const i64 start{};
    const i64 stop{};
    const i64 step{};

    Range(const std::array<i64, 3>& nums) noexcept;
    static void validateRange(const std::array<i64, 3>& nums);

    [[nodiscard]] bool operator==(const Range& other) const;
    [[nodiscard]] bool contains(const i64 num) const;

    [[nodiscard]] u64 length() const;
    [[nodiscard]] Object getIndex(const Object& index) const;
    void setIndex(const Object& index, const Object& value);

    [[nodiscard]] std::string printVal(bool nested) const;
};

struct List : public HeapObj
{
    Array<Object> array{};

    List(u64 size) noexcept;

    [[nodiscard]] bool operator==(const List& other) const;
    [[nodiscard]] bool contains(const Object& obj) const;

    [[nodiscard]] Object getIndex(const Object& index) const;
    void setIndex(const Object& index, const Object& value);

    Hash hash() const;
    [[nodiscard]] std::string printVal(bool nested) const;
};

struct ObjectHasher
{
    [[nodiscard]] Hash operator()(const Object& obj) const
    {
        return obj.hash();
    }
};

struct Table : public HeapObj
{
    ObjectTable table{};

    Table() noexcept = default;

    [[nodiscard]] bool operator==(const Table& other) const;
    [[nodiscard]] bool contains(const Object& obj) const;

    [[nodiscard]] Object getIndex(const Object& key) const;
    void setIndex(const Object& key, const Object& value);

    [[nodiscard]] std::string printVal(bool nested) const;
};

struct Cell : public HeapObj
{
    // Whether or not the captured object is an
    // element within a sequence.
    bool isElement{false};
    i64 index{};
    Object* location{};
    Object obj{};

    Cell(Object* location) noexcept;
    Cell(Object* obj, const Object& index) noexcept;
    void close();

    void assign(const Object& value);
};


/* Object iterator structs. */

struct TextIter
{
    Text* obj{};
    u64 pos{};
    // Text strings are inherently immutable, so no
    // flags needed.

    TextIter() noexcept = default;
    TextIter(Object& obj) noexcept;
    TextIter(const TextIter&) = delete;
    TextIter& operator=(const TextIter&) = delete;
    TextIter(TextIter&& other) noexcept;
    TextIter& operator=(TextIter&& other) noexcept;
    ~TextIter();

    [[nodiscard]] bool start(Object& var);
    [[nodiscard]] bool next(Object& var);
};

struct StringIter
{
    String* obj{};
    u64 pos{};
    const u8 flags{}; // Mutability flags for original object.

    StringIter() noexcept = default;
    StringIter(Object& obj) noexcept;
    StringIter(const StringIter&) = delete;
    StringIter& operator=(const StringIter&) = delete;
    StringIter(StringIter&& other) noexcept;
    StringIter& operator=(StringIter&& other) noexcept;
    ~StringIter();

    [[nodiscard]] bool start(Object& var);
    [[nodiscard]] bool next(Object& var);
};

struct RangeIter
{
    Range* obj{};
    // Integers are inherently immutable, so no
    // flags needed.
    i64 val{};

    RangeIter() noexcept = default;
    RangeIter(Object& obj) noexcept;
    RangeIter(const RangeIter&) = delete;
    RangeIter& operator=(const RangeIter&) = delete;
    RangeIter(RangeIter&& other) noexcept;
    RangeIter& operator=(RangeIter&& other) noexcept;
    ~RangeIter();

    [[nodiscard]] bool start(Object& var);
    [[nodiscard]] bool next(Object& var);
};

struct ListIter
{
    List* obj{};
    Array<Object>::iterator it{};
    const u8 flags{};

    ListIter() noexcept = default;
    ListIter(Object& obj) noexcept;
    ListIter(const ListIter&) = delete;
    ListIter& operator=(const ListIter&) = delete;
    ListIter(ListIter&& other) noexcept;
    ListIter& operator=(ListIter&& other) noexcept;
    ~ListIter();

    [[nodiscard]] bool start(Object& var);
    [[nodiscard]] bool next(Object& var);
};

struct TableIter
{
    Table* obj{};
    ObjectTable::iterator it{};
    const u8 flags{};

    TableIter() noexcept = default;
    TableIter(Object& obj) noexcept;
    TableIter(const TableIter&) = delete;
    TableIter& operator=(const TableIter&) = delete;
    TableIter(TableIter&&) noexcept;
    TableIter& operator=(TableIter&&) noexcept;
    ~TableIter();

    [[nodiscard]] bool start(Object& var);
    [[nodiscard]] bool next(Object& var);
};

struct ObjIter
{
    using Iter = std::variant<
        TextIter,
        StringIter,
        RangeIter,
        ListIter,
        TableIter
    >;

    Iter iter{};

    ObjIter() noexcept = default;
    ObjIter(Object& obj) noexcept;
    ~ObjIter() noexcept = default;

    [[nodiscard]] bool start(Object& var);
    [[nodiscard]] bool next(Object& var);
};


/* Deallocation functor. */

template<typename ObjT>
struct CustomDealloc
{
    void operator()(void* mem) noexcept
    {
        ObjT* obj = reinterpret_cast<ObjT*>(mem);
        obj->~ObjT();
    }
};

#undef TYPE_LIST
#pragma pop_macro("AS_VOID")
#pragma pop_macro("NULL")