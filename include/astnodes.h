#pragma once
#include "attributes.h"
#include "token.h"
#include <array>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace AST
{
    namespace Statement     { struct Stmt; }
    namespace Expression    { struct Expr; }
}

using StmtUP    = std::unique_ptr<AST::Statement::Stmt>;
using ExprUP    = std::unique_ptr<AST::Expression::Expr>;
using StmtVec   = std::vector<StmtUP>;
using ExprVec   = std::vector<ExprUP>;

namespace AST
{
    struct Decl
    {
        VarAttr attr{};
    };

    struct Param
    {
        const bool fix{};
        const bool variadic{};
        const Token param{};
        ExprUP defaultVal{};

        Param(
            bool fix,
            bool variadic,
            const Token& param,
            ExprUP& defaultVal
        );
    };

    struct UnpackState
    {
        // Only one can be true.

        // Whether or not to unpack extra values into
        // the last variable.
        bool unpackLastVar{};
        // Whether or not to ignore any remaining
        // values that were not unpacked.
        bool unpackIgnore{};

        operator bool() const { return unpackLastVar || unpackIgnore; }
    };

    struct LoopHeader
    {
        const bool fix{};
        const vT vars{};
        const UnpackState unpack{};
        ExprUP iter{};
        ExprUP where{};

        LoopHeader() = default;
        LoopHeader(
            bool fix,
            const vT& vars,
            UnpackState unpack,
            ExprUP& iter,
            ExprUP& where
        );
    };

    namespace Statement
    {
        enum StmtType : u8
        {
            S_VAR_DECL,
            S_FUNC_DECL,
            S_TYPE_DECL,
            S_USE_STMT,
            S_IF_STMT,
            S_WHILE_STMT,
            S_FOR_STMT,
            S_MATCH_STMT,
            S_REPEAT_STMT,
            S_RETURN_STMT,
            S_BREAK_STMT,
            S_CONT_STMT,
            S_END_STMT,
            S_EXPR_STMT,
            S_BLOCK_STMT
        };

        struct Stmt
        {
            const StmtType type{};
            u64 sourceStart{UINT64_MAX};
            // Just past the end.
            u64 sourceEnd{UINT64_MAX};

            Stmt(StmtType type);
            virtual ~Stmt() = default;
        };

        struct VarDecl : public Stmt, public Decl
        {
            const bool fix{};
            const vT names{};
            UnpackState unpack{};
            const Token oper{};
            const ExprVec values{};

            VarDecl(
                bool fix,
                const vT& names,
                UnpackState unpack,
                const Token& oper,
                ExprVec& values
            );
        };

        struct FuncDecl : public Stmt, public Decl
        {
            const Token name{};
            const std::vector<Param> params{};
            const StmtUP body{};

            FuncDecl(
                const Token& name,
                std::vector<Param>& params,
                StmtUP& body
            );
        };

        struct TypeDecl : public Stmt, public Decl
        {
            struct Field
            {
                VarAttr attr{};
                const bool fix{};
                const Token name{};
                ExprUP init{};

                Field(
                    VarAttr attr,
                    bool fix,
                    const Token& name,
                    ExprUP& init
                );
            };

            const Token name{};
            const std::vector<Field> fields{};
            const StmtVec methods{}; // Only contains function declarations.

            TypeDecl(
                const Token& name,
                std::vector<Field>& fields,
                StmtVec& methods
            );
        };

        struct UseStmt : public Stmt
        {
            struct Entry
            {
                const Token name{};
                const Token alias{};
            };

            const Token module{};
            const Token directory{};
            const Token alias{};
            const std::vector<Entry> entries{};

            UseStmt(
                const Token& module,
                const Token& directory,
                const Token& alias,
                const std::vector<Entry>& entries
            );
        };

        struct IfStmt : public Stmt
        {
            const ExprUP condition{};
            const StmtUP trueBranch{}, falseBranch{};

            IfStmt(
                ExprUP& condition,
                StmtUP& trueBranch,
                StmtUP& falseBranch
            );
        };

        struct WhileStmt : public Stmt
        {
            const ExprUP condition{};
            const Token label{};
            const StmtUP body{}, elseClause{};

