#include "../include/astnodes.h"
#include "../include/common.h"
#include "../include/token.h"
#include <utility>
#include <vector>

using namespace AST::Statement;
using namespace AST::Expression;

AST::Param::Param(bool fix, bool variadic, const Token& param,
    ExprUP& defaultVal) :
    fix{fix}, variadic{variadic}, param{param},
    defaultVal{std::move(defaultVal)} {}

AST::LoopHeader::LoopHeader(bool fix, const vT& vars, UnpackState unpack,
    ExprUP& iter, ExprUP& where) :
    fix{fix}, vars{vars}, unpack{unpack}, iter{std::move(iter)},
    where{std::move(where)} {}

// Statement constructors.

Stmt::Stmt(StmtType type) :
    type{type} {}

VarDecl::VarDecl(bool fix, const vT& names, UnpackState unpack,
    const Token& oper, ExprVec& values) :
    Stmt{S_VAR_DECL},
    fix{fix}, names{names}, unpack{unpack}, oper{oper},
    values{std::move(values)} {}

FuncDecl::FuncDecl(const Token& name, std::vector<Param>& params,
    StmtUP& body) :
    Stmt{S_FUNC_DECL},
    name{name}, params{std::move(params)}, body{std::move(body)} {}

TypeDecl::Field::Field(bool fix, const Token& name, ExprUP& init) :
    fix{fix}, name{name}, init{std::move(init)} {}

TypeDecl::TypeDecl(
    const Token& name,
    std::vector<Field>& fields,
    StmtVec& methods
) :
    Stmt{S_TYPE_DECL},
    name{name}, fields{std::move(fields)}, methods{std::move(methods)} {}

IfStmt::IfStmt(ExprUP& condition, StmtUP& trueBranch, StmtUP& falseBranch) :
    Stmt{S_IF_STMT},
    condition{std::move(condition)}, trueBranch{std::move(trueBranch)},
    falseBranch{std::move(falseBranch)} {}

WhileStmt::WhileStmt(ExprUP& condition, const Token& label, StmtUP& body,
    StmtUP& elseClause) :
    Stmt{S_WHILE_STMT},
    condition{std::move(condition)}, label{label}, body{std::move(body)},
    elseClause{std::move(elseClause)} {}

ForStmt::ForStmt(AST::LoopHeader& header, const Token& label, StmtUP& body,
    StmtUP& elseClause) :
    Stmt{S_FOR_STMT},
    header{std::move(header)}, label{label}, body{std::move(body)},
    elseClause{std::move(elseClause)} {}

MatchStmt::MatchCase::MatchCase(ExprUP& value, StmtUP& body, bool fall) :
    value{std::move(value)}, body{std::move(body)},
    fallthrough{fall} {}

MatchStmt::MatchStmt(ExprUP& matchValue, std::vector<MatchCase>& cases) :
    Stmt{S_MATCH_STMT},
    matchValue{std::move(matchValue)}, cases{std::move(cases)} {}

RepeatStmt::RepeatStmt(ExprUP& condition, const Token& label, StmtUP& body) :
    Stmt{S_REPEAT_STMT},
    condition{std::move(condition)}, label{label}, body{std::move(body)} {}

ReturnStmt::ReturnStmt(const Token& keyword, ExprUP& expr) :
    Stmt{S_RETURN_STMT},
    keyword{keyword}, expr{std::move(expr)} {}

BreakStmt::BreakStmt(const Token& label) :
    Stmt{S_BREAK_STMT},
    label{label} {}

ContinueStmt::ContinueStmt(const Token& label) :
    Stmt{S_CONT_STMT},
    label{label} {}

EndStmt::EndStmt() :
    Stmt{S_END_STMT} {}

ExprStmt::ExprStmt(ExprUP& expr) :
    Stmt{S_EXPR_STMT},
    expr{std::move(expr)} {}

BlockStmt::BlockStmt(StmtVec& block) :
    Stmt{S_BLOCK_STMT},
    block{std::move(block)} {}

// Expression constructors.

Expr::Expr(ExprType type) :
    type{type} {}

MutExpr::MutExpr(bool mut, ExprUP value) :
    Expr{E_MUT_EXPR},
    mut{mut}, value{std::move(value)} {}

