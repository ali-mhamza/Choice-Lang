#include "../include/astnodes.h"
using namespace AST::Statement;
using namespace AST::Expression;

// Statement constructors.

Stmt::Stmt(StmtType type) :
    type(type) {}

VarDecl::VarDecl(TokenType declType, Token& name, ExprUP init) :
    Stmt(S_VAR_DECL),
    declType(declType), name(name), init(std::move(init)) {}

FunDecl::FunDecl(Token name, vT& params, StmtVec& body) :
    Stmt(S_FUN_DECL),
    name(name), params(params), body(std::move(body)) {}

ClassDecl::ClassDecl(Token& name, vT& fields, StmtVec& methods) :
    Stmt(S_CLASS_DECL),
    name(name), fields(fields), methods(std::move(methods)) {}

IfStmt::IfStmt(ExprUP condition, StmtUP trueBranch, StmtUP falseBranch) :
    Stmt(S_IF_STMT),
    condition(std::move(condition)), trueBranch(std::move(trueBranch)),
    falseBranch(std::move(falseBranch)) {}

WhileStmt::WhileStmt(ExprUP condition, StmtUP body) :
    Stmt(S_WHILE_STMT),
    condition(std::move(condition)), body(std::move(body)) {}

RepeatStmt::RepeatStmt(ExprUP condition, StmtUP body) :
    Stmt(S_REPEAT_STMT),
    condition(std::move(condition)), body(std::move(body)) {}

ReturnStmt::ReturnStmt(Token& keyword, ExprUP expr) :
    Stmt(S_RETURN_STMT),
    keyword(keyword), expr(std::move(expr)) {}

ExprStmt::ExprStmt(ExprUP expr) :
    Stmt(S_EXPR_STMT),
    expr(std::move(expr)) {}

BlockStmt::BlockStmt(StmtVec& block) :
    Stmt(S_BLOCK_STMT),
    block(std::move(block)) {}

// Expression constructors.

Expr::Expr(ExprType type) :
    type(type) {}

AssignExpr::AssignExpr(ExprUP target, Token oper, ExprUP value) :
    Expr(E_ASSIGN_EXPR),
    target(std::move(target)), oper(oper), value(std::move(value)) {}

LogicExpr::LogicExpr(ExprUP left, TokenType oper, ExprUP right) :
    Expr(E_LOGIC_EXPR),
    left(std::move(left)), oper(oper), right(std::move(right)) {}

CompareExpr::CompareExpr(ExprUP left, TokenType oper, ExprUP right) :
    Expr(E_COMPARE_EXPR),
    left(std::move(left)), oper(oper), right(std::move(right)) {}

BitExpr::BitExpr(ExprUP left, TokenType oper, ExprUP right) :
    Expr(E_BIT_EXPR),
    left(std::move(left)), oper(oper), right(std::move(right)) {}

ShiftExpr::ShiftExpr(ExprUP left, TokenType oper, ExprUP right) :
    Expr(E_SHIFT_EXPR),
    left(std::move(left)), oper(oper), right(std::move(right)) {}

BinaryExpr::BinaryExpr(ExprUP left, TokenType oper, ExprUP right) :
    Expr(E_BINARY_EXPR),
    left(std::move(left)), oper(oper), right(std::move(right)) {}

UnaryExpr::UnaryExpr(Token oper, ExprUP expr) :
    Expr(E_UNARY_EXPR),
    oper(oper), expr(std::move(expr)) {}

CallExpr::CallExpr(ExprUP callee, ExprVec& args) :
    Expr(E_CALL_EXPR),
    callee(std::move(callee)), args(std::move(args)) {}

IfExpr::IfExpr(ExprUP condition, ExprUP trueExpr, ExprUP falseExpr) :
    Expr(E_IF_EXPR),
    condition(std::move(condition)), trueExpr(std::move(trueExpr)),
    falseExpr(std::move(falseExpr)) {}

VarExpr::VarExpr(Token& name) :
    Expr(E_VAR_EXPR),
    name(name) {}

LiteralExpr::LiteralExpr(Token& value) :
    Expr(E_LITERAL_EXPR),
    value(value) {}