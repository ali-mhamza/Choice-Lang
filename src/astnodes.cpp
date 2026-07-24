/*
 * Constructors for AST nodes and components.
 */

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
    Stmt{StmtType::VarDecl},
    fix{fix}, names{names}, unpack{unpack}, oper{oper},
    values{std::move(values)} {}

FuncDecl::FuncDecl(const Token& name, std::vector<Param>& params,
    StmtUP& body) :
    Stmt{StmtType::FuncDecl},
    name{name}, params{std::move(params)}, body{std::move(body)} {}

TypeDecl::Field::Field(VarAttr attr, bool fix, const Token& name,
    ExprUP& init) :
    attr{attr}, fix{fix}, name{name}, init{std::move(init)} {}

TypeDecl::TypeDecl(
    const Token& name,
    std::vector<Field>& fields,
    StmtVec& methods
) :
    Stmt{StmtType::TypeDecl},
    name{name}, fields{std::move(fields)}, methods{std::move(methods)} {}

UseStmt::UseStmt(const Token& module, const Token& directory,
    const Token& alias, const std::vector<Entry>& entries) :
    Stmt{StmtType::UseStmt},
    module{module}, directory{directory}, alias{alias}, entries{entries} {}

IfStmt::IfStmt(ExprUP& condition, StmtUP& trueBranch, StmtUP& falseBranch) :
    Stmt{StmtType::IfStmt},
    condition{std::move(condition)}, trueBranch{std::move(trueBranch)},
    falseBranch{std::move(falseBranch)} {}

WhileStmt::WhileStmt(ExprUP& condition, const Token& label, StmtUP& body,
    StmtUP& elseClause) :
    Stmt{StmtType::WhileStmt},
    condition{std::move(condition)}, label{label}, body{std::move(body)},
    elseClause{std::move(elseClause)} {}

ForStmt::ForStmt(AST::LoopHeader& header, const Token& label, StmtUP& body,
    StmtUP& elseClause) :
    Stmt{StmtType::ForStmt},
    header{std::move(header)}, label{label}, body{std::move(body)},
    elseClause{std::move(elseClause)} {}

MatchStmt::MatchCase::MatchCase(ExprUP& value, StmtUP& body, bool fall) :
    value{std::move(value)}, body{std::move(body)},
    fallthrough{fall} {}

MatchStmt::MatchStmt(ExprUP& matchValue, std::vector<MatchCase>& cases) :
    Stmt{StmtType::MatchStmt},
    matchValue{std::move(matchValue)}, cases{std::move(cases)} {}

RepeatStmt::RepeatStmt(ExprUP& condition, const Token& label, StmtUP& body) :
    Stmt{StmtType::RepeatStmt},
    condition{std::move(condition)}, label{label}, body{std::move(body)} {}

ReturnStmt::ReturnStmt(const Token& keyword, ExprUP& expr) :
    Stmt{StmtType::ReturnStmt},
    keyword{keyword}, expr{std::move(expr)} {}

BreakStmt::BreakStmt(const Token& label) :
    Stmt{StmtType::BreakStmt},
    label{label} {}

ContinueStmt::ContinueStmt(const Token& label) :
    Stmt{StmtType::ContinueStmt},
    label{label} {}

EndStmt::EndStmt() :
    Stmt{StmtType::EndStmt} {}

ExprStmt::ExprStmt(ExprUP& expr) :
    Stmt{StmtType::ExprStmt},
    expr{std::move(expr)} {}

BlockStmt::BlockStmt(StmtVec& block) :
    Stmt{StmtType::BlockStmt},
    block{std::move(block)} {}

// Expression constructors.

Expr::Expr(ExprType type) :
    type{type} {}

MutExpr::MutExpr(bool mut, ExprUP value) :
    Expr{ExprType::MutExpr},
    mut{mut}, value{std::move(value)} {}

AssignExpr::AssignExpr(ExprVec& targets, UnpackState unpack,
    const Token& oper, ExprVec& values) :
    Expr{ExprType::AssignExpr},
    targets{std::move(targets)}, unpack{unpack}, oper{oper}, values{std::move(values)} {}

LogicExpr::LogicExpr(ExprUP& left, TokenType oper, ExprUP right) :
    Expr{ExprType::LogicExpr},
    left{std::move(left)}, oper{oper}, right{std::move(right)} {}