AssignExpr::AssignExpr(ExprVec& targets, UnpackState unpack,
    const Token& oper, ExprVec& values) :
    Expr{E_ASSIGN_EXPR},
    targets{std::move(targets)}, unpack{unpack}, oper{oper}, values{std::move(values)} {}

LogicExpr::LogicExpr(ExprUP& left, TokenType oper, ExprUP right) :
    Expr{E_LOGIC_EXPR},
    left{std::move(left)}, oper{oper}, right{std::move(right)} {}

CompareExpr::CompareExpr(ExprUP& left, TokenType oper, ExprUP right) :
    Expr{E_COMPARE_EXPR},
    left{std::move(left)}, oper{oper}, right{std::move(right)} {}

BitExpr::BitExpr(ExprUP& left, TokenType oper, ExprUP right) :
    Expr{E_BIT_EXPR},
    left{std::move(left)}, oper{oper}, right{std::move(right)} {}

ShiftExpr::ShiftExpr(ExprUP& left, TokenType oper, ExprUP right) :
    Expr{E_SHIFT_EXPR},
    left{std::move(left)}, oper{oper}, right{std::move(right)} {}

BinaryExpr::BinaryExpr(ExprUP& left, TokenType oper, ExprUP right) :
    Expr{E_BINARY_EXPR},
    left{std::move(left)}, oper{oper}, right{std::move(right)} {}

UnaryExpr::UnaryExpr(const Token& oper, ExprUP expr, bool prev) :
    Expr{E_UNARY_EXPR},
    oper{oper}, expr{std::move(expr)}, prev{prev} {}

IndexExpr::IndexExpr(ExprUP& obj, ExprUP& index) :
    Expr{E_INDEX_EXPR},
    obj{std::move(obj)}, index{std::move(index)} {}

CallExpr::CallExpr(ExprUP& callee, ExprVec& args, bool builtin) :
    Expr{E_CALL_EXPR},
    callee{std::move(callee)}, args{std::move(args)}, builtin{builtin} {}

FieldExpr::FieldExpr(ExprUP& obj, const Token& field) :
    Expr{E_FIELD_EXPR},
    obj{std::move(obj)}, field{field} {}

IfExpr::IfExpr(ExprUP& condition, ExprUP& trueExpr, ExprUP& falseExpr) :
    Expr{E_IF_EXPR},
    condition{std::move(condition)}, trueExpr{std::move(trueExpr)},
    falseExpr{std::move(falseExpr)} {}

LambdaExpr::LambdaExpr(std::vector<Param>& params, StmtUP& body) :
    Expr{E_LAMBDA_EXPR},
    params{std::move(params)}, body{std::move(body)} {}

ListExpr::ListExpr(ExprVec& entries) :
    Expr{E_LIST_EXPR},
    entries{std::move(entries)} {}

TableExpr::TableExpr(std::vector<TablePair>& pairs) :
    Expr{E_TABLE_EXPR},
    pairs{std::move(pairs)} {}

InstanceExpr::Field::Field(const Token& name, ExprUP init) :
    name{name}, init{std::move(init)} {}

InstanceExpr::InstanceExpr(ExprUP& typeName, std::vector<Field>& fields) :
    Expr{E_INSTANCE_EXPR},
    typeName{std::move(typeName)}, fields{std::move(fields)} {}

ListCompExpr::ListCompExpr(AST::LoopHeader& header, ExprUP& expr) :
    Expr{E_LIST_COMP_EXPR},
    header{std::move(header)}, expr{std::move(expr)} {}

TableCompExpr::TableCompExpr(AST::LoopHeader& header, ExprUP& key,
    ExprUP& value) :
    Expr{E_TABLE_COMP_EXPR},
    header{std::move(header)}, key{std::move(key)}, value{std::move(value)} {}

ReferenceExpr::ReferenceExpr(u64 offset, const Token& name) :
    Expr{E_REF_EXPR},
    operOffset{offset}, name(name) {}

VarExpr::VarExpr(const Token& name) :
    Expr{E_VAR_EXPR},
    name{name} {}

StringPartExpr::StringPartExpr(const Token& part) :
    Expr{E_STR_PART_EXPR},
    part{part} {}

FormatExpr::FormatExpr(ExprVec& parts) :
    Expr{E_FORMAT_EXPR},
    parts{std::move(parts)} {}

LiteralExpr::LiteralExpr(const Token& value) :
    Expr{E_LITERAL_EXPR},
    value{value} {}