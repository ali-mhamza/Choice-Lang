#include "../include/error.h"
#include "../include/common.h"
#include <string>
#include <string_view>

// RuntimeError.

RuntimeError::RuntimeError(DiagCode code, const std::string& label) :
    Error{code, label} {}

// TypeMismatch.

TypeMismatch
reportBinaryOperator(Opcode oper, const Object& first, const Object& second)
{
    std::string_view sv{};
    DiagCode code{BINARY_OP_FAIL};
    auto firstType{first.printType()};
    auto secondType{second.printType()};

    switch (oper)
    {
        case OP_ADD:    sv = "add";         break;
        case OP_MULT:   sv = "multiply";    break;
        case OP_AND:    sv = "bitwise-and"; break;
        case OP_OR:     sv = "bitwise-or";  break;
        case OP_XOR:    sv = "bitwise-xor"; break;

        // Shared case.

        case OP_GT:     case OP_LT:
            sv = "compare";
            break;

        // Special operators.

        case OP_SUB:
        {
            return { code, CH_STR("cannot subtract ({}) from ({})", secondType,
                firstType) };
        }
        case OP_DIV:
        {
            return { code, CH_STR("cannot divide ({}) by ({})", firstType,
                secondType) };
        }
        case OP_MOD:
        {
            return { code, CH_STR("cannot take modulus of ({}) with base ({})",
                firstType, secondType)};
        }
        case OP_POWER:
        {
            return { code, CH_STR("cannot raise ({}) to power of ({})",
                firstType, secondType)};
        }
        case OP_SHIFT_L:    case OP_SHIFT_R:
        {
            return { code, CH_STR("cannot bitwise shift ({}) by ({})", firstType,
                secondType) };
        }
        case OP_RANGE:
        {
            return { code, CH_STR("cannot construct range from ({}) and ({})",
                firstType, secondType) };
        }
        default: CH_UNREACHABLE();
    }

    return { code, CH_STR("cannot {} ({}) and ({})", sv, firstType,
        secondType) };
}

TypeMismatch
reportUnaryOperator(Opcode oper, const Object& obj)
{
    std::string_view sv{};
    DiagCode code{UNARY_OP_FAIL};
    auto type{obj.printType()};

    switch (oper)
    {
        case OP_INCR:   sv = "increment";           break;
        case OP_DECR:   sv = "decrement";           break;
        case OP_NEG:    sv = "negate";              break;
        case OP_COMP:   sv = "bitwise complement";  break;
        case OP_NOT:
            return { code, CH_STR("cannot apply logical NOT to ({})", type) };
        default: CH_UNREACHABLE();
    }

    return { code, CH_STR("cannot {} ({})", sv, type) };
}

TypeMismatch
reportCollection(DiagCode code, const Object& first, const Object& second)
{
    auto firstType{first.printType()};
    auto secondType{IS_VALID(second) ? second.printType() : ""};

    switch (code)
    {
        case OBJ_NOT_COLLECTION:
            return { code, CH_STR("({}) cannot be indexed into", firstType) };
        case OBJ_NOT_INDEX:
        {
            return { code, CH_STR("({}) cannot be used as an index for ({})",
                secondType, firstType) };
        }
        case OBJ_NOT_ITERABLE:
            return { code, CH_STR("({}) is not an iterable type", firstType)};
        case OBJ_WRONG_ITER_TYPE:
        {
            return { code, CH_STR("({}) does not match the member type of ({})",
                firstType, secondType) };
        }
        default: CH_UNREACHABLE();
    }
}