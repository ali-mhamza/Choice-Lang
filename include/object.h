#pragma once
#include "bytecode.h"
#include "common.h"
#include "natives.h"
#include <personal/array.h>
#include <personal/linearTable.h>
#include <array>
#include <string>
#include <string_view>
#include <variant>

using Natives::FuncType;

/* Type list macro. */

#pragma push_macro("NULL")
#pragma push_macro("AS_VOID")
#undef NULL
#undef AS_VOID

#define TYPE_LIST           \
    X(INT, intVal)          \
    X(DEC, decVal)          \
    X(BOOL, boolVal)        \
    X(NULL, heapVal)        \
    X(TYPE, typeVal)        \
    X(NATIVE, nativeVal)    \
    X(FUNC, funcVal)        \
    X(CLOSURE, closureVal)  \
    X(LAMBDA, funcVal)      \
    X(BIGINT, heapVal)      \
    X(BIGDEC, heapVal)      \
    X(STRING, stringVal)    \
    X(RANGE, rangeVal)      \
    X(LIST, listVal)        \
    X(TABLE, tableVal)      \
    X(REF, refVal)          \
    /* Used in function return values. */   \
    X(VOID, voidVal)        \
    /* Used in for-loops. */                \
    X(ITER, iterVal)        \

/* Type enum. */

#define X(TYPE, field) OBJ_##TYPE,

enum ObjType : u8
{
    TYPE_LIST

    // Used in TypeMismatch errors.
    NUM_TYPES,
    OBJ_INVALID,
};

#undef X


/* Forward declarations. */

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
        ObjType type{};
        union Value {
            i64         intVal;
            double      decVal;
            bool        boolVal;
            ObjType     typeVal;
            FuncType    nativeVal;
            Function*   funcVal;
            Closure*    closureVal;
            String*     stringVal;
            Range*      rangeVal;
            List*       listVal;
            Table*      tableVal;
            Cell*       refVal;
            Void*       voidVal;
            HeapObj*    heapVal;
            ObjIter*    iterVal;
        } as;

        Object();
        template<typename T>
        Object(T val);

        #if !CH_USE_ALLOC
            Object(const Object& other) noexcept;
            Object& operator=(const Object& other) noexcept;
            Object(Object&& other) noexcept;
            Object& operator=(Object&& other) noexcept;
            ~Object();
        #endif

        [[nodiscard]] bool operator==(const Object& other) const;
        [[nodiscard]] bool operator>(const Object& other) const;
        [[nodiscard]] bool operator<(const Object& other) const;
        [[nodiscard]] bool in(const Object& other) const;

        [[nodiscard]] std::string printVal() const;
        [[nodiscard]] std::string_view printType() const;
        void emit(std::ofstream& os) const;

        [[nodiscard]] ObjIter* makeIter();
};

template<typename T>
Object::Object(T val)
{
    #if !CH_USE_ALLOC
        #define INCREMENT_REF() val->refCount++;
    #else
        #define INCREMENT_REF()
    #endif

    if constexpr (std::is_same_v<T, i64>)
    {
        type = OBJ_INT;
        as.intVal = val;
    }
    else if constexpr (std::is_same_v<T, double>)
    {
        type = OBJ_DEC;
        as.decVal = val;
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
        type = OBJ_BOOL;
        as.boolVal = val;
    }
    else if constexpr (std::is_same_v<T, std::nullptr_t>)
    {
        type = OBJ_NULL;
        as.heapVal = val; // Dummy assignment.
    }
    else if constexpr (std::is_same_v<T, ObjType>)
    {
        type = OBJ_TYPE;
        as.typeVal = val;
    }
    else if constexpr (std::is_same_v<T, FuncType>)
    {
        type = OBJ_NATIVE;
        as.nativeVal = val;
    }
    else if constexpr (std::is_same_v<T, ObjIter*>)
    {
        type = OBJ_ITER;
        // Iterators should never be copied, so we
        // don't use a refcount.
        as.iterVal = val;
    }
    else
    {
        type = val->type;
        INCREMENT_REF();
        as.heapVal = val;
    }

    #undef INCREMENT_REF
}


/* Type check, validation and conversion macros. */

