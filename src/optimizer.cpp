#include "../include/optimizer.h"
#include "../include/common.h"
#include "../include/utils.h"
#include <memory>
#include <type_traits>

#if CH_OPTIMIZATIONS_ON

using namespace AST::Statement;
using namespace AST::Expression;

/* TreeWalker class. */

#define WALKER(type) \
    Object TreeWalker::evaluate##type(type* expr)

#define WALK(type)                                  \
    do {                                            \
        auto* ptr = static_cast<type*>(expr.get()); \
        return evaluate##type(ptr);                 \
    } while (false)

#define MAKE_LIT(TYPE, val) \
    std::make_unique<LiteralExpr>(syntheticToken(TOK_##TYPE, (val)))

// TODO: Figure out how to work diagnostic source locations
// with these synthetic tokens.

template<typename T>
Token TreeWalker::syntheticToken(TokenType type, T val)
{
    Token token{};
    token.type = type;

    if constexpr (std::is_same_v<T, i64>)       token.content.i = val;
    if constexpr (std::is_same_v<T, double>)    token.content.d = val;
    if constexpr (std::is_same_v<T, bool>)      token.content.b = val;

    return token;
}

ExprUP TreeWalker::objectToNode(const Object& obj)
{
    switch (obj.type())
    {
        case OBJ_INT:   return MAKE_LIT(NUM, AS_INT(obj));
        case OBJ_DEC:   return MAKE_LIT(NUM_DEC, AS_DEC(obj));
        case OBJ_BOOL:
        {
            bool val{AS_BOOL(obj)};
            return (val ? MAKE_LIT(TRUE, val) : MAKE_LIT(FALSE, val));
        }
        case OBJ_NULL:  return MAKE_LIT(NULL, AS_NULL(obj));
        default: CH_UNREACHABLE();
    }
}

WALKER(LogicExpr)
{
    Object left{evaluateExpr(expr->left)};
    if (!IS_VALID(left)) return Object{};

    bool _and{(expr->oper == TOK_AMP_AMP) || (expr->oper == TOK_AND)};
    bool _or{(expr->oper == TOK_BAR_BAR) || (expr->oper == TOK_OR)};

    if (_and && !left.isTruthy()) return false;
    if (_or && left.isTruthy()) return true;

    Object right{evaluateExpr(expr->right)};
    if (!IS_VALID(right)) return Object{};
    return right.isTruthy();

    CH_UNREACHABLE();
}

WALKER(CompareExpr)
{
    Object left{evaluateExpr(expr->left)};
    Object right{evaluateExpr(expr->right)};

    bool valid{IS_VALID(left) && IS_VALID(right)};
    bool inequality{
        (expr->oper == TOK_GT) || (expr->oper == TOK_LT) ||
        (expr->oper == TOK_GT_EQ) || (expr->oper == TOK_LT_EQ)
    };
    bool comparable{IS_COMPARABLE(left) && IS_COMPARABLE(right)};

    if (!valid || (inequality && !comparable))
        return Object{};

    try
    {
        switch (expr->oper)
        {
            case TOK_EQ_EQ: return (left == right);
            case TOK_GT:    return (left > right);
            case TOK_GT_EQ: return ((left > right) || (left == right));
            case TOK_LT:    return (left < right);
            case TOK_LT_EQ: return ((left < right) || (left == right));
            case TOK_IN:     return left.in(right);
            default: CH_UNREACHABLE();
        }
    }
    catch (...)
    {
        return Object{};
    }
    
}

WALKER(BitExpr)
{
    Object left{evaluateExpr(expr->left)};
    Object right{evaluateExpr(expr->right)};

    if (!IS_INT(left) || !IS_INT(right)) return Object{};

    u64 aVal{AS_UINT(left)};
    u64 bVal{AS_UINT(right)};

    switch (expr->oper)
    {
        case TOK_AMP:       return fromUnsigned(aVal & bVal);
        case TOK_BAR:       return fromUnsigned(aVal | bVal);
        case TOK_UARROW:    return fromUnsigned(aVal ^ bVal);
        default: CH_UNREACHABLE();
    }
}

WALKER(ShiftExpr)
{
    Object left{evaluateExpr(expr->left)};
    Object right{evaluateExpr(expr->right)};

    if (!IS_INT(left) || !IS_INT(right)) return Object{};

    u64 aVal{AS_UINT(left)};
    u64 bVal{AS_UINT(right)};

    if (expr->oper == TOK_LEFT_SHIFT)
    {
        if (bVal >= 64) return Object{};
        return fromUnsigned(aVal << bVal);
    }
    else if (expr->oper == TOK_RIGHT_SHIFT)
    {
        if (bVal >= 64) return Object{};
        // Manually perform wraparound to maintain LHS signed-ness.
        i64 term{(AS_INT(left) >= 0) ? 0 : INT64_MIN};
        return fromUnsigned(aVal >> bVal) + term;
    }

    CH_UNREACHABLE();
}

WALKER(BinaryExpr)  { (void) expr; return Object{}; }
WALKER(UnaryExpr)   { (void) expr; return Object{}; }

WALKER(LiteralExpr)
{
    const Token& tok{expr->value};

    switch (tok.type)
    {
        case TOK_NUM:       return Object{tok.content.i};
        case TOK_NUM_DEC:   return Object{tok.content.d};
        // For now.
        case TOK_STR_LIT:   return Object{};
        case TOK_RAW_STR:   return Object{};
        case TOK_TRUE:      return Object{true};
        case TOK_FALSE:     return Object{false};
        case TOK_NULL:      return Object{OBJ_NULL};
        default: CH_UNREACHABLE();
    }
}

Object TreeWalker::evaluateExpr(ExprUP& expr)
{
    if (expr == nullptr) return Object{};

    switch (expr->type)
    {
        case E_LOGIC_EXPR:      WALK(LogicExpr);
        case E_COMPARE_EXPR:    WALK(CompareExpr);
        case E_BIT_EXPR:        WALK(BitExpr);
        case E_SHIFT_EXPR:      WALK(ShiftExpr);
        case E_BINARY_EXPR:     WALK(BinaryExpr);
        case E_UNARY_EXPR:      WALK(UnaryExpr);
        case E_LITERAL_EXPR:    WALK(LiteralExpr);
        default:            return Object{};
    }
}

ExprUP TreeWalker::evaluate(ExprUP& expr)
{
    Object result{evaluateExpr(expr)};
    // Evaluation failed.
    if (!IS_VALID(result)) return std::move(expr);
    return objectToNode(result);
}

#undef WALKER
#undef WALK
#undef MAKE_LIT

/* Optimizer class. */

#define DEF_STMT(type) \
    void Optimizer::optimize##type(StmtUP& node, type* raw)
#define DEF_EXPR(type) \
    void Optimizer::optimize##type(ExprUP& node, type* raw)

#define COMPUTE()                                       \
    do {                                                \
        if (isConstant(node)) node = computeExpr(node); \
    } while (false)

#define OPTIMIZE(type)                              \
    do {                                            \
        auto* ptr = static_cast<type*>(node.get()); \
        optimize##type(node, ptr);                  \
    } while (false)

#define PASS            (void) node; (void) raw;
#define PASS_COMPUTE    (void) node;

#define CHECK_OPERANDS(type)                                    \
    do {                                                        \
        auto* ptr = static_cast<type*>(node.get());             \
        return isConstant(ptr->left) && isConstant(ptr->right); \
    } while (false)

#define OPTIMIZE_OPERANDS(type)     \
    do {                            \
        optimizeExpr(raw->left);    \
        optimizeExpr(raw->right);   \
    } while (false)

/* Constant expression checking. */

bool Optimizer::isConstant(const ExprUP& node)
{
    if (node == nullptr) return false;

    switch (node->type)
    {
        case E_MUT_EXPR:
        {
            MutExpr* mut{static_cast<MutExpr*>(node.get())};
            return isConstant(mut->value);
        }
        case E_ASSIGN_EXPR:     return false;
        case E_LOGIC_EXPR:      CHECK_OPERANDS(LogicExpr);
        case E_COMPARE_EXPR:    CHECK_OPERANDS(CompareExpr);
        case E_BIT_EXPR:        CHECK_OPERANDS(BitExpr);
        case E_SHIFT_EXPR:      CHECK_OPERANDS(ShiftExpr);
        case E_BINARY_EXPR:     CHECK_OPERANDS(BinaryExpr);
        case E_UNARY_EXPR:
        {
            UnaryExpr* unary{static_cast<UnaryExpr*>(node.get())};
            return isConstant(unary->expr);
        }
        case E_INDEX_EXPR:
        {
            IndexExpr* index{static_cast<IndexExpr*>(node.get())};
            return isConstant(index->obj) && isConstant(index->index);
        }
        case E_CALL_EXPR:   return false;
        case E_FIELD_EXPR:  return false;
        case E_SCOPE_EXPR:  return false;
        case E_IF_EXPR:
        {
            IfExpr* expr{static_cast<IfExpr*>(node.get())};
            return (isConstant(expr->condition) && isConstant(expr->trueExpr)
                && isConstant(expr->falseExpr));
        }
        case E_LAMBDA_EXPR: return false;
        case E_LIST_EXPR:
        {
            ListExpr* list{static_cast<ListExpr*>(node.get())};
            for (const auto& entry : list->entries)
            {
                if (!isConstant(entry))
                    return false;
            }

            return true;
        }
        case E_TABLE_EXPR:
        {
            TableExpr* table{static_cast<TableExpr*>(node.get())};
            for (const auto& pair : table->pairs)
            {
                if (!isConstant(pair.key) || !isConstant(pair.value))
                    return false;
            }

            return true;
        }
        case E_INSTANCE_EXPR:   return false;
        case E_LIST_COMP_EXPR:  return false;
        case E_TABLE_COMP_EXPR: return false;
        case E_REF_EXPR:        return false;
        case E_VAR_EXPR:        return false;
        case E_STR_PART_EXPR:   return false;
        case E_FORMAT_EXPR:     return false;
        case E_LITERAL_EXPR:    return true;
        default: CH_UNREACHABLE();
    }
}

bool Optimizer::isTruthyLit(const Token& token)
{
    if (!IS_LITERAL_TOK(token.type)) return false;

    switch (token.type)
    {
        case TOK_NUM:       return (token.content.i != 0);
        case TOK_NUM_DEC:   return (token.content.d != 0.0);
        case TOK_STR_LIT:   return (token.text.size() > 2);
        case TOK_RAW_STR:   return (token.text.size() > 3);
        case TOK_TRUE:      return true;
        case TOK_FALSE:     return false;
        case TOK_NULL:      return false;
        default: CH_UNREACHABLE();
    }
}

bool Optimizer::isImmutableConstant(const ExprUP& node)
{
    if (node == nullptr) return false;

    switch (node->type)
    {
        case E_MUT_EXPR:
        {
            MutExpr* expr{static_cast<MutExpr*>(node.get())};
            if (!expr->mut && isConstant(expr->value))
                return true;
            return isImmutableConstant(expr->value);
        }
        case E_ASSIGN_EXPR:
        {
            AssignExpr* expr{static_cast<AssignExpr*>(node.get())};
            return isImmutableConstant(expr->values.front());
        }
        case E_LOGIC_EXPR:      return isConstant(node);
        case E_COMPARE_EXPR:    return isConstant(node);
        case E_BIT_EXPR:        return isConstant(node);
        case E_SHIFT_EXPR:      return isConstant(node);
        case E_BINARY_EXPR:     return false;
        // Unary expressions (currently) all evaluate to either
        // numbers or Booleans, which are both immutable types.
        case E_UNARY_EXPR:
        {
            UnaryExpr* expr{static_cast<UnaryExpr*>(node.get())};
            return isConstant(expr->expr);
        }
        case E_INDEX_EXPR:      return false;
        case E_CALL_EXPR:       return false;
        case E_FIELD_EXPR:      return false;
        // Scoped entries do not (currently) support any form of
        // modification.
        case E_SCOPE_EXPR:      return true;
        case E_IF_EXPR:
        {
            IfExpr* expr{static_cast<IfExpr*>(node.get())};
            if (isTruthyConstant(expr->condition))
                return isImmutableConstant(expr->trueExpr);
            else if (isFalsyConstant(expr->condition))
                return isImmutableConstant(expr->falseExpr);
            return false;
        }
        case E_LAMBDA_EXPR:     return true;
        case E_LIST_EXPR:       return false;
        case E_TABLE_EXPR:      return false;
        case E_INSTANCE_EXPR:   return false;
        case E_LIST_COMP_EXPR:  return false;
        case E_TABLE_COMP_EXPR: return false;
        case E_REF_EXPR:        return false;
        case E_VAR_EXPR:        return false;
        case E_STR_PART_EXPR:   return false;
        case E_FORMAT_EXPR:     return false;
        case E_LITERAL_EXPR:    return true;
        default: CH_UNREACHABLE();
    }
}

bool Optimizer::isTruthyConstant(const ExprUP& node)
{
    if (!isConstant(node)) return false;

    // TODO: Expand to other node types.

    switch (node->type)
    {
        case E_MUT_EXPR:
        {
            MutExpr* mut{static_cast<MutExpr*>(node.get())};
            return isTruthyConstant(mut->value);
        }
        case E_LITERAL_EXPR:
        {
            LiteralExpr* lit{static_cast<LiteralExpr*>(node.get())};
            return isTruthyLit(lit->value);
        }
        case E_LIST_EXPR:
        {
            ListExpr* list{static_cast<ListExpr*>(node.get())};
            return !list->entries.empty();
        }
        case E_TABLE_EXPR:
        {
            TableExpr* table{static_cast<TableExpr*>(node.get())};
            return !table->pairs.empty();
        }
        default:
        {
            return false;
        }
    }
}

bool Optimizer::isFalsyConstant(const ExprUP& node)
{
    return (isConstant(node) && !isTruthyConstant(node));
}

bool Optimizer::isEmptyConstant(const ExprUP& node)
{
    if (!isConstant(node)) return false;

    switch (node->type)
    {
        case E_LIST_EXPR:
        {
            ListExpr* list{static_cast<ListExpr*>(node.get())};
            return list->entries.empty();
        }
        case E_TABLE_EXPR:
        {
            TableExpr* table{static_cast<TableExpr*>(node.get())};
            return table->pairs.empty();
        }
        default:
        {
            return false;
        }
    }
}

/* Evaluating constant expressions. */

ExprUP Optimizer::computeExpr(ExprUP& node)
{
    return TreeWalker::evaluate(node);
}

/* Statement optimizers. */

DEF_STMT(VarDecl)
{
    PASS;

    for (ExprUP& value : raw->values)
        optimizeExpr(value);
}

DEF_STMT(FuncDecl)
{
    PASS;

    for (auto& param : raw->params)
        optimizeExpr(param.defaultVal);
    optimizeStmt(raw->body);
}

DEF_STMT(TypeDecl)
{
    PASS;

    for (auto& field : raw->fields)
        optimizeExpr(field.init);
    optimize(raw->methods);
}

DEF_STMT(UseStmt) { PASS; }

DEF_STMT(IfStmt)
{
    PASS;

    optimizeExpr(raw->condition);
    if (isTruthyConstant(raw->condition))
    {
        optimizeStmt(raw->trueBranch);
        node = std::move(raw->trueBranch);
    }
    else if (isFalsyConstant(raw->condition))
    {
        optimizeStmt(raw->falseBranch);
        node = std::move(raw->falseBranch);
    }
    else
    {
        optimizeStmt(raw->trueBranch);
        optimizeStmt(raw->falseBranch);
    }
}

DEF_STMT(WhileStmt)
{
    PASS;

    optimizeExpr(raw->condition);
    if (isFalsyConstant(raw->condition))
    {
        optimizeStmt(raw->elseClause);
        node = std::move(raw->elseClause);
    }
    else if (isTruthyConstant(raw->condition))
        raw->elseClause.reset();
    else
    {
        optimizeStmt(raw->body);
        optimizeStmt(raw->elseClause);
    }
}

DEF_STMT(ForStmt)
{
    PASS;

    optimizeExpr(raw->header.iter);
    optimizeExpr(raw->header.where);

    if (isEmptyConstant(raw->header.iter)
        || isFalsyConstant(raw->header.where))
    {
        optimizeStmt(raw->elseClause);
        node = std::move(raw->elseClause);
    }
    else
    {
        if (isTruthyConstant(raw->header.where))
            raw->header.where.reset();
        optimizeStmt(raw->body);
    }
}

DEF_STMT(MatchStmt)
{
    PASS;

    optimizeExpr(raw->matchValue);
    for (auto& match : raw->cases)
    {
        optimizeExpr(match.value);
        optimizeStmt(match.body);
    }
}

// Optimizing the loop out of a repeat-until block may
// lead to compilation errors if there are any 'break' or
// 'continue' statements in the loop body, so we make sure
// there are none before removing the loop.

static bool isDirectLoop(const RepeatStmt* repeat)
{
    const BlockStmt* body{static_cast<const BlockStmt*>(repeat->body.get())};
    for (const auto& stmt : body->block)
    {
        switch (stmt->type)
        {
            case S_BREAK_STMT:
            {
                const BreakStmt* breakStmt{static_cast<const BreakStmt*>(stmt.get())};
                bool equalLabels{
                    repeat->label && (breakStmt->label.text == repeat->label.text)
                };
                if (!breakStmt->label || equalLabels) return false;
                continue;
            }
            case S_CONT_STMT:
            {
                const ContinueStmt* contStmt{static_cast<const ContinueStmt*>(stmt.get())};
                bool equalLabels{
                    repeat->label && (contStmt->label.text == repeat->label.text)
                };
                if (!contStmt->label || equalLabels) return false;
                continue;
            }
            default: continue;
        }
    }

    return true;
}

DEF_STMT(RepeatStmt)
{
    PASS;

    optimizeExpr(raw->condition);
    optimizeStmt(raw->body);
    if (isTruthyConstant(raw->condition) && isDirectLoop(raw))
        node = std::move(raw->body);
}

DEF_STMT(ReturnStmt)
{
    PASS;

    optimizeExpr(raw->expr);
}

DEF_STMT(BreakStmt)     { PASS; }
DEF_STMT(ContinueStmt)  { PASS; }
DEF_STMT(EndStmt)       { PASS; }

DEF_STMT(ExprStmt)
{
    PASS;

    optimizeExpr(raw->expr);
}

DEF_STMT(BlockStmt)
{
    PASS;

    for (StmtUP& stmt : raw->block)
        optimizeStmt(stmt);
}

/* Expression optimizers. */

DEF_EXPR(MutExpr)
{
    PASS;

    if (isImmutableConstant(raw->value))
    {
        node = std::move(raw->value);
        optimizeExpr(node);
    }
    else if (isConstant(raw->value))
        raw->value = computeExpr(raw->value);
}

DEF_EXPR(AssignExpr)
{
    PASS;

    for (auto& value : raw->values)
        optimizeExpr(value);
}

DEF_EXPR(LogicExpr)     { PASS; OPTIMIZE_OPERANDS(); COMPUTE(); }
DEF_EXPR(CompareExpr)   { PASS; OPTIMIZE_OPERANDS(); COMPUTE(); }
DEF_EXPR(BitExpr)       { PASS; OPTIMIZE_OPERANDS(); COMPUTE(); }
DEF_EXPR(ShiftExpr)     { PASS; OPTIMIZE_OPERANDS(); COMPUTE(); }
DEF_EXPR(BinaryExpr)    { PASS; OPTIMIZE_OPERANDS(); COMPUTE(); }

DEF_EXPR(UnaryExpr)
{
    PASS;

    optimizeExpr(raw->expr);
    COMPUTE();
}

DEF_EXPR(IndexExpr)
{
    PASS;

    optimizeExpr(raw->obj);
    optimizeExpr(raw->index);
    COMPUTE();
}

DEF_EXPR(CallExpr)
{
    PASS;

    // For now.
    for (auto& arg : raw->args)
        optimizeExpr(arg);
}

DEF_EXPR(FieldExpr)
{
    PASS;

    optimizeExpr(raw->obj);
}

DEF_EXPR(ScopeExpr)
{
    PASS;

    optimizeExpr(raw->module);
}

DEF_EXPR(IfExpr)
{
    PASS;

    optimizeExpr(raw->condition);
    if (isTruthyConstant(raw->condition))
    {
        optimizeExpr(raw->trueExpr);
        node = std::move(raw->trueExpr);
    }
    else if (isFalsyConstant(raw->condition))
    {
        optimizeExpr(raw->falseExpr);
        node = std::move(raw->falseExpr);
    }
    else
    {
        optimizeExpr(raw->trueExpr);
        optimizeExpr(raw->falseExpr);
    }
}

DEF_EXPR(LambdaExpr)
{
    PASS;

    for (auto& param : raw->params)
        optimizeExpr(param.defaultVal);
    optimizeStmt(raw->body);
}

DEF_EXPR(ListExpr)
{
    PASS;

    for (ExprUP& entry : raw->entries)
        optimizeExpr(entry);
}

DEF_EXPR(TableExpr)
{
    PASS;

    for (auto& pair : raw->pairs)
    {
        optimizeExpr(pair.key);
        optimizeExpr(pair.value);
    }
}

DEF_EXPR(InstanceExpr)
{
    PASS;

    for (auto& field : raw->fields)
        optimizeExpr(field.init);
}

DEF_EXPR(ListCompExpr)
{
    PASS;

    optimizeExpr(raw->header.iter);
    optimizeExpr(raw->header.where);

    if (isEmptyConstant(raw->header.iter)
        || isFalsyConstant(raw->header.where))
    {
        ExprVec vec{};
        node = std::make_unique<ListExpr>(vec);
    }
    else
    {
        if (isTruthyConstant(raw->header.where))
            raw->header.where.reset();
        optimizeExpr(raw->expr);
    }
}

DEF_EXPR(TableCompExpr)
{
    PASS;

    optimizeExpr(raw->header.iter);
    optimizeExpr(raw->header.where);

    if (isEmptyConstant(raw->header.iter)
        || isFalsyConstant(raw->header.where))
    {
        std::vector<TableExpr::TablePair> vec{};
        node = std::make_unique<TableExpr>(vec);
    }
    else
    {
        if (isTruthyConstant(raw->header.where))
            raw->header.where.reset();
        optimizeExpr(raw->key);
        optimizeExpr(raw->value);
    }
}

DEF_EXPR(ReferenceExpr)     { PASS; }
DEF_EXPR(VarExpr)           { PASS; }
DEF_EXPR(StringPartExpr)    { PASS; }

DEF_EXPR(FormatExpr)
{
    PASS;

    for (auto& part : raw->parts)
        optimizeExpr(part);
}

DEF_EXPR(LiteralExpr) { PASS; }

void Optimizer::optimizeExpr(ExprUP& node)
{
    if (node == nullptr) return;

    switch (node->type)
    {
        case E_MUT_EXPR:        OPTIMIZE(MutExpr);          break;
        case E_ASSIGN_EXPR:     OPTIMIZE(AssignExpr);       break;
        case E_LOGIC_EXPR:      OPTIMIZE(LogicExpr);        break;
        case E_COMPARE_EXPR:    OPTIMIZE(CompareExpr);      break;
        case E_BIT_EXPR:        OPTIMIZE(BitExpr);          break;
        case E_SHIFT_EXPR:      OPTIMIZE(ShiftExpr);        break;
        case E_BINARY_EXPR:     OPTIMIZE(BinaryExpr);       break;
        case E_UNARY_EXPR:      OPTIMIZE(UnaryExpr);        break;
        case E_INDEX_EXPR:      OPTIMIZE(IndexExpr);        break;
        case E_CALL_EXPR:       OPTIMIZE(CallExpr);         break;
        case E_FIELD_EXPR:      OPTIMIZE(FieldExpr);        break;
        case E_SCOPE_EXPR:      OPTIMIZE(ScopeExpr);        break;
        case E_IF_EXPR:         OPTIMIZE(IfExpr);           break;
        case E_LAMBDA_EXPR:     OPTIMIZE(LambdaExpr);       break;
        case E_LIST_EXPR:       OPTIMIZE(ListExpr);         break;
        case E_TABLE_EXPR:      OPTIMIZE(TableExpr);        break;
        case E_INSTANCE_EXPR:   OPTIMIZE(InstanceExpr);     break;
        case E_LIST_COMP_EXPR:  OPTIMIZE(ListCompExpr);     break;
        case E_TABLE_COMP_EXPR: OPTIMIZE(TableCompExpr);    break;
        case E_REF_EXPR:        OPTIMIZE(ReferenceExpr);    break;
        case E_VAR_EXPR:        OPTIMIZE(VarExpr);          break;
        case E_STR_PART_EXPR:   OPTIMIZE(StringPartExpr);   break;
        case E_FORMAT_EXPR:     OPTIMIZE(FormatExpr);       break;
        case E_LITERAL_EXPR:    OPTIMIZE(LiteralExpr);      break;
    }
}

void Optimizer::optimizeStmt(StmtUP& node)
{
    if (node == nullptr) return;

    switch (node->type)
    {
        case S_VAR_DECL:    OPTIMIZE(VarDecl);      break;
        case S_FUNC_DECL:   OPTIMIZE(FuncDecl);     break;
        case S_TYPE_DECL:   OPTIMIZE(TypeDecl);     break;
        case S_USE_STMT:    OPTIMIZE(UseStmt);      break;
        case S_IF_STMT:     OPTIMIZE(IfStmt);       break;
        case S_WHILE_STMT:  OPTIMIZE(WhileStmt);    break;
        case S_FOR_STMT:    OPTIMIZE(ForStmt);      break;
        case S_MATCH_STMT:  OPTIMIZE(MatchStmt);    break;
        case S_REPEAT_STMT: OPTIMIZE(RepeatStmt);   break;
        case S_RETURN_STMT: OPTIMIZE(ReturnStmt);   break;
        case S_BREAK_STMT:  OPTIMIZE(BreakStmt);    break;
        case S_CONT_STMT:   OPTIMIZE(ContinueStmt); break;
        case S_END_STMT:    OPTIMIZE(EndStmt);      break;
        case S_EXPR_STMT:   OPTIMIZE(ExprStmt);     break;
        case S_BLOCK_STMT:  OPTIMIZE(BlockStmt);    break;
    }
}

void Optimizer::optimize(StmtVec& program)
{
    for (StmtUP& stmt : program)
        optimizeStmt(stmt);
}

#undef DEF_STMT
#undef DEF_EXPR
#undef COMPUTE
#undef OPTIMIZE
#undef PASS
#undef PASS_COMPUTE
#undef CHECK_OPERANDS
#undef OPTIMIZE_OPERANDS

#endif