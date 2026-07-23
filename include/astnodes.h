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
        enum class StmtType : u8
        {
            VarDecl,
            FuncDecl,
            TypeDecl,
            UseStmt,
            IfStmt,
            WhileStmt,
            ForStmt,
            MatchStmt,
            RepeatStmt,
            ReturnStmt,
            BreakStmt,
            ContinueStmt,
            EndStmt,
            ExprStmt,
            BlockStmt
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
            ExprVec values{};

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
            std::vector<Param> params{};
            StmtUP body{};

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
            std::vector<Field> fields{};
            StmtVec methods{}; // Only contains function declarations.

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
            ExprUP condition{};
            StmtUP trueBranch{}, falseBranch{};

            IfStmt(
                ExprUP& condition,
                StmtUP& trueBranch,
                StmtUP& falseBranch
            );
        };

        struct WhileStmt : public Stmt
        {
            ExprUP condition{};
            const Token label{};
            StmtUP body{}, elseClause{};

            WhileStmt(
                ExprUP& condition,
                const Token& label,
                StmtUP& body,
                StmtUP& elseClause
            );
        };

        struct ForStmt : public Stmt
        {
            LoopHeader header{};
            const Token label{};
            StmtUP body{}, elseClause{};

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

            ExprUP matchValue{};
            std::vector<MatchCase> cases{};

            MatchStmt(
                ExprUP& matchValue,
                std::vector<MatchCase>& cases
            );
        };

        struct RepeatStmt : public Stmt
        {
            ExprUP condition{};
            const Token label{};
            StmtUP body{}; // Must be a block statement.

            RepeatStmt(
                ExprUP& condition,
                const Token& label,
                StmtUP& body
            );
        };

        struct ReturnStmt : public Stmt
        {
            const Token keyword{};
            ExprUP expr{};

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
            ExprUP expr{};

            ExprStmt(ExprUP& expr);
        };

        struct BlockStmt : public Stmt
        {
            StmtVec block{};

            BlockStmt(StmtVec& block);
        };
    };

    namespace Expression
    {
        enum class ExprType : u8
        {
            MutExpr,
            AssignExpr,
            LogicExpr,
            CompareExpr,
            BitExpr,
            ShiftExpr,
            BinaryExpr,
            UnaryExpr,
            IndexExpr,
            CallExpr,
            FieldExpr,
            ScopeExpr,
            IfExpr,
            LambdaExpr,
            ListExpr,
            TableExpr,
            InstanceExpr,
            ListCompExpr,
            TableCompExpr,
            RefExpr,
            VarExpr,
            StringPartExpr,
            FormatExpr,
            LiteralExpr
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
            ExprUP value{};

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
            ExprVec values{};

            AssignExpr(
                ExprVec& targets,
                UnpackState unpack,
                const Token& oper,
                ExprVec& values
            );
        };

        struct LogicExpr : public Expr
        {
            ExprUP left{};
            const TokenType oper{};
            ExprUP right{};

            LogicExpr(
                ExprUP& left,
                TokenType oper,
                ExprUP right
            );
        };

        struct CompareExpr : public Expr
        {
            ExprUP left{};
            const TokenType oper{};
            ExprUP right{};

            CompareExpr(
                ExprUP& left,
                TokenType oper,
                ExprUP right
            );
        };

        struct BitExpr : public Expr
        {
            ExprUP left{};
            const TokenType oper{};
            ExprUP right{};

            BitExpr(
                ExprUP& left,
                TokenType oper,
                ExprUP right
            );
        };

        struct ShiftExpr : public Expr
        {
            ExprUP left{};
            const TokenType oper{};
            ExprUP right{};

            ShiftExpr(
                ExprUP& left,
                TokenType oper,
                ExprUP right
            );
        };

        struct BinaryExpr : public Expr
        {
            ExprUP left{};
            const TokenType oper{};
            ExprUP right{};

            BinaryExpr(
                ExprUP& left,
                TokenType oper,
                ExprUP right
            );
        };

        struct UnaryExpr : public Expr
        {
            const Token oper{};
            ExprUP expr{};
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
            ExprUP obj{};
            ExprUP index{};

            IndexExpr(
                ExprUP& obj,
                ExprUP& index
            );
        };

        struct CallExpr : public Expr
        {
            ExprUP callee{};
            ExprVec args{};
            const bool builtin{};

            CallExpr(
                ExprUP& callee,
                ExprVec& args,
                bool builtin
            );
        };

        struct FieldExpr : public Expr
        {
            ExprUP obj{};
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
            ExprUP module{};
            const Token entry{};

            ScopeExpr(
                ExprUP& module,
                const Token& entry
            );
        };

        struct IfExpr : public Expr
        {
            ExprUP condition{};
            ExprUP trueExpr{}, falseExpr{};

            IfExpr(
                ExprUP& condition,
                ExprUP& trueExpr,
                ExprUP& falseExpr
            );
        };

        struct LambdaExpr : public Expr
        {
            std::vector<Param> params{};
            StmtUP body{};

            LambdaExpr(
                std::vector<Param>& params,
                StmtUP& body
            );
        };

        struct ListExpr : public Expr
        {
            ExprVec entries{};

            ListExpr(ExprVec& entries);
        };

        struct TableExpr : public Expr
        {
            struct TablePair
            {
                ExprUP key;
                ExprUP value;
            };

            std::vector<TablePair> pairs{};

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
            std::vector<Field> fields{};

            InstanceExpr(
                ExprUP& typeName,
                std::vector<Field>& fields
            );
        };

        // Comp = Comprehension.

        struct ListCompExpr : public Expr
        {
            LoopHeader header{};
            ExprUP expr{};

            ListCompExpr(
                LoopHeader& header,
                ExprUP& expr
            );
        };

        struct TableCompExpr : public Expr
        {
            LoopHeader header{};
            ExprUP key{};
            ExprUP value{};

            TableCompExpr(
                LoopHeader& header,
                ExprUP& key,
                ExprUP& value
            );
        };

        struct RefExpr : public Expr
        {
            const ExprUP obj{};

            RefExpr(ExprUP& obj);
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