            WhileStmt(
                ExprUP& condition,
                const Token& label,
                StmtUP& body,
                StmtUP& elseClause
            );
        };

        struct ForStmt : public Stmt
        {
            const LoopHeader header{};
            const Token label{};
            const StmtUP body{}, elseClause{};

            ForStmt(
                LoopHeader& header,
                const Token& label,
                StmtUP& body,
                StmtUP& elseClause
            );
        };

        struct MatchStmt : public Stmt
        {
            struct MatchCase
            {
                ExprUP value{}; // Must be a literal (even if an iterable).
                StmtUP body{};  // No declarations without a block.
                bool fallthrough{};

                MatchCase(
                    ExprUP& value,
                    StmtUP& body,
                    bool fall
                );
            };

            const ExprUP matchValue{};
            const std::vector<MatchCase> cases{};

            MatchStmt(
                ExprUP& matchValue,
                std::vector<MatchCase>& cases
            );
        };

        struct RepeatStmt : public Stmt
        {
            const ExprUP condition{};
            const Token label{};
            const StmtUP body{}; // Must be a block statement.

            RepeatStmt(
                ExprUP& condition,
                const Token& label,
                StmtUP& body
            );
        };

        struct ReturnStmt : public Stmt
        {
            const Token keyword{};
            const ExprUP expr{};

            ReturnStmt(
                const Token& keyword,
                ExprUP& expr
            );
        };

        struct BreakStmt : public Stmt
        {
            const Token label{};

            BreakStmt(const Token& label);
        };

        struct ContinueStmt : public Stmt
        {
            const Token label{};

            ContinueStmt(const Token& label);
        };

        struct EndStmt : public Stmt
        {
            EndStmt();
        };

        struct ExprStmt : public Stmt
        {
            const ExprUP expr{};

            ExprStmt(ExprUP& expr);
        };

        struct BlockStmt : public Stmt
        {
            const StmtVec block{};

            BlockStmt(StmtVec& block);
        };
    };

    namespace Expression
    {
        enum ExprType : u8
        {
            E_MUT_EXPR,
            E_ASSIGN_EXPR,
            E_LOGIC_EXPR,
            E_COMPARE_EXPR,
            E_BIT_EXPR,
            E_SHIFT_EXPR,
            E_BINARY_EXPR,
            E_UNARY_EXPR,
            E_INDEX_EXPR,
            E_CALL_EXPR,
            E_FIELD_EXPR,
            E_SCOPE_EXPR,
            E_IF_EXPR,
            E_LAMBDA_EXPR,
            E_LIST_EXPR,
            E_TABLE_EXPR,
            E_INSTANCE_EXPR,
            E_LIST_COMP_EXPR,
            E_TABLE_COMP_EXPR,
            E_REF_EXPR,
            E_VAR_EXPR,
            E_STR_PART_EXPR,
            E_FORMAT_EXPR,
            E_LITERAL_EXPR
        };

        struct Expr
        {
            const ExprType type{};
            u64 sourceStart{UINT64_MAX};
            // Just past the end.
            u64 sourceEnd{UINT64_MAX};

            Expr(ExprType type);
            virtual ~Expr() = default;
        };

        struct MutExpr : public Expr
        {
            bool mut;
            const ExprUP value{};

            MutExpr(
                bool mut,
                ExprUP value
            );
        };

        struct AssignExpr : public Expr
        {
            const ExprVec targets{};
            UnpackState unpack{};
            const Token oper{};
            const ExprVec values{};

            AssignExpr(
                ExprVec& targets,
                UnpackState unpack,
                const Token& oper,
                ExprVec& values
            );
        };

        struct LogicExpr : public Expr
        {
            const ExprUP left{};
            const TokenType oper{};
            const ExprUP right{};

            LogicExpr(
                ExprUP& left,
                TokenType oper,
                ExprUP right
            );
        };

        struct CompareExpr : public Expr
        {
            const ExprUP left{};
            const TokenType oper{};
            const ExprUP right{};

            CompareExpr(
                ExprUP& left,
                TokenType oper,
                ExprUP right
            );
        };

        struct BitExpr : public Expr
        {
            const ExprUP left{};
            const TokenType oper{};
            const ExprUP right{};

