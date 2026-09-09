#pragma once
#include "astnodes.h"
#include "object.h"
#include "token.h"
#include "vartable.h"
#include <personal/hash_table.h>
#include <memory>
#include <variant>
#include <vector>

#if CH_TYPE_CHECKING_ON

class TypeChecker
{
    #define CHECK_STMT(type)    void check##type(const AST::Statement::type* node)
    #define CHECK_EXPR(type)    void check##type(const AST::Expression::type* node)
    #define TYPE_GETTER(type)   Type get##type##Type(const AST::Expression::type* node) const

    private:
        enum class TypeTag : u8
        {
            Dummy,      // Empty type object.
            Any,        // Match to any type.
            Basic,      // Basic type.
            Signature,  // Function signature.
            Option,     // Mutually-exclusive set of types.
            Group,      // Group (list) of types.
            Generic,    // Generic type with specialization type(s).
            Reference,  // Reference to another type.
            Nullable    // Type with possible 'null' value.
        };

        struct Type;
        using TypeSP = std::shared_ptr<Type>;
        using TypeVec = std::vector<TypeSP>;

        struct BasicType
        {
            bool builtin{};
            std::variant<ObjType, std::string> type{};

            BasicType() = default;
            template<typename T>
            BasicType(bool builtin, T val) : builtin{builtin}, type{val} {}
            bool operator==(const BasicType& other) const;
        };

        struct TypeList
        {
            TypeVec types{};

            TypeList() = default;
            TypeList(TypeVec& types) : types{std::move(types)} {}
            bool operator==(const TypeList& other) const;

            void add(const Type& type);
            u64 size() const { return types.size(); }
            const TypeSP& operator[](u64 i) const { return types[i]; }
        };

        struct Signature
        {
            TypeList paramTypes{};
            TypeSP returnType{};

            Signature() = default;
            Signature(TypeList& paramTypes, TypeSP returnType) :
                paramTypes{std::move(paramTypes)},
                returnType{std::move(returnType)} {}
            bool operator==(const Signature& other) const;
        };

        struct Generic
        {
            TypeSP baseType{};
            // Specialization type(s).
            TypeList specTypes{};

            Generic() = default;
            Generic(TypeSP baseType, TypeList& specTypes) :
                baseType{std::move(baseType)}, specTypes{std::move(specTypes)} {}
            bool operator==(const Generic& other) const;
        };

        struct InnerType
        {
            TypeSP type{};

            InnerType() = default;
            InnerType(TypeSP type) : type{std::move(type)} {}
            bool operator==(const InnerType& other) const;
        };

        #undef X

        using TypeVariant = std::variant<
            std::monostate, BasicType, Signature, TypeList, Generic,
            InnerType
        >;

        struct Type
        {
            TypeTag tag{};
            TypeVariant variant{};

            bool operator==(const Type& other) const;
            bool operator!=(const Type& other) const;
            bool isAny() const;
            bool isBasic() const;
            template<typename... Types>
            bool isBasicWithTypes(Types... types) const;
        };

        enum class ErrorCode
        {
            Dummy,
            InitMismatch,
            AssignMismatch,
            IterMismatch,
            OperatorMismatch,
            TypeMismatch,
            ArgMismatch,
            ReturnMismatch,
            FieldMismatch,
            InstanceMismatch
        };

        struct TypeError
        {
            ErrorCode code{};

            void print() const;
        };

        struct VarRecord
        {
            Type type{};
        };

        struct FuncRecord
        {
            std::vector<Type> paramTypes{};
            Type returnType{};
        };

        struct TypeRecord
        {
            std::vector<std::string> fields{};
            std::vector<std::string> methods{};
            std::vector<Type> fieldTypes{};
            std::vector<FuncRecord> methodRecords{};
        };

        static std::vector<TypeError> errors;

        HashTable<VarEntry, VarRecord, VarHasher> varRecords{};
        HashTable<VarEntry, FuncRecord, VarHasher> funcRecords{};
        HashTable<VarEntry, TypeRecord, VarHasher> typeRecords{};

        const TypeChecker* const scopeChecker{};
        Type currentReturnType{};
        u8 scope{0};

        /* Helpers. */

        template<typename RecordT>
        const RecordT* getRecord(const std::string& name) const;

        bool compatibleTypes(const Type& t1, const Type& t2) const;
        bool canApplyLogicOperator(TokenType oper, const Type& t1, const Type& t2) const;
        bool canApplyCompareOperator(TokenType oper, const Type& t1, const Type& t2) const;
        bool canApplyBitOperator(TokenType oper, const Type& t1, const Type& t2) const;
        bool canApplyShiftOperator(TokenType oper, const Type& t1, const Type& t2) const;
        bool canApplyBinaryOperator(TokenType oper, const Type& t1, const Type& t2) const;
        bool canApplyUnaryOperator(TokenType oper, const Type& type) const;

