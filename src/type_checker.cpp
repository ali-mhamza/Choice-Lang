#include "../include/type_checker.h"
#include "../include/core.h"
#include "../include/utils.h"
#include <algorithm>
#include <functional>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <variant>

#if CH_TYPE_CHECKING_ON

using namespace AST::Statement;
using namespace AST::Expression;

#define CHECK(type)                                 \
    do {                                            \
        auto* ptr{static_cast<type*>(node.get())};  \
        check##type(ptr);                           \
    } while (false)

#define CHECK_OPERATOR(type)                                                \
    do {                                                                    \
        checkExpr(node->left);                                              \
        checkExpr(node->right);                                             \
        if ((node->left == nullptr) || (node->right == nullptr)) return;    \
        Type leftType{getExprType(node->left)};                             \
        Type rightType{getExprType(node->right)};                           \
        if (!canApply##type##Operator(node->oper, leftType, rightType))     \
            reportTypeError(ErrorCode::OperatorMismatch);                   \
    } while (false)

#define GET_TYPE(type)                              \
    do {                                            \
        auto* ptr{static_cast<type*>(node.get())};  \
        return get##type##Type(ptr);                \
    } while (false)

#define CHECKER(type) \
    void TypeChecker::check##type(const type* node)
#define TYPE_GETTER(type) \
    TypeChecker::Type TypeChecker::get##type##Type(const type* node) const

#define BUILTIN_TYPE(type)  Type{TypeTag::Basic, BasicType{true, ObjType::type}}
#define ANY_TYPE            Type{TypeTag::Any}
#define DUMMY_TYPE          Type{}

#define PASS (void) node
#define RETURN return DUMMY_TYPE

/* Plain data. */

static std::unordered_map<std::string_view, ObjType> typeTokens{
    {"Int",     ObjType::Int},
    {"Dec",     ObjType::Dec},
    {"Bool",    ObjType::Bool},
    {"Null",    ObjType::Null},
    {"Void",    ObjType::Void},
    {"Module",  ObjType::Module},
    {"Text",    ObjType::Text},
    {"String",  ObjType::String},
    {"Range",   ObjType::Range},
    {"List",    ObjType::List},
    {"Table",   ObjType::Table}
};

std::vector<TypeChecker::TypeError> TypeChecker::errors{};

/* Helper data structures. */

bool TypeChecker::BasicType::operator==(const BasicType& other) const
{
    return ((this->builtin == other.builtin)
            && (this->type == other.type));
}

bool TypeChecker::TypeList::operator==(const TypeList& other) const
{
    u64 thisCount{this->types.size()};
    u64 otherCount{other.types.size()};

    if (thisCount != otherCount) return false;

    for (u64 i{0}; i < thisCount; i++)
    {
        if (*(this->types[i]) != *(other.types[i]))
            return false;
    }

    return true;
}

bool TypeChecker::Signature::operator==(const Signature& other) const
{
    return ((this->paramTypes == other.paramTypes)
            && (*(this->returnType) == *(other.returnType)));
}

bool TypeChecker::Generic::operator==(const Generic& other) const
{
    return ((*(this->baseType) == *(other.baseType))
            && (this->specTypes == other.specTypes));
}

bool TypeChecker::InnerType::operator==(const InnerType& other) const
{
    return (*(this->type) == *(other.type));
}

void TypeChecker::TypeList::add(const Type& type)
{
    types.push_back(std::make_shared<Type>(type));
}

bool TypeChecker::Type::operator==(const Type& other) const
{
    return ((this->tag == other.tag)
            && (this->type == other.type));
}

bool TypeChecker::Type::operator!=(const Type& other) const
{
    return !(*this == other);
}

bool TypeChecker::Type::isAny() const
{
    return (tag == TypeTag::Any);
}

bool TypeChecker::Type::isBasic() const
{
    return (tag == TypeTag::Basic);
}

template<typename... Types>
bool TypeChecker::Type::isBasicWithTypes(Types... types) const
{
    static_assert((std::is_same_v<ObjType, Types> || ...));

    if (tag == TypeTag::Any) return true;
    if (tag != TypeTag::Basic) return false;
    BasicType basic{std::get<BasicType>(type)};
    if (!basic.builtin) return false;

    ObjType objType{std::get<ObjType>(basic.type)};
    for (auto type : {types...})
    {
        if (type == objType)
            return true;
    }

    return false;
}

void TypeChecker::TypeError::print() const
{
    #define CASE(code) case ErrorCode::code: CH_PRINT(#code "\n"); break;

    switch (code)
    {
        CASE(Dummy);
        CASE(InitMismatch);
        CASE(AssignMismatch);
        CASE(IterMismatch);
        CASE(OperatorMismatch);
        CASE(TypeMismatch);
        CASE(ArgMismatch);
        CASE(ReturnMismatch);
        CASE(FieldMismatch);
        CASE(InstanceMismatch);
    }

    #undef CASE
}

/* Type-checking helpers. */

template<typename RecordT>
const RecordT* TypeChecker::getRecord(const std::string& name) const
{
    const auto* records = std::invoke([&] {
        if constexpr (std::is_same_v<RecordT, VarRecord>)
            return &(this->varRecords);
        else if constexpr (std::is_same_v<RecordT, FuncRecord>)
            return &(this->funcRecords);
        else
            return &(this->typeRecords);
    });

    for (u8 i{0}; i <= scope; i++)
    {
        VarEntry entry{ name, static_cast<u8>(scope - i) };
        const RecordT* record{records->get(entry)};
        if (record != nullptr)
            return record;
    }

    if (scopeChecker != nullptr)
        return scopeChecker->getRecord<RecordT>(name);

    return nullptr;
}

bool TypeChecker::compatibleTypes(const Type& t1, const Type& t2) const
{
    if ((t1.tag == TypeTag::Dummy) || (t2.tag == TypeTag::Dummy))
        return false;
    if (t1.tag == TypeTag::Any)
        return true;

    // Special handling for optional types.
    if (t1.tag == TypeTag::Option)
    {
        const auto& list{std::get<TypeList>(t1.type)};
        for (const auto& typeEntry : list.types)
        {
            if (compatibleTypes(*typeEntry, t2))
                return true;
        }

        return false;
    }
    else if (t1.tag == TypeTag::Nullable)
    {
        const InnerType& inner{std::get<InnerType>(t1.type)};
        const Type& type{*(inner.type)};
        return (
            t2.isBasicWithTypes(ObjType::Null) || compatibleTypes(type, t2)
        );
    }
    else if (t1.tag != t2.tag)
        return false;

    return std::visit([&](auto&& t1, auto&& t2) -> bool {
        using T1 = std::decay_t<decltype(t1)>;
        using T2 = std::decay_t<decltype(t2)>;

        if constexpr (std::is_same_v<T1, T2>)
            return t1 == t2;
        else
            return false;
    }, t1.type, t2.type);
}

bool TypeChecker::canApplyLogicOperator(
    TokenType oper,
    const Type& t1,
    const Type& t2
) const
{
    (void) oper; (void) t1; (void) t2;

    // Any value type can be evaluated as truthy or falsy.
    return true;
}

bool TypeChecker::canApplyCompareOperator(
    TokenType oper,
    const Type& t1,
    const Type& t2
) const
{
    switch (oper)
    {
        case TOK_EQ_EQ:
        case TOK_BANG_EQ:
        {
            return true;
        }
        case TOK_GT:
        case TOK_GT_EQ:
        case TOK_LT:
        case TOK_LT_EQ:
        {
            return (
                t1.isBasicWithTypes(ObjType::Int, ObjType::String)
                && t2.isBasicWithTypes(ObjType::Int, ObjType::String)
            );
        }
        case TOK_IN:
        {
            bool stringLike{
                t1.isBasicWithTypes(ObjType::Text, ObjType::String)
                && t2.isBasicWithTypes(ObjType::Text, ObjType::String)
            };
            bool range{
                t1.isBasicWithTypes(ObjType::Int)
                && t2.isBasicWithTypes(ObjType::Range)
            };
            bool collections{
                t2.isBasicWithTypes(ObjType::List, ObjType::Table)
            };

            return (stringLike || range || collections);
        }
        default: CH_UNREACHABLE();
    }
}

bool TypeChecker::canApplyBitOperator(
    TokenType oper,
    const Type& t1,
    const Type& t2
) const
{
    switch (oper)
    {
        case TOK_AMP:
        case TOK_BAR:
        case TOK_UARROW:
        {
            return (
                t1.isBasicWithTypes(ObjType::Int)
                && t2.isBasicWithTypes(ObjType::Int)
            );
        }
        default: CH_UNREACHABLE();
    }
}

bool TypeChecker::canApplyShiftOperator(
    TokenType oper,
    const Type& t1,
    const Type& t2
) const
{
    switch (oper)
    {
        case TOK_LEFT_SHIFT:
        case TOK_RIGHT_SHIFT:
        {
            return (
                t1.isBasicWithTypes(ObjType::Int)
                && t2.isBasicWithTypes(ObjType::Int)
            );
        }
        default: CH_UNREACHABLE();
    }
}

bool TypeChecker::canApplyBinaryOperator(
    TokenType oper,
    const Type& t1,
    const Type& t2
) const
{
    switch (oper)
    {
        case TOK_PLUS:
        {
            bool numeric{
                t1.isBasicWithTypes(ObjType::Int, ObjType::Dec)
                && t2.isBasicWithTypes(ObjType::Int, ObjType::Dec)
            };
            bool strings{
                t1.isBasicWithTypes(ObjType::Text, ObjType::String)
                && t2.isBasicWithTypes(ObjType::Text, ObjType::String)
            };

            return (numeric || strings);
        }
        case TOK_MINUS:
        case TOK_STAR:
        case TOK_SLASH:
        case TOK_PERCENT:
        case TOK_STAR_STAR:
        case TOK_DOT_DOT:
        {
            return (
                t1.isBasicWithTypes(ObjType::Int)
                && t2.isBasicWithTypes(ObjType::Int)
            );
        }
        default: CH_UNREACHABLE();
    }
}

bool TypeChecker::canApplyUnaryOperator(TokenType oper, const Type& type) const
{
    switch (oper)
    {
        case TOK_BANG:
        {
            return true;
        }
        case TOK_INCR:
        case TOK_DECR:
        case TOK_MINUS:
        case TOK_TILDE:
        {
            return type.isBasicWithTypes(ObjType::Int);
        }
        default: CH_UNREACHABLE();
    }
}

bool TypeChecker::canIndexInto(const Type& type) const
{
    return type.isBasicWithTypes(
        ObjType::Text,
        ObjType::String,
        ObjType::Range,
        ObjType::List,
        ObjType::Table
    );
}

bool TypeChecker::canIndexWith(const Type& objType, const Type& indexType) const
{
    if (objType.isAny()) return true;
    if (!objType.isBasic()) return false;
    BasicType basic{std::get<BasicType>(objType.type)};
    if (!basic.builtin) return false;

    ObjType obj{std::get<ObjType>(basic.type)};
    switch (obj)
    {
        case ObjType::Text:
        case ObjType::String:
        case ObjType::Range:
        case ObjType::List:
            return indexType.isBasicWithTypes(ObjType::Int);
        case ObjType::Table:
            return true;
        default:
            return false;
    }
}

bool TypeChecker::canCall(const Type& type) const
{
    // Function object.
    if (type.isAny() || (type.tag == TypeTag::Signature))
        return true;

    // Custom type (constructor call).
    if (type.isBasic() && !std::get<BasicType>(type.type).builtin)
        return true;

    return false;
}

bool TypeChecker::validArgsForObj(const CallExpr* node, const VarRecord& record) const
{
    if (record.type.tag != TypeTag::Signature) return false;
    const Signature& signature{std::get<Signature>(record.type.type)};

    if (node->args.size() != signature.paramTypes.size())
        return false;

    u8 argCount{static_cast<u8>(node->args.size())};
    for (u8 i{0}; i < argCount; i++)
    {
        Type argType{getExprType(node->args[i])};
        if (!compatibleTypes(argType, *(signature.paramTypes[i])))
            return false;
    }

    return true;
}

bool TypeChecker::validArgsForFunc(const CallExpr* node, const FuncRecord& record) const
{
    if (node->args.size() != record.paramTypes.size())
        return false;

    u8 argCount{static_cast<u8>(node->args.size())};
    for (u8 i{0}; i < argCount; i++)
    {
        Type argType{getExprType(node->args[i])};
        if (!compatibleTypes(argType, record.paramTypes[i]))
            return false;
    }

    return true;
}

bool TypeChecker::validArgsForCtor(const CallExpr* node, const TypeRecord& record) const
{
    const auto& methods{record.methods};
    auto it{std::find(methods.begin(), methods.end(), CH_CONSTRUCTOR)};
    if (it != methods.end())
    {
        auto pos{it - methods.begin()};
        return validArgsForFunc(node, record.methodRecords[pos]);
    }

    if (node->args.size() != 0)
    {
        if (node->args.size() != record.fields.size())
            return false;

        u8 argCount{static_cast<u8>(node->args.size())};
        for (u8 i{0}; i < argCount; i++)
        {
            Type argType{getExprType(node->args[i])};
            if (!compatibleTypes(argType, record.fieldTypes[i]))
                return false;
        }
    }

    return true;
}

bool TypeChecker::validArgsForCallable(const CallExpr* node) const
{
    if (node->builtin) return true; // For now.

    const VarExpr* var{static_cast<const VarExpr*>(node->callee.get())};
    std::string name{var->name.text};

    const VarRecord* varRecord{getRecord<VarRecord>(name)};
    if (varRecord != nullptr) return validArgsForObj(node, *varRecord);

    const FuncRecord* funcRecord{getRecord<FuncRecord>(name)};
    if (funcRecord != nullptr) return validArgsForFunc(node, *funcRecord);

    const TypeRecord* typeRecord{getRecord<TypeRecord>(name)};
    if (typeRecord != nullptr) return validArgsForCtor(node, *typeRecord);

    return false;
}

bool TypeChecker::validFieldForType(const Type& type, const Token& field) const
{
    if (type.isAny()) return true;
    if (!type.isBasic()) return false;

    BasicType basic{std::get<BasicType>(type.type)};
    if (basic.builtin) return false; // For now.

    const auto& typeName{std::get<std::string>(basic.type)};
    const TypeRecord* record{getRecord<TypeRecord>(typeName)};
    if (record == nullptr) return false;

    for (const auto& fieldEntry : record->fields)
    {
        if (fieldEntry == field.text)
            return true;
    }

    return false;
}

/* Type derivation/evaluation. */

TypeChecker::Type TypeChecker::typeFromHint(
    const AST::Types::TypeHint& typeHint
) const
{
    using namespace AST;
    using namespace AST::Types;

    switch (typeHint.tag)
    {
        case HintType::Dummy:
        {
            return DUMMY_TYPE;
        }
        case HintType::Any:
        {
            return ANY_TYPE;
        }
        case HintType::Name:
        {
            auto typeToken{std::get<Token>(typeHint.hint)};
            if (typeToken.text == "Any") return ANY_TYPE;

            auto* check{getRecord<VarRecord>(std::string{typeToken.text})};
            if (check != nullptr) return check->type;

            auto it{typeTokens.find(typeToken.text)};
            BasicType type{};
            if (it != typeTokens.end())
                type = BasicType{ true, it->second };
            else
                type = BasicType{ false, std::string{typeToken.text} };

            return { TypeTag::Basic, type };
        }
        case HintType::Signature:
        {
            const auto& signature{std::get<Types::Signature>(typeHint.hint)};
            TypeList paramTypes{};
            for (const auto& type : signature.paramTypes)
                paramTypes.add(typeFromHint(*type));
            Type returnType{typeFromHint(*(signature.returnType))};
            Signature type{paramTypes, std::make_shared<Type>(returnType)};

            return { TypeTag::Signature, type };
        }
        case HintType::Option:
        case HintType::Group:
        {
            const auto& typeVec{std::get<Types::TypeVec>(typeHint.hint)};
            TypeList types{};
            for (const auto& type : typeVec)
                types.add(typeFromHint(*type));

            if (typeHint.tag == HintType::Option)
                return { TypeTag::Option, types };
            return { TypeTag::Group, types };
        }
        case HintType::Generic:
        {
            const auto& collection{std::get<Types::Generic>(typeHint.hint)};
            Type base{typeFromHint(*(collection.baseType))};
            TypeList specTypes{};
            for (const auto& type : collection.specTypes)
                specTypes.add(typeFromHint(*type));

            Generic type{ std::make_shared<Type>(base), specTypes };
            return { TypeTag::Generic, type };
        }
        case HintType::Reference:
        case HintType::Nullable:
        {
            const auto& innerType{std::get<Types::TypeUP>(typeHint.hint)};
            TypeSP type{std::make_shared<Type>(typeFromHint(*innerType))};

            if (typeHint.tag == HintType::Reference)
                return { TypeTag::Reference, InnerType{ type } };
            return { TypeTag::Nullable, InnerType{ type } };
        }
    }

    CH_UNREACHABLE();
}

TypeChecker::Type TypeChecker::signatureFromFuncRecord(
    const FuncRecord& record
) const
{
    Signature signature{};

    for (const auto& type : record.paramTypes)
        signature.paramTypes.add(type);
    signature.returnType = std::make_shared<Type>(record.returnType);

    return Type{TypeTag::Signature, signature};
}

TypeChecker::Type TypeChecker::getElementType(const Type& type) const
{
    if (type.isAny()) return ANY_TYPE;
    if (!type.isBasic()) return DUMMY_TYPE; // For now.

    BasicType basic{std::get<BasicType>(type.type)};
    if (!basic.builtin) return DUMMY_TYPE;

    switch (std::get<ObjType>(basic.type))
    {
        case ObjType::Text:     return BUILTIN_TYPE(Text);
        case ObjType::String:   return BUILTIN_TYPE(String);
        case ObjType::Range:    return BUILTIN_TYPE(Int);
        case ObjType::List:     return ANY_TYPE;
        // Returns a List, since this function is called to
        // type-check loop variables.
        case ObjType::Table:    return BUILTIN_TYPE(List);
        default:                return DUMMY_TYPE;
    }
}

TypeChecker::Type TypeChecker::getExprType(const ExprUP& node) const
{
    if (node == nullptr) return DUMMY_TYPE;

    switch (node->type)
    {
        case ExprType::MutExpr:         GET_TYPE(MutExpr);
        case ExprType::AssignExpr:      GET_TYPE(AssignExpr);
        case ExprType::LogicExpr:       GET_TYPE(LogicExpr);
        case ExprType::CompareExpr:     GET_TYPE(CompareExpr);
        case ExprType::BitExpr:         GET_TYPE(BitExpr);
        case ExprType::ShiftExpr:       GET_TYPE(ShiftExpr);
        case ExprType::BinaryExpr:      GET_TYPE(BinaryExpr);
        case ExprType::UnaryExpr:       GET_TYPE(UnaryExpr);
        case ExprType::IndexExpr:       GET_TYPE(IndexExpr);
        case ExprType::CallExpr:        GET_TYPE(CallExpr);
        case ExprType::FieldExpr:       GET_TYPE(FieldExpr);
        case ExprType::ScopeExpr:       GET_TYPE(ScopeExpr);
        case ExprType::IfExpr:          GET_TYPE(IfExpr);
        case ExprType::LambdaExpr:      GET_TYPE(LambdaExpr);
        case ExprType::ListExpr:        GET_TYPE(ListExpr);
        case ExprType::TableExpr:       GET_TYPE(TableExpr);
        case ExprType::InstanceExpr:    GET_TYPE(InstanceExpr);
        case ExprType::ListCompExpr:    GET_TYPE(ListCompExpr);
        case ExprType::TableCompExpr:   GET_TYPE(TableCompExpr);
        case ExprType::RefExpr:         GET_TYPE(RefExpr);
        case ExprType::VarExpr:         GET_TYPE(VarExpr);
        case ExprType::StringPartExpr:  GET_TYPE(StringPartExpr);
        case ExprType::FormatExpr:      GET_TYPE(FormatExpr);
        case ExprType::LiteralExpr:     GET_TYPE(LiteralExpr);
    }

    CH_UNREACHABLE();
}

TYPE_GETTER(MutExpr)
{
    return getExprType(node->value);
}

TYPE_GETTER(AssignExpr)
{
    // This function can only be called if the assignment
    // expression is nested within another expression, which
    // can only occur with single-variable assignment.
    // Thus, we check the first (and only) RHS assignment value.
    return getExprType(node->values.front());
}

TYPE_GETTER(LogicExpr)
{
    PASS;
    return BUILTIN_TYPE(Bool);
}

TYPE_GETTER(CompareExpr)
{
    PASS;
    return BUILTIN_TYPE(Bool);
}

TYPE_GETTER(BitExpr)
{
    PASS;
    return BUILTIN_TYPE(Int);
}

TYPE_GETTER(ShiftExpr)
{
    PASS;
    return BUILTIN_TYPE(Int);
}

TYPE_GETTER(BinaryExpr)
{
    switch (node->oper)
    {
        case TOK_PLUS:
        {
            Type t1{getExprType(node->left)};
            Type t2{getExprType(node->right)};

            bool integers{
                t1.isBasicWithTypes(ObjType::Int)
                && t2.isBasicWithTypes(ObjType::Int)
            };
            bool numeric{
                t1.isBasicWithTypes(ObjType::Int, ObjType::Dec)
                && t2.isBasicWithTypes(ObjType::Int, ObjType::Dec)
            };
            bool strings{
                t1.isBasicWithTypes(ObjType::Text, ObjType::String)
                && t2.isBasicWithTypes(ObjType::Text, ObjType::String)
            };

            if (integers) return BUILTIN_TYPE(Int);
            else if (numeric) return BUILTIN_TYPE(Dec);
            else if (strings) return BUILTIN_TYPE(String);

            return DUMMY_TYPE;
        }
        case TOK_MINUS:
        case TOK_STAR:
        case TOK_SLASH:
        case TOK_PERCENT:
        case TOK_STAR_STAR:
        {
            return BUILTIN_TYPE(Int);
        }
        case TOK_DOT_DOT:
        {
            return BUILTIN_TYPE(Range);
        }
        default: CH_UNREACHABLE();
    }
}

TYPE_GETTER(UnaryExpr)
{
    switch (node->oper.type)
    {
        case TOK_BANG:
        {
            return BUILTIN_TYPE(Bool);
        }
        case TOK_INCR:
        case TOK_DECR:
        case TOK_MINUS:
        case TOK_TILDE:
        {
            return BUILTIN_TYPE(Int);
        }
        default: CH_UNREACHABLE();
    }
}

TYPE_GETTER(IndexExpr)
{
    Type objType{getExprType(node->obj)};
    return getElementType(objType);
}

TYPE_GETTER(CallExpr)
{
    if (node->builtin || (node->callee == nullptr)
        || (node->callee->type != ExprType::VarExpr))
    {
        return DUMMY_TYPE;
    }

    const VarExpr* var{static_cast<const VarExpr*>(node->callee.get())};
    std::string name{var->name.text};

    const VarRecord* varRecord{getRecord<VarRecord>(name)};
    if (varRecord != nullptr)
    {
        if (varRecord->type.tag != TypeTag::Signature)
            return DUMMY_TYPE;
        const Signature& signature{std::get<Signature>(varRecord->type.type)};
        return *(signature.returnType);
    }

    const FuncRecord* funcRecord{getRecord<FuncRecord>(name)};
    if (funcRecord != nullptr) return funcRecord->returnType;

    const TypeRecord* typeRecord{getRecord<TypeRecord>(name)};
    if (typeRecord != nullptr)
    {
        return Type{
            TypeTag::Basic,
            BasicType{false, name}
        };
    }

    return DUMMY_TYPE;
}

TYPE_GETTER(FieldExpr)
{
    Type objType{getExprType(node->obj)};
    if (objType.isAny()) return ANY_TYPE;

    // For now, only handling singular types.
    if (!objType.isBasic()) return DUMMY_TYPE;
    BasicType basic{std::get<BasicType>(objType.type)};
    if (basic.builtin) return DUMMY_TYPE;

    const auto& typeName{std::get<std::string>(basic.type)};
    const TypeRecord* record{getRecord<TypeRecord>(typeName)};
    if (record == nullptr) return DUMMY_TYPE;

    const auto& fields{record->fields};
    auto fieldIt{std::find(fields.begin(), fields.end(), node->field.text)};
    if (fieldIt != fields.end())
    {
        auto pos{fieldIt - fields.begin()};
        return record->fieldTypes[pos];
    }

    // TODO: remove method look-up when field expression is
    // an assignment target.

    const auto& methods{record->methods};
    auto methodIt{std::find(methods.begin(), methods.end(), node->field.text)};
    if (methodIt != methods.end())
    {
        auto pos{methodIt - methods.begin()};
        return signatureFromFuncRecord(record->methodRecords[pos]);
    }

    return DUMMY_TYPE;
}

TYPE_GETTER(ScopeExpr)
{
    PASS;
    return ANY_TYPE;
}

TYPE_GETTER(IfExpr)
{
    Type trueType{getExprType(node->trueExpr)};
    Type falseType{getExprType(node->falseExpr)};

    TypeList list{};
    list.add(trueType);
    list.add(falseType);

    return Type{ TypeTag::Option, list };
}

TYPE_GETTER(LambdaExpr)
{
    TypeList paramTypes{};
    for (const auto& paramEntry : node->params)
        paramTypes.add(typeFromHint(paramEntry.param.typeHint));

    Type returnType{typeFromHint(node->typeHint)};
    return Type{
        TypeTag::Signature,
        Signature{ paramTypes, std::make_shared<Type>(returnType) }
    };
}

TYPE_GETTER(ListExpr)
{
    PASS;
    return BUILTIN_TYPE(List);
}

TYPE_GETTER(TableExpr)
{
    PASS;
    return BUILTIN_TYPE(Table);
}

TYPE_GETTER(InstanceExpr)
{
    // For now.
    if (node->type->type != ExprType::VarExpr)
        return DUMMY_TYPE;

    const VarExpr* var{static_cast<const VarExpr*>(node->type.get())};
    std::string name{var->name.text};
    return Type{ TypeTag::Basic, BasicType{false, name}};
}

TYPE_GETTER(ListCompExpr)
{
    PASS;
    return BUILTIN_TYPE(List);
}

TYPE_GETTER(TableCompExpr)
{
    PASS;
    return BUILTIN_TYPE(Table);
}

TYPE_GETTER(RefExpr)
{
    return Type{
        TypeTag::Reference,
        std::make_shared<Type>(getExprType(node->obj))
    };
}

TYPE_GETTER(VarExpr)
{
    std::string name{node->name.text};

    const VarRecord* varRecord{getRecord<VarRecord>(name)};
    if (varRecord != nullptr) return varRecord->type;

    const FuncRecord* funcRecord{getRecord<FuncRecord>(name)};
    if (funcRecord != nullptr) return signatureFromFuncRecord(*funcRecord);

    const TypeRecord* typeRecord{getRecord<TypeRecord>(name)};
    if (typeRecord != nullptr)
    {
        // Construct a "meta-type" holding this particular type.
        Type baseType{ TypeTag::Basic, BasicType{ false, "Type" } };
        TypeList specTypes;
        specTypes.add({ TypeTag::Basic, BasicType{ false, name } });

        Generic generic{
            std::make_shared<Type>(baseType),
            specTypes
        };
        return Type{TypeTag::Generic, generic};
    }

    return DUMMY_TYPE;
}

TYPE_GETTER(StringPartExpr) { PASS; RETURN; }
TYPE_GETTER(FormatExpr)     { PASS; RETURN; }

TYPE_GETTER(LiteralExpr)
{
    switch (node->value.type)
    {
        case TOK_NUM:       return BUILTIN_TYPE(Int);
        case TOK_NUM_DEC:
        {
            bool approximatelyInt{fmod(node->value.content.d, 1.0) == 0.0};
            return (approximatelyInt ? BUILTIN_TYPE(Int) : BUILTIN_TYPE(Dec));
        }
        case TOK_STR_LIT:   return BUILTIN_TYPE(Text);
        case TOK_RAW_STR:   return BUILTIN_TYPE(Text);
        case TOK_TRUE:      return BUILTIN_TYPE(Bool);
        case TOK_FALSE:     return BUILTIN_TYPE(Bool);
        case TOK_NULL:      return BUILTIN_TYPE(Null);
        default: CH_UNREACHABLE();
    }
}

void TypeChecker::reportTypeError(ErrorCode code)
{
    errors.push_back(TypeError{ code });
}

/* Type-checking functions. */

TypeChecker::FuncRecord TypeChecker::makeFuncRecord(const FuncDecl* func) const
{
    std::vector<Type> paramTypes{};
    for (const auto& paramEntry : func->params)
        paramTypes.push_back(typeFromHint(paramEntry.param.typeHint));
    Type returnType{typeFromHint(func->typeHint)};

    return FuncRecord{ paramTypes, returnType };
}

template<typename NodeT>
void TypeChecker::checkFuncBody(const NodeT* node)
{
    TypeChecker miniChecker{*this};

    for (const auto& paramEntry : node->params)
    {
        Type paramType{typeFromHint(paramEntry.param.typeHint)};
        miniChecker.varRecords.add(
            VarEntry{ paramEntry.param.var.text, 0 },
            VarRecord{ paramType }
        );
    }

    miniChecker.checkStmt(node->body);
}

void TypeChecker::checkLoopHeader(const AST::LoopHeader& header)
{
    checkExpr(header.iter);
    checkExpr(header.where);

    // For now.
    if (header.vars.size() == 1)
    {
        Type varType{typeFromHint(header.vars.front().typeHint)};
        Type iterType{getElementType(getExprType(header.iter))};

        if (!compatibleTypes(varType, iterType))
            reportTypeError(ErrorCode::IterMismatch);
    }
}

CHECKER(VarDecl)
{
    u64 nameCount{node->names.size()};
    u64 valueCount{node->values.size()};

    // For now.
    if (nameCount != valueCount) return;

    for (u64 i{0}; i < nameCount; i++)
    {
        const auto& name{node->names[i]};
        Type varType{typeFromHint(name.typeHint)};
        varRecords.add(
            VarEntry{ name.var.text, scope },
            VarRecord{ varType }
        );

        const auto& value{node->values[i]};
        if (value == nullptr) continue;

        checkExpr(value);
        Type valueType{getExprType(value)};
        if (!compatibleTypes(varType, valueType))
            reportTypeError(ErrorCode::InitMismatch);
    }
}

CHECKER(FuncDecl)
{
    auto record{makeFuncRecord(node)};
    auto returnType{currentReturnType};
    currentReturnType = record.returnType;

    funcRecords.add(
        VarEntry{ node->name.text, scope },
        record
    );

    for (const auto& paramEntry : node->params)
    {
        if (paramEntry.defaultVal == nullptr) continue;

        checkExpr(paramEntry.defaultVal);
        Type hintType{typeFromHint(paramEntry.param.typeHint)};
        Type initType{getExprType(paramEntry.defaultVal)};

        if (!compatibleTypes(hintType, initType))
            reportTypeError(ErrorCode::InitMismatch);
    }

    checkFuncBody(node);
    currentReturnType = returnType;
}

CHECKER(TypeDecl)
{
    auto unwrapField = [](const ExprUP& expr) -> const ExprUP& {
        LambdaExpr* lambda{static_cast<LambdaExpr*>(expr.get())};
        ReturnStmt* stmt{static_cast<ReturnStmt*>(lambda->body.get())};
        return stmt->expr;
    };

    std::vector<std::string> fields{};
    std::vector<Type> fieldTypes{};
    for (const auto& field : node->fields)
    {
        fields.push_back(std::string{field.name.var.text});
        fieldTypes.push_back(typeFromHint(field.name.typeHint));
    }

    std::vector<std::string> methods{};
    std::vector<FuncRecord> methodRecords{};
    for (const auto& method : node->methods)
    {
        const FuncDecl* decl{static_cast<const FuncDecl*>(method.get())};
        methods.push_back(std::string{decl->name.text});
        methodRecords.push_back(makeFuncRecord(decl));
    }

    typeRecords.add(
        VarEntry{ node->name.text, scope },
        TypeRecord{ fields, methods, fieldTypes, methodRecords }
    );

    for (const auto& field : node->fields)
    {
        checkExpr(field.init);
        Type fieldType{typeFromHint(field.name.typeHint)};
        Type initType{getExprType(unwrapField(field.init))};
        if (!compatibleTypes(fieldType, initType))
            reportTypeError(ErrorCode::InitMismatch);
    }
    for (const auto& method : node->methods)
        checkStmt(method);
}

CHECKER(AliasDecl)
{
    varRecords.add(
        VarEntry{ node->alias.text, scope },
        VarRecord{ typeFromHint(node->type) }
    );
}

CHECKER(UseStmt)
{
    if (node->entries.empty())
    {
        std::string name{
            node->alias ? node->alias.text : node->module.text
        };
        varRecords.add(
            VarEntry{ name, scope },
            VarRecord{ BUILTIN_TYPE(Module) }
        );
    }
    else
    {
        for (const auto& entry : node->entries)
        {
            std::string name{
                entry.alias ? entry.alias.text : entry.name.text
            };
            varRecords.add(
                VarEntry{ name, scope },
                VarRecord{ ANY_TYPE }
            );
        }
    }
}

CHECKER(IfStmt)
{
    checkExpr(node->condition);
    checkStmt(node->trueBranch);
    checkStmt(node->falseBranch);
}

CHECKER(WhileStmt)
{
    checkExpr(node->condition);
    checkStmt(node->body);
    checkStmt(node->elseClause);
}

CHECKER(ForStmt)
{
    checkLoopHeader(node->header);
    checkStmt(node->body);
    checkStmt(node->elseClause);
}

CHECKER(MatchStmt)
{
    checkExpr(node->matchValue);
    for (const auto& matchCase : node->cases)
    {
        checkExpr(matchCase.value);
        checkStmt(matchCase.body);
    }
}

CHECKER(RepeatStmt)
{
    checkExpr(node->condition);
    checkStmt(node->body);
}

CHECKER(ReturnStmt)
{
    checkExpr(node->expr);

    // Return value is present.
    if (node->expr != nullptr)
    {
        Type valType{getExprType(node->expr)};
        if (!compatibleTypes(currentReturnType, valType))
            reportTypeError(ErrorCode::ReturnMismatch);
    }
    else
    {
        if (!currentReturnType.isBasicWithTypes(ObjType::Void))
            reportTypeError(ErrorCode::ReturnMismatch);
    }
}

CHECKER(BreakStmt)      { PASS; }
CHECKER(ContinueStmt)   { PASS; }
CHECKER(EndStmt)        { PASS; }

CHECKER(ExprStmt)
{
    checkExpr(node->expr);
}

CHECKER(BlockStmt)
{
    for (const auto& stmt : node->block)
        checkStmt(stmt);
}

CHECKER(MutExpr)
{
    checkExpr(node->value);
}

CHECKER(AssignExpr)
{
    u64 targetCount{node->targets.size()};
    u64 valueCount{node->values.size()};

    // For now.
    if (targetCount == valueCount)
    {
        for (u64 i{0}; i < targetCount; i++)
        {
            const auto& target{node->targets[i]};
            const auto& value{node->values[i]};

            checkExpr(target);
            checkExpr(value);

            // So we don't report type errors on top of potential
            // parsing errors.
            if ((target == nullptr) || (value == nullptr)) continue;

            // 'target' must be an assignable expression (as guaranteed
            // by the parser), so we can get its type directly.
            Type targetType{getExprType(target)};
            Type valueType{getExprType(value)};
            if (!compatibleTypes(targetType, valueType))
                reportTypeError(ErrorCode::AssignMismatch);
        }
    }
}

CHECKER(LogicExpr)      { CHECK_OPERATOR(Logic); }
CHECKER(CompareExpr)    { CHECK_OPERATOR(Compare); }
CHECKER(BitExpr)        { CHECK_OPERATOR(Bit); }
CHECKER(ShiftExpr)      { CHECK_OPERATOR(Shift); }
CHECKER(BinaryExpr)     { CHECK_OPERATOR(Binary); }

CHECKER(UnaryExpr)
{
    checkExpr(node->expr);
    Type exprType{getExprType(node->expr)};
    if (!canApplyUnaryOperator(node->oper.type, exprType))
        reportTypeError(ErrorCode::OperatorMismatch);
}

CHECKER(IndexExpr)
{
    checkExpr(node->obj);
    checkExpr(node->index);

    Type objType{getExprType(node->obj)};
    Type indexType{getExprType(node->index)};
    if (!canIndexInto(objType) || !canIndexWith(objType, indexType))
        reportTypeError(ErrorCode::TypeMismatch);
}

CHECKER(CallExpr)
{
    checkExpr(node->callee);
    for (const auto& arg : node->args)
        checkExpr(arg);

    Type calleeType{getExprType(node->callee)};
    if (!canCall(calleeType))
    {
        reportTypeError(ErrorCode::TypeMismatch);
        return;
    }

    if ((node->callee != nullptr) && (node->callee->type == ExprType::VarExpr)
        && !validArgsForCallable(node))
    {
        reportTypeError(ErrorCode::ArgMismatch);
    }
}

CHECKER(FieldExpr)
{
    checkExpr(node->obj);

    Type type{getExprType(node->obj)};
    if (!validFieldForType(type, node->field))
        reportTypeError(ErrorCode::FieldMismatch);
}

CHECKER(ScopeExpr)
{
    Type moduleType{getExprType(node->module)};
    if (!moduleType.isBasicWithTypes(ObjType::Module))
        reportTypeError(ErrorCode::TypeMismatch);
}

CHECKER(IfExpr)
{
    checkExpr(node->condition);
    checkExpr(node->trueExpr);
    checkExpr(node->falseExpr);
}

CHECKER(LambdaExpr)
{
    auto returnType{currentReturnType};
    currentReturnType = typeFromHint(node->typeHint);

    for (const auto& paramEntry : node->params)
    {
        if (paramEntry.defaultVal == nullptr) continue;

        checkExpr(paramEntry.defaultVal);
        Type hintType{typeFromHint(paramEntry.param.typeHint)};
        Type initType{getExprType(paramEntry.defaultVal)};

        if (!compatibleTypes(hintType, initType))
            reportTypeError(ErrorCode::InitMismatch);
    }

    checkFuncBody(node);
    currentReturnType = returnType;
}

CHECKER(ListExpr)
{
    for (const auto& entry : node->entries)
        checkExpr(entry);
}

CHECKER(TableExpr)
{
    for (const auto& pair : node->pairs)
    {
        checkExpr(pair.key);
        checkExpr(pair.value);
    }
}

CHECKER(InstanceExpr)
{
    // For now.
    if (node->type->type != ExprType::VarExpr)
        return;

    const VarExpr* var{static_cast<const VarExpr*>(node->type.get())};
    std::string name{var->name.text};
    const TypeRecord* record{getRecord<TypeRecord>(name)};

    if (record == nullptr)
        reportTypeError(ErrorCode::InstanceMismatch);
    else
    {
        const auto& fields{record->fields};
        for (const auto& field : node->fields)
        {
            auto it{std::find(fields.begin(), fields.end(), field.name.text)};
            if (it != fields.end())
            {
                auto pos{it - fields.begin()};
                Type initType{getExprType(field.init)};
                if (!compatibleTypes(record->fieldTypes[pos], initType))
                    reportTypeError(ErrorCode::FieldMismatch);
                break;
            }
        }
    }
}

// TODO: Check loop expressions for comprehensions.
// Requires defining scoped loop variables.

CHECKER(ListCompExpr)
{
    checkLoopHeader(node->header);
}

CHECKER(TableCompExpr)
{
    checkLoopHeader(node->header);
}

CHECKER(RefExpr)
{
    checkExpr(node->obj);
}

CHECKER(VarExpr)        { PASS; }
CHECKER(StringPartExpr) { PASS; }

CHECKER(FormatExpr)
{
    for (const auto& part : node->parts)
        checkExpr(part);
}

CHECKER(LiteralExpr) { PASS; }

void TypeChecker::checkStmt(const StmtUP& node)
{
    if (node == nullptr) return;

    switch (node->type)
    {
        case StmtType::VarDecl:         CHECK(VarDecl);         break;
        case StmtType::FuncDecl:        CHECK(FuncDecl);        break;
        case StmtType::TypeDecl:        CHECK(TypeDecl);        break;
        case StmtType::AliasDecl:       CHECK(AliasDecl);       break;
        case StmtType::UseStmt:         CHECK(UseStmt);         break;
        case StmtType::IfStmt:          CHECK(IfStmt);          break;
        case StmtType::WhileStmt:       CHECK(WhileStmt);       break;
        case StmtType::ForStmt:         CHECK(ForStmt);         break;
        case StmtType::MatchStmt:       CHECK(MatchStmt);       break;
        case StmtType::RepeatStmt:      CHECK(RepeatStmt);      break;
        case StmtType::ReturnStmt:      CHECK(ReturnStmt);      break;
        case StmtType::BreakStmt:       CHECK(BreakStmt);       break;
        case StmtType::ContinueStmt:    CHECK(ContinueStmt);    break;
        case StmtType::EndStmt:         CHECK(EndStmt);         break;
        case StmtType::ExprStmt:        CHECK(ExprStmt);        break;
        case StmtType::BlockStmt:       CHECK(BlockStmt);       break;
    }
}

void TypeChecker::checkExpr(const ExprUP& node)
{
    if (node == nullptr) return;

    switch (node->type)
    {
        case ExprType::MutExpr:         CHECK(MutExpr);         break;
        case ExprType::AssignExpr:      CHECK(AssignExpr);      break;
        case ExprType::LogicExpr:       CHECK(LogicExpr);       break;
        case ExprType::CompareExpr:     CHECK(CompareExpr);     break;
        case ExprType::BitExpr:         CHECK(BitExpr);         break;
        case ExprType::ShiftExpr:       CHECK(ShiftExpr);       break;
        case ExprType::BinaryExpr:      CHECK(BinaryExpr);      break;
        case ExprType::UnaryExpr:       CHECK(UnaryExpr);       break;
        case ExprType::IndexExpr:       CHECK(IndexExpr);       break;
        case ExprType::CallExpr:        CHECK(CallExpr);        break;
        case ExprType::FieldExpr:       CHECK(FieldExpr);       break;
        case ExprType::ScopeExpr:       CHECK(ScopeExpr);       break;
        case ExprType::IfExpr:          CHECK(IfExpr);          break;
        case ExprType::LambdaExpr:      CHECK(LambdaExpr);      break;
        case ExprType::ListExpr:        CHECK(ListExpr);        break;
        case ExprType::TableExpr:       CHECK(TableExpr);       break;
        case ExprType::InstanceExpr:    CHECK(InstanceExpr);    break;
        case ExprType::ListCompExpr:    CHECK(ListCompExpr);    break;
        case ExprType::TableCompExpr:   CHECK(TableCompExpr);   break;
        case ExprType::RefExpr:         CHECK(RefExpr);         break;
        case ExprType::VarExpr:         CHECK(VarExpr);         break;
        case ExprType::StringPartExpr:  CHECK(StringPartExpr);  break;
        case ExprType::FormatExpr:      CHECK(FormatExpr);      break;
        case ExprType::LiteralExpr:     CHECK(LiteralExpr);     break;
    }
}

TypeChecker::TypeChecker(TypeChecker* checker) :
    scopeChecker{checker} {}

void TypeChecker::check(const StmtVec& program)
{
    errors.clear();

    for (const auto& stmt : program)
        checkStmt(stmt);

    if (!errors.empty())
    {
        CH_PRINT("Type errors:\n");
        for (const auto& error : errors)
            error.print();
    }
}

#undef CHECK
#undef CHECK_OPERATOR
#undef CHECK_FUNC_BODY
#undef GET_TYPE
#undef CHECKER
#undef TYPE_GETTER
#undef BUILTIN_TYPE
#undef ANY_TYPE
#undef DUMMY_TYPE
#undef PASS
#undef RETURN

#endif