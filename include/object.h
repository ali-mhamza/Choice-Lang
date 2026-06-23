#pragma once
#include "bytecode.h"
#include "common.h"
#include "natives.h"
#include <personal/array.h>
#include <personal/hash_table.h>
#include <array>
#include <fstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

using Natives::FuncType;

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

#define TYPE_LIST               \
    X(INT, intVal)              \
    X(DEC, decVal)              \
    X(BOOL, boolVal)            \
    X(NULL, heapVal)            \
    X(CORE_TYPE, coreTypeVal)   \
    X(CORE_FUNC, coreFuncVal)   \
    X(USER_TYPE, userTypeVal)   \
    X(INSTANCE, instanceVal)    \
    X(USER_FUNC, userFuncVal)   \
    X(CLOSURE, closureVal)      \
    X(LAMBDA, userFuncVal)      \
    X(BIGINT, heapVal)          \
    X(BIGDEC, heapVal)          \
    X(STRING, stringVal)        \
    X(RANGE, rangeVal)          \
    X(LIST, listVal)            \
    X(TABLE, tableVal)          \
    X(REF, refVal)              \
    X(VOID, voidVal)            \
    /* Used in for-loops. */    \
    X(ITER, iterVal)            \

/* Type enum. */

#define X(TYPE, field) OBJ_##TYPE,

enum ObjType : u8
{
    TYPE_LIST

    NUM_TYPES,
    OBJ_INVALID,
};

#undef X


/* Forward declarations. */

struct Type;
struct Instance;
struct Function;
struct Closure;
struct String;
struct Range;
struct List;
struct Table;
struct Cell;
struct Void;
struct HeapObj;
struct ObjIter;


/* Main object class. */

class Object
{
    private:
        #if !CH_USE_ALLOC
            void clean();
        #endif

    public:
        u8 type_{};
        union Value {
            i64             intVal;
            double          decVal;
            bool            boolVal;
            ObjType         coreTypeVal;
            Type*           userTypeVal;
            Instance*       instanceVal;
            FuncType        coreFuncVal;
            Function*       userFuncVal;
            Closure*        closureVal;
            String*         stringVal;
            Range*          rangeVal;
            List*           listVal;
            Table*          tableVal;
            Cell*           refVal;
            Void*           voidVal;
            HeapObj*        heapVal;
            ObjIter*        iterVal;
            const HeapObj*  dummyVal;
        } as;

        Object() noexcept : type_{OBJ_INVALID}, as{0} {}
        explicit Object(ObjType type) : type_{type}, as{0} {};
        template<typename T>
        Object(T val) noexcept;

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

        // Get the size of the collection object payload.
        // Only valid to call for collections.
        [[nodiscard]] u64 collectionSize() const;

        [[nodiscard]] Hash hash() const;
        [[nodiscard]] std::string printVal() const;
        [[nodiscard]] std::string_view printType() const;
        void emit(std::ofstream& os) const;

        [[nodiscard]] ObjIter* makeIter();
};

template<typename T>
ObjType getObjectType(T val)
{
    using U = std::remove_const_t<std::remove_pointer_t<T>>;

    if constexpr (std::is_same_v<U, Function>)
    {
        if (val->name == nullptr) return OBJ_LAMBDA;
        return OBJ_USER_FUNC;
    }

    if constexpr (std::is_same_v<U, Type>)      return OBJ_USER_TYPE;
    if constexpr (std::is_same_v<U, Instance>)  return OBJ_INSTANCE;
    if constexpr (std::is_same_v<U, Closure>)   return OBJ_CLOSURE;
    if constexpr (std::is_same_v<U, String>)    return OBJ_STRING;
    if constexpr (std::is_same_v<U, Range>)     return OBJ_RANGE;
    if constexpr (std::is_same_v<U, List>)      return OBJ_LIST;
    if constexpr (std::is_same_v<U, Table>)     return OBJ_TABLE;
    if constexpr (std::is_same_v<U, Cell>)      return OBJ_REF;
    if constexpr (std::is_same_v<U, Void>)      return OBJ_VOID;

    return OBJ_INVALID; // Dummy return value.
}

template<typename T>
Object::Object(T val) noexcept
{
    if constexpr (std::is_same_v<T, i64>)
    {
        type_ = OBJ_INT;
        as.intVal = val;
    }
    else if constexpr (std::is_same_v<T, double>)
    {
        type_ = OBJ_DEC;
        as.decVal = val;
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
        type_ = OBJ_BOOL;
        as.boolVal = val;
    }
    else if constexpr (std::is_same_v<T, std::nullptr_t>)
    {
        type_ = OBJ_NULL;
        as.heapVal = val; // Dummy assignment.
    }
    else if constexpr (std::is_same_v<T, ObjType>)
    {
        type_ = OBJ_CORE_TYPE;
        as.coreTypeVal = val;
    }
    else if constexpr (std::is_same_v<T, FuncType>)
    {
        type_ = OBJ_CORE_FUNC;
        as.coreFuncVal = val;
    }
    else if constexpr (std::is_same_v<T, ObjIter*>)
    {
        type_ = OBJ_ITER;
        // Iterators should never be copied, so we
        // don't use a refcount.
        as.iterVal = val;
    }
    else
    {
        type_ = getObjectType(val);

        #if !CH_USE_ALLOC
            val->refCount++;
        #endif

        if constexpr (std::is_const_v<std::remove_pointer_t<T>>)
            as.dummyVal = val;
        else
            as.heapVal = val;
    }
}