            BitExpr(
                ExprUP& left,
                TokenType oper,
                ExprUP right
            );
        };

        struct ShiftExpr : public Expr
        {
            const ExprUP left{};
            const TokenType oper{};
            const ExprUP right{};

            ShiftExpr(
                ExprUP& left,
                TokenType oper,
                ExprUP right
            );
        };

        struct BinaryExpr : public Expr
        {
            const ExprUP left{};
            const TokenType oper{};
            const ExprUP right{};

            BinaryExpr(
                ExprUP& left,
                TokenType oper,
                ExprUP right
            );
        };

        struct UnaryExpr : public Expr
        {
            const Token oper{};
            const ExprUP expr{};
            // Whether or not it evaluates to the previous
            // value in the register (like with post-increment/
            // decrement operators) or the new value.
            const bool prev{};

            UnaryExpr(
                const Token& oper,
                ExprUP expr,
                bool prev = false
            );
        };

        struct IndexExpr : public Expr
        {
            const ExprUP obj{};
            const ExprUP index{};

            IndexExpr(
                ExprUP& obj,
                ExprUP& index
            );
        };

        struct CallExpr : public Expr
        {
            const ExprUP callee{};
            const ExprVec args{};
            const bool builtin{};

            CallExpr(
                ExprUP& callee,
                ExprVec& args,
                bool builtin
            );
        };

        struct FieldExpr : public Expr
        {
            const ExprUP obj{};
            const Token field{};

            FieldExpr(
                ExprUP& obj,
                const Token& field
            );
        };

        // Scoped access from within a module.
        // E.g., module::variable.
        struct ScopeExpr : public Expr
        {
            const ExprUP module{};
            const Token entry{};

            ScopeExpr(
                ExprUP& module,
                const Token& entry
            );
        };

        struct IfExpr : public Expr
        {
            const ExprUP condition{}, trueExpr{}, falseExpr{};

            IfExpr(
                ExprUP& condition,
                ExprUP& trueExpr,
                ExprUP& falseExpr
            );
        };

        struct LambdaExpr : public Expr
        {
            const std::vector<Param> params{};
            const StmtUP body{};

            LambdaExpr(
                std::vector<Param>& params,
                StmtUP& body
            );
        };

        struct ListExpr : public Expr
        {
            const ExprVec entries{};

            ListExpr(ExprVec& entries);
        };

        struct TableExpr : public Expr
        {
            struct TablePair
            {
                ExprUP key;
                ExprUP value;
            };

            const std::vector<TablePair> pairs{};

            TableExpr(std::vector<TablePair>& pairs);
        };

        struct InstanceExpr : public Expr
        {
            struct Field
            {
                const Token name{};
                ExprUP init{};

                Field(
                    const Token& name,
                    ExprUP init
                );
            };

            // Expression so we can evaluate it easily.
            // Must still be a variable.
            const ExprUP typeName{};
            const std::vector<Field> fields{};

            InstanceExpr(
                ExprUP& typeName,
                std::vector<Field>& fields
            );
        };

        // Comp = Comprehension.

        struct ListCompExpr : public Expr
        {
            const LoopHeader header{};
            const ExprUP expr{};

            ListCompExpr(
                LoopHeader& header,
                ExprUP& expr
            );
        };

        struct TableCompExpr : public Expr
        {
            const LoopHeader header{};
            const ExprUP key{};
            const ExprUP value{};

            TableCompExpr(
                LoopHeader& header,
                ExprUP& key,
                ExprUP& value
            );
        };

        struct ReferenceExpr : public Expr
        {
            const ExprUP obj{};

            ReferenceExpr(ExprUP& obj);
        };

        struct VarExpr : public Expr
        {
            const Token name{};

            VarExpr(const Token& name);
        };

        struct StringPartExpr : public Expr
        {
            const Token part{};

            StringPartExpr(const Token& part);
        };

        // Format strings/string interpolation.
        struct FormatExpr : public Expr
        {
            ExprVec parts;

            FormatExpr(ExprVec& parts);
        };

        struct LiteralExpr : public Expr
        {
            const Token value{};

            LiteralExpr(const Token& value);
        };
    };
}