#define X(TYPE, field) \
    [[nodiscard]] static inline bool IS_##TYPE(const Object& obj) { \
        return (obj.type == OBJ_##TYPE);                            \
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
    (((obj).type == OBJ_FUNC) || ((obj).type == OBJ_LAMBDA) || ((obj).type == OBJ_CLOSURE))
// Object can be called.
#define IS_CALLABLE(obj)    (IS_NATIVE(obj) || IS_FUNCOBJ(obj))
// Object is allocated/involves allocation on the heap.
#define IS_HEAP_OBJ(obj)    (((obj).type >= OBJ_FUNC) && ((obj).type <= OBJ_VOID))
// Object is a numeric object (int or dec/float).
#define IS_NUM(obj)         (IS_INT(obj) || IS_DEC(obj))
// Object is iterable.
#define IS_ITERABLE(obj)    (((obj).type >= OBJ_STRING) && ((obj).type <= OBJ_TABLE))
// Object can be compared with <, >, or == operators.
#define IS_COMPARABLE(obj)  (IS_NUM(obj) || ((obj).type == OBJ_STRING))
// Object data is stored in-line within the object as a payload.
#define IS_PRIMITIVE(obj)   (!IS_HEAP_OBJ(obj) && !IS_ITER(obj))
// Object is a valid, initialized object.
#define IS_VALID(obj)       ((obj).type != OBJ_INVALID)

#define AS_HEAP_PTR(obj)    ((obj).as.heapVal)
#define AS_NUM(obj)         (IS_INT(obj) ? AS_INT(obj) : AS_DEC(obj))
#define AS_UINT(obj)        (static_cast<u64>(AS_INT(obj)))


/* Heap-allocated object structs. */

struct HeapObj
{
    ObjType type{};

    #if !CH_USE_ALLOC
        int refCount{0};
    #endif

    HeapObj();
    HeapObj(ObjType type);

    #if !CH_USE_ALLOC
        virtual ~HeapObj() = default;
    #endif
};

struct Cell : public HeapObj
{
    Object* location{};
    Object obj{};

    Cell(Object* location);
    void close();
};

struct DebugRange;

struct Function : public HeapObj
{
    const char* name{};
    const ByteCode code{};
    const u8 argCount{};
    const bool lambda{};

    Function(const ByteCode& code, const u8 argCount);
    Function(
        const std::string& name,
        const ByteCode& code,
        const u8 argCount
    );
    ~Function();

    [[nodiscard]] bool operator==(const Function& other) const;

    const DebugRange& getErrorRange(const u8* ip) const;
    void emit(std::ofstream& os) const;
    u64 byteSize() const;
};

struct Closure : public HeapObj
{
    Function* function{};
    Array<Cell*> cells{};

    Closure(Function* function);
    ~Closure();

    [[nodiscard]] bool operator==(const Closure& other) const;

    void addCell(Cell* cell);
};

struct String : public HeapObj
{
    std::string str{};

    String(const std::string& str);
    String(const std::string_view& view);
    String(const char* str, size_t len = -1);

    [[nodiscard]] bool operator==(const String& other) const;
    [[nodiscard]] bool contains(const String& substr) const;

    [[nodiscard]] std::string printVal() const;
    void emit(std::ofstream& os) const;
    u64 byteSize() const;
};

struct Range : public HeapObj
{
    const i64 start{};
    const i64 stop{};
    const i64 step{};

    Range(const std::array<i64, 3>& limits);

    [[nodiscard]] bool operator==(const Range& other) const;
    [[nodiscard]] bool contains(const i64 num) const;

    [[nodiscard]] i64 length() const;
    [[nodiscard]] std::string printVal() const;
};

struct List : public HeapObj
{
    Array<Object> array{};

    List(u32 size);

    [[nodiscard]] bool operator==(const List& other) const;
    [[nodiscard]] bool contains(const Object& obj) const;

    [[nodiscard]] std::string printVal() const;
};

struct Table : public HeapObj
{
    linearTable<Object, Object> table{};
};

struct Void : public HeapObj
{
    Void();
};


/* Object iterator structs. */

struct StringIter
{
    String* obj{};
    u64 pos;

    StringIter() = default;
    StringIter(String* obj);
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
    i64 val{};

    RangeIter() = default;
    RangeIter(Range* obj);
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

    ListIter() = default;
    ListIter(List* obj);
    ListIter(const ListIter&) = delete;
    ListIter& operator=(const ListIter&) = delete;
    ListIter(ListIter&& other) noexcept;
    ListIter& operator=(ListIter&& other) noexcept;
    ~ListIter();

    [[nodiscard]] bool start(Object& var);
    [[nodiscard]] bool next(Object& var);
};

struct ObjIter
{
    using Iter = std::variant<
        StringIter,
        RangeIter,
        ListIter
    >;

    Iter iter{};

    ObjIter() = default;
    ObjIter(Object& obj);
    ~ObjIter() = default;

    [[nodiscard]] bool start(Object& var);
    [[nodiscard]] bool next(Object& var);
};


/* Deallocation functor. */

template<typename ObjT>
struct CustomDealloc
{
    void operator()(void* mem)
    {
        ObjT* obj = reinterpret_cast<ObjT*>(mem);
        obj->~ObjT();
    }
};

#undef TYPE_LIST
#pragma pop_macro("AS_VOID")
#pragma pop_macro("NULL")