/* Object type names. */

inline constexpr std::array<std::string_view, NUM_TYPES> objTypes{
    "Int", "Dec", "Bool", "Null", "Builtin Type",
    "Builtin Function", "User Type", "Type Instance",
    "User Function", "User Function", "Lambda", "BigInt",
    "BigDec", "String", "Range", "List", "Table",
    "", // References take the type of the contained object.
    "Void", "Iterable"
};


/* Object helper functions and macros. */

#define X(TYPE, field) \
    [[nodiscard]] static inline bool IS_##TYPE(const Object& obj) { \
        return (obj.type() == OBJ_##TYPE);                          \
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
#define IS_FUNCOBJ(obj)     (IS_USER_FUNC(obj) || IS_LAMBDA(obj) || IS_CLOSURE(obj))

// Object can be called.
#define IS_CALLABLE(obj) \
    (IS_CORE_FUNC(obj) || IS_FUNCOBJ(obj) || IS_CORE_TYPE(obj) || IS_USER_TYPE(obj))

// Object is allocated/involves allocation on the heap.
#define IS_HEAP_OBJ(obj)    (((obj).type() >= OBJ_USER_TYPE) && ((obj).type() <= OBJ_VOID))

// Object is a numeric object (int or dec/float).
#define IS_NUM(obj)         (IS_INT(obj) || IS_DEC(obj))

// Object is iterable.
#define IS_ITERABLE(obj)    (((obj).type() >= OBJ_STRING) && ((obj).type() <= OBJ_TABLE))

// Object is a collection (i.e., implements the [] operator).
#define IS_COLLECTION(obj)  (((obj).type() >= OBJ_STRING) && ((obj).type() <= OBJ_TABLE))

// Object can be compared with <, >, or == operators.
#define IS_COMPARABLE(obj)  (IS_NUM(obj) || ((obj).type() == OBJ_STRING))

// Object data is stored in-line within the object as a payload.
#define IS_PRIMITIVE(obj)   (!IS_HEAP_OBJ(obj) && !IS_ITER(obj))

// Object is a valid, initialized object.
#define IS_VALID(obj)       ((obj).type() != OBJ_INVALID)

// Object has internal state that may be mutated without rebinding.
// For now only applies to collection types.
#define HAS_MUT_STATE(obj)  (IS_COLLECTION(obj))

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

#define AS_HEAP_PTR(obj)    ((obj).as.heapVal)
#define AS_NUM(obj)         (IS_INT(obj) ? AS_INT(obj) : AS_DEC(obj))
#define AS_UINT(obj)        (static_cast<u64>(AS_INT(obj)))

inline std::string getElementText(const Object& obj)
{
    if (IS_STRING(obj)) return CH_QUOTED(obj.printVal());
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

struct Type : public HeapObj
{
    const char* name{};
    std::vector<std::string> fields{};
    HashTable<std::string, Object> methods{};

    Type(const std::string& name, vT& fields) noexcept;
    Type(const std::string& name, std::vector<std::string>& fields) noexcept;
    ~Type() noexcept;

    void emit(std::ofstream& os) const;
    [[nodiscard]] u64 byteSize() const;
};

struct Instance : public HeapObj
{
    const Type* type{};
    HashTable<std::string, Object> fields{};

    Instance(const Type* type) noexcept;
    bool operator==(const Instance& other) const;

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

struct String : public HeapObj
{
    std::string str{};

    String(const std::string& str) noexcept;
    String(const std::string_view& view) noexcept;
    String(const char* str, size_t len = -1) noexcept;

    [[nodiscard]] bool operator==(const String& other) const;
    [[nodiscard]] bool contains(const String& substr) const;

    [[nodiscard]] Object getIndex(const Object& index) const;
    void setIndex(const Object& index, const Object& value);

    // Parameter is not used, but it keeps our iterators
    // consistent.
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

    List(u32 size) noexcept;

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
    HashTable<Object, Object, ObjectHasher> table{};

    Table() noexcept = default;

    [[nodiscard]] bool operator==(const Table& other) const;
    [[nodiscard]] bool contains(const Object& obj) const;

    [[nodiscard]] Object getIndex(const Object& key) const;
    void setIndex(const Object& key, const Object& value);

    [[nodiscard]] std::string printVal(bool nested) const;
};

struct Cell : public HeapObj
{
    Object* location{};
    Object obj{};

    Cell(Object* location) noexcept;
    void close();
};

struct Void : public HeapObj
{
    Void() noexcept = default;
};


/* Object iterator structs. */

struct StringIter
{
    String* obj{};
    u64 pos;
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
    HashTable<Object, Object, ObjectHasher>::iterator it{};
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