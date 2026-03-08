#pragma once
#include "bytecode.h"
#include "common.h"
#include "natives.h"
#include "opcodes.h"
#include "../dependencies/personal/array.h"
#include "../dependencies/personal/linearTable.h"
#include <array>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
using Natives::FuncType;

enum ObjType
{
    OBJ_INT,
    OBJ_DEC,
    OBJ_BOOL,
    OBJ_NULL,
    OBJ_TYPE,
    OBJ_NATIVE,
    OBJ_FUNC,
    OBJ_BIGINT,
    OBJ_BIGDEC,
    OBJ_STRING,
    OBJ_RANGE,
    OBJ_LIST,
    OBJ_TABLE,
    // OBJ_NUM only used with TypeMismatch (below).
    // Not to be stored in any Object.
    OBJ_NUM,
    // Only used internally for for-loops.
    OBJ_ITER,
    OBJ_INVALID,
};

struct HeapObj
{
    ObjType type;
    int refCount;

    HeapObj();
    HeapObj(ObjType type);
    virtual ~HeapObj() = default;
};

struct Function : public HeapObj
{
    const char* name;
    ByteCode code;
    ui8 argCount;
    bool lambda;

    Function() = default;
    Function(const ByteCode& code, ui8 argCount);
    Function(const std::string& name, const ByteCode& code, ui8 argCount);
    bool operator==(const Function& other) const;

    void emit(std::ofstream& os) const;
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
    std::string printVal() const;
    void emit(std::ofstream& os) const;
};

class Object;

struct List : public HeapObj
{
    Array<Object> array;

    List() = default;
    List(ui32 size);
    bool operator==(const List& other) const;

    bool contains(const Object& obj) const;
    std::string printVal() const;
    void emit(std::ofstream& os) const;
};

struct Table : public HeapObj
{
    linearTable<Object, Object> table;
};

struct ObjIter;

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
            String*     stringVal;
            Range*      rangeVal;
            List*       listVal;
            Table*      tableVal;
            HeapObj*    heapVal;
            ObjIter*    iterVal;
        } as;

        Object();
        template<typename T>
        Object(T val);
        Object(const Object& other);
        Object& operator=(const Object& other);
        Object(Object&& other) noexcept;
        Object& operator=(Object&& other) noexcept;
        ~Object();

        bool operator==(const Object& other) const;
        bool operator>(const Object& other) const;
        bool operator<(const Object& other) const;

        std::string printVal() const;
        std::string_view printType() const;
        void emit(std::ofstream& os) const;

        ObjIter* makeIter();
};

struct StringIter
{
    String* obj;
    String* iter;
    const char* begin;

    StringIter();
    StringIter(String* obj);
    StringIter(const StringIter&) = delete;
    StringIter& operator=(const StringIter&) = delete;
    StringIter(StringIter&& other);
    StringIter& operator=(StringIter&& other);
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
    RangeIter(RangeIter&& other);
    RangeIter& operator=(RangeIter&& other);
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
    ListIter(ListIter&& other);
    ListIter& operator=(ListIter&& other);
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

// Deallocation functor.

template<typename ObjT>
struct ObjDealloc
{
    void operator()(void* mem)
    {
        ObjT* obj = reinterpret_cast<ObjT*>(mem);
        obj->~ObjT();
    }
};

// General type mismatch error class.

struct TypeMismatch
{    
    std::string message;
    ObjType expect;
    ObjType actual;

    TypeMismatch(const std::string& message, ObjType expect,
        ObjType actual);
    void report();
};

template<typename T>
Object::Object(T val)
{
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
    else if constexpr (std::is_same_v<T, Function*>)
    {
        type = OBJ_FUNC;
        #if !USE_ALLOC
            val->refCount++;
        #endif
        as.funcVal = val;
    }
    else if constexpr (std::is_same_v<T, String*>)
    {
        type = OBJ_STRING;
        #if !USE_ALLOC
            val->refCount++;
        #endif
        as.stringVal = val;
    }
    else if constexpr (std::is_same_v<T, Range*>)
    {
        type = OBJ_RANGE;
        #if !USE_ALLOC
            val->refCount++;
        #endif
        as.rangeVal = val;
    }
    else if constexpr (std::is_same_v<T, List*>)
    {
        type = OBJ_LIST;
        #if !USE_ALLOC
            val->refCount++;
        #endif
        as.listVal = val;
    }
    else if constexpr (std::is_same_v<T, Table*>)
    {
        type = OBJ_TABLE;
        #if !USE_ALLOC
            val->refCount++;
        #endif
        as.tableVal = val;
    }
    else if constexpr (std::is_same_v<T, HeapObj*>)
    {
        type = val->type;
        #if !USE_ALLOC
            val->refCount++;
        #endif
        as.heapVal = val;
    }
    else if constexpr (std::is_same_v<T, ObjIter*>)
    {
        type = OBJ_ITER;
        // Iterators should never be copied, so we
        // don't use a refcount.
        as.iterVal = val;
    }
}

// Type check and validation macros.

#define IS_(TYPE, obj)      ((obj).type == OBJ_##TYPE)
#define IS_CALLABLE(obj)    (IS_(NATIVE, obj) || IS_(FUNC, obj))
#define IS_HEAP_OBJ(obj)    (((obj).type > OBJ_NATIVE) && ((obj).type < OBJ_NUM))
#define IS_NUM(obj)         (IS_(INT, obj) || IS_(DEC, obj))
#define IS_ITERABLE(obj)    (((obj).type >= OBJ_STRING) && ((obj).type <= OBJ_TABLE))
#define IS_PRIMITIVE(obj)   (!IS_HEAP_OBJ(obj) && !IS_(ITER, obj))
#define IS_VALID(obj)       ((obj).type != OBJ_INVALID)

// Conversion macros.

#define AS_(TYPE, obj)      ((obj).as.TYPE##Val)
#define AS_HEAP_PTR(obj)    ((obj).as.heapVal)
#define AS_NUM(obj)         (IS_(INT, obj) ? AS_(int, obj) : AS_(dec, obj))
#define AS_UINT(obj)        (static_cast<ui64>(AS_(int, obj)))
