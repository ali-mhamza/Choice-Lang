#pragma once
#include "astnodes.h"
#include "common.h"
#include "object.h"
#include "token.h"

#if CH_OPTIMIZATIONS_ON

// Used for small evaluation results in constant
// expressions.

class TreeWalker
{
    #define WALKER(type) \
        [[nodiscard]] static Object evaluate##type(AST::Expression::type* expr)

    private:
        template<typename T>
        [[nodiscard]] static Token syntheticToken(TokenType type, T val);
        [[nodiscard]] static ExprUP objectToNode(const Object& obj);

        WALKER(LogicExpr);
        WALKER(CompareExpr);
        WALKER(BitExpr);
        WALKER(ShiftExpr);
        WALKER(BinaryExpr);
        WALKER(UnaryExpr);
        WALKER(LiteralExpr);
        [[nodiscard]] static Object evaluateExpr(ExprUP& expr);

    public:
        [[nodiscard]] static ExprUP evaluate(ExprUP& expr);

    #undef WALKER
};

class Optimizer
{
    #define DECL_STMT(type) \
        static void optimize##type(StmtUP& node, AST::Statement::type* raw)
    #define DECL_EXPR(type) \
        static void optimize##type(ExprUP& node, AST::Expression::type* raw)

    private:
        /* Constant expression checking. */

        [[nodiscard]] static bool isConstant(const ExprUP& node);
        // Constant expression *with* immutable result/value.
        [[nodiscard]] static bool isImmutableConstant(const ExprUP& node);
        [[nodiscard]] static bool isTruthyConstant(const ExprUP& node);
        [[nodiscard]] static bool isFalsyConstant(const ExprUP& node);
        [[nodiscard]] static bool isEmptyConstant(const ExprUP& node);

        [[nodiscard]] static bool isTruthyLit(const Token& token);

        /* Evaluating constant expressions. */

        [[nodiscard]] static ExprUP computeExpr(ExprUP& node);

        /* Statement optimizers. */

        DECL_STMT(VarDecl);
        DECL_STMT(FuncDecl);
        DECL_STMT(TypeDecl);
        DECL_STMT(UseStmt);
        DECL_STMT(IfStmt);
        DECL_STMT(WhileStmt);
        DECL_STMT(ForStmt);
        DECL_STMT(MatchStmt);
        DECL_STMT(RepeatStmt);
        DECL_STMT(ReturnStmt);
        DECL_STMT(BreakStmt);
        DECL_STMT(ContinueStmt);
        DECL_STMT(EndStmt);
        DECL_STMT(ExprStmt);
        DECL_STMT(BlockStmt);

        /* Expression optimizers. */

        DECL_EXPR(MutExpr);
        DECL_EXPR(AssignExpr);
        DECL_EXPR(LogicExpr);
        DECL_EXPR(CompareExpr);
        DECL_EXPR(BitExpr);
        DECL_EXPR(ShiftExpr);
        DECL_EXPR(BinaryExpr);
        DECL_EXPR(UnaryExpr);
        DECL_EXPR(IndexExpr);
        DECL_EXPR(CallExpr);
        DECL_EXPR(FieldExpr);
        DECL_EXPR(ScopeExpr);
        DECL_EXPR(IfExpr);
        DECL_EXPR(LambdaExpr);
        DECL_EXPR(ListExpr);
        DECL_EXPR(TableExpr);
        DECL_EXPR(InstanceExpr);
        DECL_EXPR(ListCompExpr);
        DECL_EXPR(TableCompExpr);
        DECL_EXPR(ReferenceExpr);
        DECL_EXPR(VarExpr);
        DECL_EXPR(StringPartExpr);
        DECL_EXPR(FormatExpr);
        DECL_EXPR(LiteralExpr);

        /* General driver functions. */

        static void optimizeExpr(ExprUP& node);
        static void optimizeStmt(StmtUP& node);

    public:
        static void optimize(StmtVec& program);

        friend class TreeWalker;

    #undef DECL_STMT
    #undef DECL_EXPR
};

#endif