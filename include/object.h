#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <variant>

namespace Object
{
    enum ObjType : uint8_t
    {
        OBJ_BASE,
        OBJ_INT,
        OBJ_UINT,
        OBJ_DEC,
        OBJ_BOOL,
        OBJ_STRING,
        OBJ_NULL,
        OBJ_INVALID
    };
    
    struct Base
    {
        ObjType type;   // Object type.
        size_t size;    // Object size.

        Base();
        Base(ObjType type, size_t size);
    };

    using BaseUP = std::unique_ptr<Base>;

    using IntType = std::variant<int8_t, int16_t, int32_t>;
    
    template<typename SizeInt>
    struct Int : public Base
    {
        SizeInt value;

        Int();
        Int(SizeInt value);
    };

    using UIntType = std::variant<uint8_t, uint16_t, uint32_t>;

    template<typename SizeUInt>
    struct UInt : public Base
    {
        SizeUInt value;

        UInt();
        UInt(SizeUInt value);
    };

    using DecType = std::variant<float, double>;

    template<typename SizeDec>
    struct Dec : public Base
    {
        SizeDec value;

        Dec();
        Dec(SizeDec value);
    };

    struct Bool : public Base
    {
        bool value;

        Bool() = default;
        Bool(bool value);
    };

    struct String : public Base
    {
        std::string value;

        String();
        String(std::string& value);
    };

    struct Null : public Base
    {
        Null();
    };
};