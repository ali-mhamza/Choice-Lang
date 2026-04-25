#pragma once
#include "token.h"
#include <memory>   // For std::unique_ptr.
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
    namespace Statement
    {
        enum StmtType : ui8
        {
            S_VAR_DECL,
            S_FUNC_DECL,
            S_CLASS_DECL,
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
        enum ExprType : ui8
        {
            E_TUPLE_EXPR,
            E_ASSIGN_EXPR,
            E_LOGIC_EXPR,
            E_COMPARE_EXPR,
            E_BIT_EXPR,
            E_SHIFT_EXPR,
            E_BINARY_EXPR,
            E_UNARY_EXPR,
            E_CALL_EXPR,
            E_IF_EXPR,
            E_LAMBDA_EXPR,
            E_COMPREHEN_EXPR,
            E_LIST_EXPR,
            E_VAR_EXPR,
            E_LITERAL_EXPR
        };

        struct Expr
        {
            const ExprType type{};

            Expr(ExprType type);
            virtual ~Expr() = default;
        };

        struct TupleExpr : public Expr
        {
            const ExprVec entries{};

            TupleExpr(ExprVec& entries);
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

        struct VarExpr : public Expr
        {
            const Token name{};

            VarExpr(const Token& name);
        };

        struct LiteralExpr : public Expr
        {
            const Token value{};

            LiteralExpr(const Token& value);
        };
    };
}