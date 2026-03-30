#pragma once
#include "bytecode.h"
#include "common.h"
#include "natives.h"
#include "opcodes.h"
#include "../dependencies/personal/array.h"
#include "../dependencies/personal/linearTable.h"
#include <array>
#include <string>
#include <string_view>
#include <variant>
using Natives::FuncType;

/* Type enum. */

enum ObjType
{
    OBJ_INT,
    OBJ_DEC,
    OBJ_BOOL,
    OBJ_NULL,
    OBJ_TYPE,
    OBJ_NATIVE,
    OBJ_FUNC,
    OBJ_CLOSURE,
    OBJ_LAMBDA,
    OBJ_BIGINT,
    OBJ_BIGDEC,
    OBJ_STRING,
    OBJ_RANGE,
    OBJ_LIST,
    OBJ_TABLE,

    // Internal types.

    // Used in function return values.
    OBJ_TUPLE,
    // Used in TypeMismatch errors.
    OBJ_NUM,
    // Used in for-loops.
    OBJ_ITER,

    OBJ_INVALID,
};


/* Type check and validation macros. */

#define IS_(TYPE, obj)      ((obj).type == OBJ_##TYPE)
// Object is a function object.
#define IS_FUNC(obj) \
    (((obj).type == OBJ_FUNC) || ((obj).type == OBJ_LAMBDA) || ((obj).type == OBJ_CLOSURE))
// Object can be called.
#define IS_CALLABLE(obj)    (IS_(NATIVE, obj) || IS_FUNC(obj))
// Object is allocated/involves allocation on the heap.
#define IS_HEAP_OBJ(obj)    (((obj).type >= OBJ_FUNC) && ((obj).type <= OBJ_TUPLE))
// Object is a numeric object (int or dec/float).
#define IS_NUM(obj)         (IS_(INT, obj) || IS_(DEC, obj))
// Object is iterable.
#define IS_ITERABLE(obj)    (((obj).type >= OBJ_STRING) && ((obj).type <= OBJ_TABLE))
// Object data is stored in-line within the object as a payload.
#define IS_PRIMITIVE(obj)   (!IS_HEAP_OBJ(obj) && !IS_(ITER, obj))
// Object is a valid, initialized object.
#define IS_VALID(obj)       ((obj).type != OBJ_INVALID)


/* Conversion macros. */

#define AS_(TYPE, obj)      ((obj).as.TYPE##Val)
#define AS_HEAP_PTR(obj)    ((obj).as.heapVal)
#define AS_NUM(obj)         (IS_(INT, obj) ? AS_(int, obj) : AS_(dec, obj))
#define AS_UINT(obj)        (static_cast<ui64>(AS_(int, obj)))


/* Forward declarations. */

struct Function;
struct Closure;
struct String;
struct Range;
struct List;
struct Table;
struct Tuple;
struct HeapObj;
struct ObjIter;


/* Main object class. */

class Object
{
    private:
        void clean();

    public:
        ObjType type;
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
            Tuple*      tupleVal;
            HeapObj*    heapVal;
            ObjIter*    iterVal;
        } as;

        Object();
        template<typename T>
        Object(T val);
        Object(const Object& other) noexcept;
        Object& operator=(const Object& other) noexcept;
        Object(Object&& other) noexcept;
        Object& operator=(Object&& other) noexcept;
        ~Object();

        bool operator==(const Object& other) const;
        bool operator>(const Object& other) const;
        bool operator<(const Object& other) const;
        bool in(const Object& other) const;

        std::string printVal() const;
        std::string_view printType() const;
        void emit(std::ofstream& os) const;

        ObjIter* makeIter();
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


/* Heap-allocated object structs. */

struct HeapObj
{
    ObjType type;
    int refCount;

    HeapObj();
    HeapObj(ObjType type);
    virtual ~HeapObj() = default;
};

struct Cell
{
    Object* location;
    Object obj;
    int refCount;

    Cell(Object* location);
    void close();
};

struct Function : public HeapObj
{
    char* name;
    ByteCode code;
    ui8 argCount;
    bool lambda;

    Function(const ByteCode& code, ui8 argCount);
    Function(const std::string& name, const ByteCode& code, ui8 argCount);
    ~Function();

    bool operator==(const Function& other) const;

    void emit(std::ofstream& os) const;
};

struct Closure : public HeapObj
{
    Function* function;
    Array<Cell*> cells;

    Closure(Function* function);
    ~Closure();

    bool operator==(const Closure& other) const;

    void addCell(Cell* cell);
};

struct String : public HeapObj
{
    std::string str;

    String(const std::string& str);
    String(const std::string_view& view);
    String(const char* str, size_t len = -1);

    bool operator==(const String& other) const;
    bool contains(const String& substr) const;

    std::string printVal() const;
    void emit(std::ofstream& os) const;
};

struct Range : public HeapObj
{
    i64 start;
    i64 stop;
    i64 step;

    Range(const std::array<i64, 3>& limits);

    bool operator==(const Range& other) const;
    bool contains(const i64 num) const;

    i64 length() const;
    std::string printVal() const;
    void emit(std::ofstream& os) const;
};

struct List : public HeapObj
{
    Array<Object> array;

    List(ui32 size);

    bool operator==(const List& other) const;
    bool contains(const Object& obj) const;

    std::string printVal() const;
};

struct Table : public HeapObj
{
    linearTable<Object, Object> table;
};

struct Tuple : public HeapObj
{
    Array<Object> entries;

    Tuple();
    Tuple(ui32 size);

    std::string printVal() const;
};


/* Object iterator structs. */

struct StringIter
{
    String* obj;
    const char* begin;

    StringIter();
    StringIter(String* obj);
    StringIter(const StringIter&) = delete;
    StringIter& operator=(const StringIter&) = delete;
    StringIter(StringIter&& other) noexcept;
    StringIter& operator=(StringIter&& other) noexcept;
    ~StringIter();

    bool start(Object& var);
    bool next(Object& var);
};

struct RangeIter
{
    Range* obj;
    i64 val;

    RangeIter();
    RangeIter(Range* obj);
    RangeIter(const RangeIter&) = delete;
    RangeIter& operator=(const RangeIter&) = delete;
    RangeIter(RangeIter&& other) noexcept;
    RangeIter& operator=(RangeIter&& other) noexcept;
    ~RangeIter();

    bool start(Object& var);
    bool next(Object& var);
};

struct ListIter
{
    List* obj;
    Array<Object>::iterator it;

    ListIter();
    ListIter(List* obj);
    ListIter(const ListIter&) = delete;
    ListIter& operator=(const ListIter&) = delete;
    ListIter(ListIter&& other) noexcept;
    ListIter& operator=(ListIter&& other) noexcept;
    ~ListIter();

    bool start(Object& var);
    bool next(Object& var);
};

struct ObjIter
{
    using Iter = std::variant<
        StringIter,
        RangeIter,
        ListIter
    >;

    Iter iter;

    ObjIter() = default;
    ObjIter(Object& obj);
    ~ObjIter() = default;

    bool start(Object& var);
    bool next(Object& var);
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


/* General type mismatch error class. */

struct TypeMismatch
{    
    std::string message;
    ObjType expect;
    ObjType actual;

    TypeMismatch(const std::string& message, ObjType expect,
        ObjType actual);
    void report();
};
