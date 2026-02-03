#pragma once
#include "common.h"
#include "opcodes.h"
#include <array>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <variant>

enum ObjType
{
    OBJ_INT,
    OBJ_DEC,
    OBJ_BOOL,
    OBJ_NULL,
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

    bool contains(const Object& num) const;
    std::string printVal() const;
    void emit(std::ofstream& os) const;
};

struct List : public HeapObj
{

};

struct Table : public HeapObj
{

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
            double      doubleVal;
            bool        boolVal;
            String*     stringVal;
            Range*      rangeVal;
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
        std::string printType() const;
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

struct ObjIter
{
    using Iter = std::variant<StringIter, RangeIter>;

    Iter iter;

    ObjIter() = default;
    ObjIter(Object& obj);
    ~ObjIter() = default;

    bool start(Object& var);
    bool next(Object& var);
};

// Deallocation functors.

struct StringDealloc    { void operator()(void* mem); };
struct RangeDealloc     { void operator()(void* mem); };
struct ObjIterDealloc   { void operator()(void* mem); };

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
        as.doubleVal = val;
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
        type = OBJ_BOOL;
        as.boolVal = val;
    }
    else if constexpr (std::is_same_v<T, nullptr_t>)
    {
        type = OBJ_NULL;
        as.heapVal = val; // Dummy assignment.
    }
    else if constexpr (std::is_same_v<T, String*>)
    {
        type = OBJ_STRING;
        #ifndef USE_ALLOC
            val->refCount++;
        #endif
        as.stringVal = val;
    }
    else if constexpr (std::is_same_v<T, Range*>)
    {
        type = OBJ_RANGE;
        #ifndef USE_ALLOC
            val->refCount++;
        #endif
        as.rangeVal = val;
    }
    else if constexpr (std::is_same_v<T, HeapObj*>)
    {
        type = val->type;
        #ifndef USE_ALLOC
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

#define IS_INT(obj)         ((obj).type == OBJ_INT)
#define IS_DEC(obj)         ((obj).type == OBJ_DEC)
#define IS_BOOL(obj)        ((obj).type == OBJ_BOOL)
#define IS_NULL(obj)        ((obj).type == OBJ_NULL)
#define IS_STRING(obj)      ((obj).type == OBJ_STRING)
#define IS_RANGE(obj)       ((obj).type == OBJ_RANGE)

#define IS_HEAP_TYPE(type)  (((type) > OBJ_NULL) && ((type) < OBJ_NUM))
#define IS_HEAP_OBJ(obj)    (IS_HEAP_TYPE((obj).type))
#define IS_ITER(obj)        ((obj).type == OBJ_ITER)

#define IS_VALID(obj)       ((obj).type != OBJ_INVALID)
#define IS_NUM(obj)         (IS_INT(obj) || IS_DEC(obj))
#define IS_ITERABLE(obj)    (((obj).type >= OBJ_STRING) && ((obj).type <= OBJ_TABLE))


#define AS_INT(obj)         ((obj).as.intVal)
#define AS_DEC(obj)         ((obj).as.doubleVal)
#define AS_BOOL(obj)        ((obj).as.boolVal)
#define AS_STRING(obj)      (*((obj).as.stringVal))
#define AS_RANGE(obj)       (*((obj).as.rangeVal))

#define AS_HEAP_PTR(obj)    ((obj).as.heapVal)
#define AS_ITER(obj)        ((obj).as.iterVal)

#define AS_NUM(obj)         ((obj).type == OBJ_INT ? AS_INT(obj) : AS_DEC(obj))
#define AS_UINT(obj)        (static_cast<ui64>(AS_INT(obj)))
