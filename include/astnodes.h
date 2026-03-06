#pragma once
#include "common.h"
#include "token.h"
#include <memory>
#include <vector>

namespace AST
{
    namespace Statement { struct Stmt; }
    namespace Expression { struct Expr; }
}

using StmtUP        = std::unique_ptr<AST::Statement::Stmt>;
using ExprUP        = std::unique_ptr<AST::Expression::Expr>;
using StmtVec       = std::vector<StmtUP>;
using ExprVec       = std::vector<ExprUP>;

#define StmtUP(ptr) static_cast<StmtUP>(ptr)
#define ExprUP(ptr) static_cast<ExprUP>(ptr)
#define UP(type)    std::unique_ptr<type>

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
            StmtType type;

            Stmt(StmtType type);
            virtual ~Stmt() = default;
        };

        struct VarDecl : public Stmt
        {
            TokenType declType;
            Token name;
            ExprUP init;

            VarDecl(TokenType declType, const Token& name, ExprUP init);
        };

        struct FuncDecl : public Stmt
        {
            Token name;
            vT params;
            StmtUP body;

            FuncDecl(const Token& name, const vT& params, StmtUP& body);
        };

        struct ClassDecl : public Stmt
        {
            Token name;
            vT fields;
            StmtVec methods;

            ClassDecl(const Token& name, const vT& fields, StmtVec& methods);
        };

        struct IfStmt : public Stmt
        {
            ExprUP condition;
            StmtUP trueBranch;
            StmtUP falseBranch;

            IfStmt(ExprUP condition, StmtUP trueBranch,
                StmtUP falseBranch);
        };

        struct WhileStmt : public Stmt
        {
            ExprUP condition;
            Token label;
            StmtUP body;
            StmtUP elseClause;

            WhileStmt(ExprUP condition, const Token& label, StmtUP body,
                StmtUP elseClause);
        };

        struct ForStmt : public Stmt
        {
            Token var;
            ExprUP iter; // Must be an iterable.
            ExprUP where;
            Token label;
            StmtUP body;
            StmtUP elseClause;

            ForStmt(const Token& var, ExprUP iter, ExprUP where,
                const Token& label, StmtUP body, StmtUP elseClause);
        };

        struct MatchStmt : public Stmt
        {
            struct matchCase
            {
                ExprUP value; // Must be a literal (even if an iterable).
                StmtUP body; // No declarations without a block.
                bool fallthrough;

                matchCase(ExprUP value, StmtUP body, bool fall);
            };
            
            ExprUP matchValue;
            std::vector<matchCase> cases;

            MatchStmt(ExprUP matchValue, std::vector<matchCase>& cases);
        };

        struct RepeatStmt : public Stmt
        {
            ExprUP condition;
            StmtUP body; // Must be a block statement.

            RepeatStmt(ExprUP condition, StmtUP body);
        };

        struct ReturnStmt : public Stmt
        {
            Token keyword;
            ExprUP expr;

            ReturnStmt(const Token& keyword, ExprUP expr);
        };

        struct BreakStmt : public Stmt
        {
            Token label;

            BreakStmt(const Token& label);
        };

        struct ContinueStmt : public Stmt
        {
            ContinueStmt();
        };

        struct EndStmt : public Stmt
        {
            EndStmt();
        };

        struct ExprStmt : public Stmt
        {
            ExprUP expr;

            ExprStmt(ExprUP expr);
        };

        struct BlockStmt : public Stmt
        {
            StmtVec block;

            BlockStmt(StmtVec& block);
        };
    };

    namespace Expression
    {
        enum ExprType : ui8
        {
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
            E_VAR_EXPR,
            E_LITERAL_EXPR
        };
        
        struct Expr
        {
            ExprType type;

            Expr(ExprType type);
            virtual ~Expr() = default;
        };

        struct AssignExpr : public Expr
        {
            ExprUP target;
            Token oper;
            ExprUP value;

            AssignExpr(ExprUP target, const Token& oper, ExprUP value);
        };

        // All five are effectively the same,
        // but it would simplify things to keep them
        // as separate node types.

        struct LogicExpr : public Expr
        {
            ExprUP left;
            TokenType oper;
            ExprUP right;

            LogicExpr(ExprUP left, TokenType oper, ExprUP right);
        };

        struct CompareExpr : public Expr
        {
            ExprUP left;
            TokenType oper;
            ExprUP right;

            CompareExpr(ExprUP left, TokenType oper, ExprUP right);
        };

        struct BitExpr : public Expr
        {
            ExprUP left;
            TokenType oper;
            ExprUP right;

            BitExpr(ExprUP left, TokenType oper, ExprUP right);
        };

        struct ShiftExpr : public Expr
        {
            ExprUP left;
            TokenType oper;
            ExprUP right;

            ShiftExpr(ExprUP left, TokenType oper, ExprUP right);
        };

        struct BinaryExpr : public Expr
        {
            ExprUP left;
            TokenType oper;
            ExprUP right;

            BinaryExpr(ExprUP left, TokenType oper, ExprUP right);
        };

        struct UnaryExpr : public Expr
        {
            Token oper;
            ExprUP expr;
            // Whether or not it evaluates to the previous
            // value in the register (like with post-increment/
            // decrement operators) or the new value.
            bool prev;

            UnaryExpr(const Token& oper, ExprUP expr, bool prev);
        };

        struct CallExpr : public Expr
        {
            ExprUP callee;
            ExprVec args;
            bool builtin;
            Token rightParen; // For error reporting.

            CallExpr(ExprUP callee, ExprVec& args, bool builtin,
                const Token& paren);
        };

        struct IfExpr : public Expr
        {
            ExprUP condition;
            ExprUP trueExpr;
            ExprUP falseExpr;

            IfExpr(ExprUP condition, ExprUP trueExpr, ExprUP falseExpr);
        };

        struct LambdaExpr : public Expr
        {
            vT params;
            StmtUP body;

            LambdaExpr(const vT& params, StmtUP& body);
        };

        struct VarExpr : public Expr
        {
            Token name;

            VarExpr(const Token& name);
        };

        struct LiteralExpr : public Expr
        {
            Token value;

            LiteralExpr(const Token& value);
        };
    };
}