        bool canIndexInto(const Type& type) const;
        bool canIndexWith(const Type& objType, const Type& indexType) const;
        bool canCall(const Type& type) const;

        bool validArgsForObj(
            const AST::Expression::CallExpr* node,
            const VarRecord& record
        ) const;
        bool validArgsForFunc(
            const AST::Expression::CallExpr* node,
            const FuncRecord& record
        ) const;
        bool validArgsForCtor(
            const AST::Expression::CallExpr* node,
            const TypeRecord& record
        ) const;
        bool validArgsForCallable(const AST::Expression::CallExpr* node) const;
        bool validFieldForType(const Type& type, const Token& field) const;

        /* Type derivation/evaluation. */

        Type typeFromHint(const AST::Types::TypeHint& typeHint) const;
        Type signatureFromFuncRecord(const FuncRecord& record) const;
        Type getElementType(const Type& type) const;

        Type getExprType(const ExprUP& node) const;
        TYPE_GETTER(MutExpr);
        TYPE_GETTER(AssignExpr);
        TYPE_GETTER(LogicExpr);
        TYPE_GETTER(CompareExpr);
        TYPE_GETTER(BitExpr);
        TYPE_GETTER(ShiftExpr);
        TYPE_GETTER(BinaryExpr);
        TYPE_GETTER(UnaryExpr);
        TYPE_GETTER(IndexExpr);
        TYPE_GETTER(CallExpr);
        TYPE_GETTER(FieldExpr);
        TYPE_GETTER(ScopeExpr);
        TYPE_GETTER(IfExpr);
        TYPE_GETTER(LambdaExpr);
        TYPE_GETTER(ListExpr);
        TYPE_GETTER(TableExpr);
        TYPE_GETTER(InstanceExpr);
        TYPE_GETTER(ListCompExpr);
        TYPE_GETTER(TableCompExpr);
        TYPE_GETTER(RefExpr);
        TYPE_GETTER(VarExpr);
        TYPE_GETTER(StringPartExpr);
        TYPE_GETTER(FormatExpr);
        TYPE_GETTER(LiteralExpr);

        void reportTypeError(ErrorCode code);

        /* Statement type-checkers. */

        FuncRecord makeFuncRecord(const AST::Statement::FuncDecl* func) const;
        template<typename NodeT>
        void checkFuncBody(const NodeT* node);
        void checkLoopHeader(const AST::LoopHeader& header);

        CHECK_STMT(VarDecl);
        CHECK_STMT(FuncDecl);
        CHECK_STMT(TypeDecl);
        CHECK_STMT(AliasDecl);
        CHECK_STMT(UseStmt);
        CHECK_STMT(IfStmt);
        CHECK_STMT(WhileStmt);
        CHECK_STMT(ForStmt);
        CHECK_STMT(MatchStmt);
        CHECK_STMT(RepeatStmt);
        CHECK_STMT(ReturnStmt);
        CHECK_STMT(BreakStmt);
        CHECK_STMT(ContinueStmt);
        CHECK_STMT(EndStmt);
        CHECK_STMT(ExprStmt);
        CHECK_STMT(BlockStmt);

        /* Expression type-checkers. */

        CHECK_EXPR(MutExpr);
        CHECK_EXPR(AssignExpr);
        CHECK_EXPR(LogicExpr);
        CHECK_EXPR(CompareExpr);
        CHECK_EXPR(BitExpr);
        CHECK_EXPR(ShiftExpr);
        CHECK_EXPR(BinaryExpr);
        CHECK_EXPR(UnaryExpr);
        CHECK_EXPR(IndexExpr);
        CHECK_EXPR(CallExpr);
        CHECK_EXPR(FieldExpr);
        CHECK_EXPR(ScopeExpr);
        CHECK_EXPR(IfExpr);
        CHECK_EXPR(LambdaExpr);
        CHECK_EXPR(ListExpr);
        CHECK_EXPR(TableExpr);
        CHECK_EXPR(InstanceExpr);
        CHECK_EXPR(ListCompExpr);
        CHECK_EXPR(TableCompExpr);
        CHECK_EXPR(RefExpr);
        CHECK_EXPR(VarExpr);
        CHECK_EXPR(StringPartExpr);
        CHECK_EXPR(FormatExpr);
        CHECK_EXPR(LiteralExpr);

        /* General driver functions. */

        void checkStmt(const StmtUP& node);
        void checkExpr(const ExprUP& node);

    public:
        TypeChecker(TypeChecker* checker = nullptr);
        void check(const StmtVec& program);

    #undef CHECK_STMT
    #undef CHECK_EXPR
    #undef TYPE_GETTER
};

#endif