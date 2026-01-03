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
            S_FUN_DECL,
            S_CLASS_DECL,
            S_RETURN_STMT,
            S_LOOP_STMT,
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

            VarDecl(TokenType declType, Token& name, ExprUP init);
        };

        struct FunDecl : public Stmt
        {
            Token name;
            vT params;
            StmtVec body;

            FunDecl(Token name, vT& params, StmtVec& body);
        };

        struct ClassDecl : public Stmt
        {
            Token name;
            vT fields;
            StmtVec methods;

            ClassDecl(Token& name, vT& fields, StmtVec& methods);
        };

        struct ReturnStmt : public Stmt
        {
            Token keyword;
            ExprUP expr;

            ReturnStmt(Token& keyword, ExprUP expr);
        };

        struct LoopStmt : public Stmt
        {
            ExprUP condition;
            StmtVec body;

            LoopStmt(ExprUP condition, StmtVec& body);
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

            AssignExpr(ExprUP target, Token oper, ExprUP value);
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
            TokenType oper;
            ExprUP expr;

            UnaryExpr(TokenType oper, ExprUP expr);
        };

        struct CallExpr : public Expr
        {
            ExprUP callee;
            ExprVec args;

            CallExpr(ExprUP callee, ExprVec& args);
        };

        struct VarExpr : public Expr
        {
            Token name;

            VarExpr(Token& name);
        };

        struct LiteralExpr : public Expr
        {
            Token value;

            LiteralExpr(Token& value);
        };
    };
}