CompareExpr::CompareExpr(ExprUP& left, TokenType oper, ExprUP right) :
    Expr{ExprType::CompareExpr},
    left{std::move(left)}, oper{oper}, right{std::move(right)} {}

BitExpr::BitExpr(ExprUP& left, TokenType oper, ExprUP right) :
    Expr{ExprType::BitExpr},
    left{std::move(left)}, oper{oper}, right{std::move(right)} {}

ShiftExpr::ShiftExpr(ExprUP& left, TokenType oper, ExprUP right) :
    Expr{ExprType::ShiftExpr},
    left{std::move(left)}, oper{oper}, right{std::move(right)} {}

BinaryExpr::BinaryExpr(ExprUP& left, TokenType oper, ExprUP right) :
    Expr{ExprType::BinaryExpr},
    left{std::move(left)}, oper{oper}, right{std::move(right)} {}

UnaryExpr::UnaryExpr(const Token& oper, ExprUP expr, bool prev) :
    Expr{ExprType::UnaryExpr},
    oper{oper}, expr{std::move(expr)}, prev{prev} {}

IndexExpr::IndexExpr(ExprUP& obj, ExprUP& index) :
    Expr{ExprType::IndexExpr},
    obj{std::move(obj)}, index{std::move(index)} {}

CallExpr::CallExpr(ExprUP& callee, ExprVec& args, bool builtin) :
    Expr{ExprType::CallExpr},
    callee{std::move(callee)}, args{std::move(args)}, builtin{builtin} {}

FieldExpr::FieldExpr(ExprUP& obj, const Token& field) :
    Expr{ExprType::FieldExpr},
    obj{std::move(obj)}, field{field} {}

ScopeExpr::ScopeExpr(ExprUP& module, const Token& entry) :
    Expr{ExprType::ScopeExpr},
    module{std::move(module)}, entry{entry} {}

IfExpr::IfExpr(ExprUP& condition, ExprUP& trueExpr, ExprUP& falseExpr) :
    Expr{ExprType::IfExpr},
    condition{std::move(condition)}, trueExpr{std::move(trueExpr)},
    falseExpr{std::move(falseExpr)} {}

LambdaExpr::LambdaExpr(std::vector<Param>& params, StmtUP& body, bool iife) :
    Expr{ExprType::LambdaExpr},
    params{std::move(params)}, body{std::move(body)}, iife{iife} {}

ListExpr::ListExpr(ExprVec& entries) :
    Expr{ExprType::ListExpr},
    entries{std::move(entries)} {}

TableExpr::TableExpr(std::vector<TablePair>& pairs) :
    Expr{ExprType::TableExpr},
    pairs{std::move(pairs)} {}

InstanceExpr::Field::Field(const Token& name, ExprUP init) :
    name{name}, init{std::move(init)} {}

InstanceExpr::InstanceExpr(ExprUP& typeName, std::vector<Field>& fields) :
    Expr{ExprType::InstanceExpr},
    typeName{std::move(typeName)}, fields{std::move(fields)} {}

ListCompExpr::ListCompExpr(AST::LoopHeader& header, ExprUP& expr) :
    Expr{ExprType::ListCompExpr},
    header{std::move(header)}, expr{std::move(expr)} {}

TableCompExpr::TableCompExpr(AST::LoopHeader& header, ExprUP& key,
    ExprUP& value) :
    Expr{ExprType::TableCompExpr},
    header{std::move(header)}, key{std::move(key)}, value{std::move(value)} {}

RefExpr::RefExpr(ExprUP& obj) :
    Expr{ExprType::RefExpr},
    obj{std::move(obj)} {}

VarExpr::VarExpr(const Token& name) :
    Expr{ExprType::VarExpr},
    name{name} {}

StringPartExpr::StringPartExpr(const Token& part) :
    Expr{ExprType::StringPartExpr},
    part{part} {}

FormatExpr::FormatExpr(ExprVec& parts) :
    Expr{ExprType::FormatExpr},
    parts{std::move(parts)} {}

LiteralExpr::LiteralExpr(const Token& value) :
    Expr{ExprType::LiteralExpr},
    value{value} {}