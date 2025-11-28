#include "../include/object.h"
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
using namespace Object;

// Base.

Base::Base() :
    type(OBJ_INVALID), size(0) {}

Base::Base(ObjType type, size_t size) :
    type(type), size(size) {}

std::string Base::printType()
{
    return "OBJECT";
}

void Base::emit(std::ofstream& os)
{
    os.put(static_cast<char>(OBJ_BASE));
}

// Int and Dec covered in the header file since they use templates.

// Bool.

Bool::Bool(bool value) :
    Base(OBJ_BOOL, sizeof(bool)), value(value) {}

std::string Bool::print()
{
    return (this->value ? "true" : "false");
}

std::string Bool::printType()
{
    return "BOOL";
}

// String.

String::String() :
    Base(OBJ_STRING, sizeof(std::string_view)), value("") {} // Default initialize to empty string.

String::String(std::string_view& value) :
    Base(OBJ_STRING, sizeof(std::string_view)), value(value) {}

String String::makeString(const std::string& value)
{
    String str;
    str.alt = value;
    return str;
}

std::string String::print()
{
    return std::string(this->value);
}

std::string String::printType()
{
    return "STRING";
}

void String::emit(std::ofstream& os)
{
    os.put(static_cast<char>(OBJ_STRING));
    os.write(value.data(), value.size());
    os.put('\0');
}

// Null.
Null::Null() :
    Base(OBJ_NULL, sizeof(Base)) {}

std::string Null::print()
{
    return "null";
}

std::string Null::printType()
{
    return "NULL";
}