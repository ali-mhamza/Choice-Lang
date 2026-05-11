#pragma once
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

#define STMT_LIST       \
    X(S_VAR_DECL)       \
    X(S_FUNC_DECL)      \
    X(S_CLASS_DECL)     \
    X(S_IF_STMT)        \
    X(S_WHILE_STMT)     \
    X(S_FOR_STMT)       \
    X(S_MATCH_STMT)     \
    X(S_REPEAT_STMT)    \
    X(S_RETURN_STMT)    \
    X(S_BREAK_STMT)     \
    X(S_CONT_STMT)      \
    X(S_END_STMT)       \
    X(S_EXPR_STMT)      \
    X(S_BLOCK_STMT)

#define EXPR_LIST       \
    X(E_ASSIGN_EXPR)    \
    X(E_LOGIC_EXPR)     \
    X(E_COMPARE_EXPR)   \
    X(E_BIT_EXPR)       \
    X(E_SHIFT_EXPR)     \
    X(E_BINARY_EXPR)    \
    X(E_UNARY_EXPR)     \
    X(E_CALL_EXPR)      \
    X(E_IF_EXPR)        \
    X(E_LAMBDA_EXPR)    \
    X(E_COMPREHEN_EXPR) \
    X(E_LIST_EXPR)      \
    X(E_REF_EXPR)       \
    X(E_VAR_EXPR)       \
    X(E_STR_PART_EXPR)  \
    X(E_FORMAT_EXPR)    \
    X(E_LITERAL_EXPR)

namespace AST
{
    namespace Statement
    {
        enum StmtType : u8
        {
            #define X(type) type,
            STMT_LIST
            #undef X
        };

        struct Stmt
        {
            const StmtType type{};
            u64 sourceStart{UINT64_MAX};
            u64 sourceEnd{UINT64_MAX};

            Stmt(StmtType type);
            virtual ~Stmt() = default;
        };

        struct VarDecl : public Stmt
        {
            const TokenType declType{};
            const Token name{};
            const ExprUP init{};

            VarDecl(
                TokenType declType,
                const Token& name,
                ExprUP& init
            );
        };

        struct FuncDecl : public Stmt
        {
            const Token name{};
            const vT params{};
            const StmtUP body{};

            FuncDecl(
                const Token& name,
                const vT& params,
                StmtUP& body
            );
        };

        struct ClassDecl : public Stmt
        {
            const Token name{};
            const vT fields{};
            const StmtVec methods{};

            ClassDecl(
                const Token& name,
                const vT& fields,
                StmtVec& methods
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
            const Token var{};
            const ExprUP iter{}; // Must be an iterable.
            const ExprUP where{};
            const Token label{};
            const StmtUP body{}, elseClause{};

            ForStmt(
                const Token& var,
                ExprUP& iter,
                ExprUP& where,
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

            ExprStmt(ExprUP expr);
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
            #define X(type) type,
            EXPR_LIST
            #undef X
        };

        struct Expr
        {
            const ExprType type{};
            u64 sourceStart{UINT64_MAX};
            u64 sourceEnd{UINT64_MAX};

            Expr(ExprType type);
            virtual ~Expr() = default;
        };

        struct AssignExpr : public Expr
        {
            const ExprUP target{};
            const Token oper{};
            const ExprUP value{};

            AssignExpr(
                ExprUP& target,
                const Token& oper,
                ExprUP value
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
                const bool prev
            );
        };

        struct CallExpr : public Expr
        {
            const ExprUP callee{};
            const ExprVec args{};
            const bool builtin{};
            const Token rightParen{}; // For error reporting.

            CallExpr(
                ExprUP& callee,
                ExprVec& args,
                const bool builtin,
                const Token& paren
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
            const vT params{};
            const StmtUP body{};

            LambdaExpr(
                const vT& params,
                StmtUP& body
            );
        };

        struct ComprehensionExpr : public Expr
        {
            const Token var{};
            const ExprUP iter{}; // Must be an iterable.
            const ExprUP where{};
            const ExprUP expr{};

            ComprehensionExpr(
                const Token& var,
                ExprUP& iter,
                ExprUP& where,
                ExprUP& expr
            );
        };

        struct ListExpr : public Expr
        {
            const ExprVec entries{};

            ListExpr(ExprVec& entries);
        };

        struct ReferenceExpr : public Expr
        {
            const u64 operOffset{};
            const Token name{};

            ReferenceExpr(u64 offset, const Token& name);
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

constexpr u64 numStmts{AST::Statement::S_BLOCK_STMT + 1};
constexpr u64 numExprs{AST::Expression::E_LITERAL_EXPR + 1};

constexpr std::array<std::string_view, numStmts> stmtTypes{
    #define X(type) #type,
    STMT_LIST
    #undef X
};

constexpr std::array<std::string_view, numExprs> exprTypes{
    #define X(type) #type,
    EXPR_LIST
    #undef X
};