#ifdef COMP_AST

#include "chainTable.h"
#include "../include/astcompiler.h"
#include "../include/bytecode.h"
#include "../include/common.h"
#include "../include/error.h"
#include "../include/opcodes.h"
#include "../include/vartable.h"

constexpr bool accessFix = false;
constexpr bool accessVar = true;

class ASTCompVarsWrapper
{
    public:
        chainTable<VarEntry, ui8, VarHasher> vars;
        chainTable<ui8, bool> access;

        ASTCompVarsWrapper() = default;
};

ASTCompiler::ASTCompiler() :
    previousReg(0), scope(0), varScopes(1), // For global scope.
    varsWrapper(new ASTCompVarsWrapper) {}

ASTCompiler::~ASTCompiler()
{
    delete varsWrapper;
}

inline void ASTCompiler::defVar(std::string name, ui8 reg)
{
    varsWrapper->vars[{name, scope}] = reg;
    varScopes.back().push_back(name);
}

inline void ASTCompiler::defAccess(ui8 reg, bool access)
{
    varsWrapper->access[reg] = access;
}

inline ui8* ASTCompiler::getVarSlot(const Token& token)
{
    for (ui8 i = 0; i <= scope; i++)
    {
        VarEntry entry(token.text, scope - i);
        ui8* slot = varsWrapper->vars.get(entry);
        if (slot != nullptr)
            return slot;
    }

    return nullptr;
}

inline ui8* ASTCompiler::getVarSlot(ExprUP& node)
{
    VarExpr* var = static_cast<VarExpr*>(node.release());
    ui8* slot = getVarSlot(var->name);
    node.reset(var);
    return slot;
}

bool ASTCompiler::getAccess(ui8 reg)
{
    return *(varsWrapper->access.get(reg));
}

void ASTCompiler::popScope()
{
    for (std::string& var : varScopes.back())
        varsWrapper->vars.remove({var, scope});
}

DEF(VarDecl)
{
    ui8* slot = getVarSlot(node->name);
    if (slot != nullptr)
    {
        throw CompileError(node->name, "Variable '"
            + std::string(node->name.text)
            + "' is already defined in this scope.");
    }
    
    ui8 varSlot = previousReg;
    if (node->init)
        compileExpr(node->init);
    else
    {
        code.loadReg(varSlot, OP_NULL);
        reserveReg();
    }

    // We pass an std::string instead of std::string_view
    // since the line containing the variable's text will
    // likely be destroyed soon after (if using the REPL),
    // and thus we must take ownership of the string first
    // to avoid invalidating the view.

    defVar(
        std::string(node->name.text),
        varSlot
    );
    defAccess(varSlot,
        node->declType == TOK_MAKE ? accessVar : accessFix);
}

DEF(FunDecl) { (void) node; }
DEF(ClassDecl) { (void) node; }

DEF(IfStmt)
{
    ui8 reg = previousReg;
    compileExpr(node->condition);
    ui64 falseJump = code.addJump(OP_JUMP_FALSE, reg);
    freeReg();
    compileStmt(node->trueBranch);
    ui64 trueJump = code.addJump(OP_JUMP);
    code.patchJump(falseJump);
    if (node->falseBranch != nullptr)
        compileStmt(node->falseBranch);
    code.patchJump(trueJump);
}

DEF(ReturnStmt) { (void) node; }

DEF(WhileStmt)
{
    ui8 reg = previousReg;
    ui64 loopStart = code.getLoopStart();
    compileExpr(node->condition);
    ui64 falseJump = code.addJump(OP_JUMP_FALSE, reg);
    freeReg();
    compileStmt(node->body);
    code.addLoop(loopStart);
    code.patchJump(falseJump);
}

DEF(ExprStmt)
{
    compileExpr(node->expr);
    freeReg();
}

DEF(BlockStmt)
{
    scope++;
    ui8 origVarReg = previousReg;
    varScopes.emplace_back();
    for (StmtUP& stmt : node->block)
        compileStmt(stmt);
    popScope();
    varScopes.pop_back();
    scope--;
    previousReg = origVarReg;
}

void ASTCompiler::compoundAssign(UP(AssignExpr)& node, ui8 slot)
{
    ui8 reg = previousReg;
    compileExpr(node->value);

    Opcode op;
    switch (node->oper.type)
    {
        case TOK_PLUS_EQ:       op = OP_ADD;            break;
        case TOK_MINUS_EQ:      op = OP_SUB;            break;
        case TOK_STAR_EQ:       op = OP_MULT;           break;
        case TOK_SLASH_EQ:      op = OP_DIV;            break;
        case TOK_PERCENT_EQ:    op = OP_MOD;            break;
        case TOK_STAR_STAR_EQ:  op = OP_POWER;          break;

        case TOK_AMP_EQ:        op = OP_BIT_AND;        break;
        case TOK_BAR_EQ:        op = OP_BIT_OR;         break;
        case TOK_UARROW_EQ:     op = OP_BIT_XOR;        break;
        case TOK_TILDE_EQ:      op = OP_BIT_COMP;       break;
        case TOK_LSHIFT_EQ:     op = OP_BIT_SHIFT_L;    break;
        case TOK_RSHIFT_EQ:     op = OP_BIT_SHIFT_R;    break;
        default: UNREACHABLE();
    }

    code.addOp(op, slot, slot, reg);
    code.addOp(OP_GET_VAR, reg, slot);
}

DEF(AssignExpr)
{
    ui8* ptr = getVarSlot(node->target);
    // Temporarily assuming regular variables only.
    if (ptr == nullptr)
    {
        VarExpr* temp = static_cast<VarExpr*>(node->target.release());
        UP(VarExpr) varUP = UP(VarExpr)(temp);
        throw CompileError(varUP->name, "Undefined variable '"
            + std::string(varUP->name.text) + "'.");
    }
    else if (getAccess(*ptr) == accessFix)
        throw CompileError(node->oper,
            "Cannot assign to a fixed-value variable.");

    if (node->oper.type != TOK_EQUAL)
    {
        compoundAssign(node, *ptr);
        return;
    }

    ui8 reg = previousReg;
    compileExpr(node->value);
    code.addOp(OP_SET_VAR, *ptr, reg);
}

DEF(LogicExpr)
{
    ui8 firstOper = previousReg;
    compileExpr(node->left);

    ui8 secondOper = previousReg;
    compileExpr(node->right);

    code.addOp(node->oper == TOK_AMP_AMP ? OP_AND : OP_OR,
        firstOper, firstOper, secondOper);
    freeReg();
}

DEF(CompareExpr)
{
    ui8 firstOper = previousReg;
    compileExpr(node->left);

    ui8 secondOper = previousReg;
    compileExpr(node->right);

    Opcode op;
    switch (node->oper)
    {
        case TOK_EQ_EQ: op = OP_EQUAL;  break;
        case TOK_GT:
        case TOK_LT_EQ:
            op = OP_GT;
            break;
        case TOK_LT:
        case TOK_GT_EQ:
            op = OP_LT;
            break;
        default: UNREACHABLE();
    }

    code.addOp(op, firstOper, firstOper, secondOper);
    if ((node->oper == TOK_GT_EQ) || (node->oper == TOK_LT_EQ))
        code.addOp(OP_NOT, firstOper, firstOper);
    freeReg();
}

DEF(BitExpr)
{
    ui8 firstOper = previousReg;
    compileExpr(node->left);

    ui8 secondOper = previousReg;
    compileExpr(node->right);

    Opcode op;
    switch (node->oper)
    {
        case TOK_AMP:       op = OP_BIT_AND;    break;
        case TOK_BAR:       op = OP_BIT_OR;     break;
        case TOK_UARROW:    op = OP_BIT_XOR;    break;
        default: UNREACHABLE();
    }

    code.addOp(op, firstOper, firstOper, secondOper);
    freeReg();
}

DEF(ShiftExpr)
{
    ui8 firstOper = previousReg;
    compileExpr(node->left);

    ui8 secondOper = previousReg;
    compileExpr(node->right);

    code.addOp(node->oper == TOK_RIGHT_SHIFT ?
        OP_BIT_SHIFT_R : OP_BIT_SHIFT_L,
        firstOper, firstOper, secondOper);
    freeReg();
}

DEF(BinaryExpr)
{
    ui8 firstOper = previousReg;
    compileExpr(node->left);

    ui8 secondOper = previousReg;
    compileExpr(node->right);

    Opcode op;
    switch (node->oper)
    {
        case TOK_PLUS:      op = OP_ADD;    break;
        case TOK_MINUS:     op = OP_SUB;    break;
        case TOK_STAR:      op = OP_MULT;   break;
        case TOK_SLASH:     op = OP_DIV;    break;
        case TOK_PERCENT:   op = OP_MOD;    break;
        case TOK_STAR_STAR: op = OP_POWER;  break;
        default: UNREACHABLE();
    }

    code.addOp(op, firstOper, firstOper, secondOper);
    freeReg();
}

// Temporarily only dealing with simple identifier
// variables; will need to extend later.

void ASTCompiler::_crementExpr(UP(UnaryExpr)& node)
{
    if (node->expr->type != E_VAR_EXPR)
        throw CompileError(node->oper,
            "Invalid increment/decrement target.");
    ui8* ptr = getVarSlot(node->expr);
    // Temporarily assuming regular variables only.
    if (ptr == nullptr)
    {
        VarExpr* temp = static_cast<VarExpr*>(node->expr.release());
        UP(VarExpr) varUP = UP(VarExpr)(temp);
        throw CompileError(varUP->name, "Undefined variable '"
            + std::string(varUP->name.text) + "'.");
    }
    else if (getAccess(*ptr) == accessFix)
        throw CompileError(node->oper,
            "Cannot modify a fixed-value variable.");

    ui8 reg = previousReg;
    code.loadReg(reg, OP_ONE);
    reserveReg();
    code.addOp((node->oper.type == TOK_INCR ? OP_ADD : OP_SUB),
        *ptr, *ptr, reg);
    code.addOp(OP_GET_VAR, reg, *ptr);
}

DEF(UnaryExpr)
{
    if ((node->oper.type == TOK_INCR) || (node->oper.type == TOK_DECR))
    {
        _crementExpr(node);
        return;
    }
    
    ui8 firstOper = previousReg;
    compileExpr(node->expr);

    Opcode op;
    switch (node->oper.type)
    {
        case TOK_MINUS: op = OP_NEGATE;     break;
        case TOK_BANG:  op = OP_NOT;        break;
        case TOK_TILDE: op = OP_BIT_COMP;   break;
        default: UNREACHABLE();
    }

    code.addOp(op, firstOper, firstOper);
    // We don't free a register since unary
    // operators don't use any extra registers.
    // They apply an operator directly onto a
    // register.
}

DEF(CallExpr) { (void) node; }

DEF(IfExpr)
{
    ui8 reg = previousReg;
    compileExpr(node->condition);
    ui64 falseJump = code.addJump(OP_JUMP_FALSE, reg);
    freeReg();

    ui8 current = previousReg;
    compileExpr(node->trueExpr);
    ui64 trueJump = code.addJump(OP_JUMP);
    code.patchJump(falseJump);

    previousReg = current;
    compileExpr(node->falseExpr);
    code.patchJump(trueJump);
}

DEF(VarExpr)
{
    ui8* ptr = getVarSlot(node->name);
    if (ptr == nullptr)
    {
        throw CompileError(node->name, "Undefined variable '"
            + std::string(node->name.text) + "'.");
    }
    code.addOp(OP_GET_VAR, previousReg, *ptr);
    reserveReg();
}

DEF(LiteralExpr)
{
    Token tok = node->value;
    
    if (tok.type == TOK_NUM)
    {
        Object obj{tok.content.i};
        code.loadRegConst(obj, previousReg);
        reserveReg();
    }

    else if (tok.type == TOK_NUM_DEC)
    {
        Object obj{tok.content.d};
        code.loadRegConst(obj, previousReg);
        reserveReg();
    }
    
    else if (tok.type == TOK_STR_LIT)
    {
        HeapObj* ptr = new String(GET_STR(tok));
        Object obj{ptr};
        code.loadRegConst(obj, previousReg);
        reserveReg();
    }

    else if ((tok.type == TOK_TRUE) || (tok.type == TOK_FALSE))
    {
        bool value = tok.content.b;
        code.loadReg(previousReg, (value ? OP_TRUE : OP_FALSE));
        reserveReg();
    }

    else if (tok.type == TOK_NULL)
    {
        code.loadReg(previousReg, OP_NULL);
        reserveReg();
    }
}

void ASTCompiler::compileExpr(ExprUP& node)
{
    if (node == nullptr) return;
    
    switch (node->type)
    {
        case E_ASSIGN_EXPR:     COMPILE(AssignExpr, node);  break;
        case E_LOGIC_EXPR:      COMPILE(LogicExpr, node);   break;
        case E_COMPARE_EXPR:    COMPILE(CompareExpr, node); break;
        case E_BIT_EXPR:        COMPILE(BitExpr, node);     break;
        case E_SHIFT_EXPR:      COMPILE(ShiftExpr, node);   break;
        case E_BINARY_EXPR:     COMPILE(BinaryExpr, node);  break;
        case E_UNARY_EXPR:      COMPILE(UnaryExpr, node);   break;
        case E_CALL_EXPR:       COMPILE(CallExpr, node);    break;
        case E_IF_EXPR:         COMPILE(IfExpr, node);      break;
        case E_VAR_EXPR:        COMPILE(VarExpr, node);     break;
        case E_LITERAL_EXPR:    COMPILE(LiteralExpr, node); break;
    }
}

void ASTCompiler::compileStmt(StmtUP& node)
{
    if (node == nullptr) return;
    
    switch (node->type)
    {
        case S_VAR_DECL:    COMPILE(VarDecl, node);     break;
        case S_FUN_DECL:    COMPILE(FunDecl, node);     break;
        case S_CLASS_DECL:  COMPILE(ClassDecl, node);   break;
        case S_IF_STMT:     COMPILE(IfStmt, node);      break;
        case S_RETURN_STMT: COMPILE(ReturnStmt, node);  break;
        case S_WHILE_STMT:  COMPILE(WhileStmt, node);   break;
        case S_EXPR_STMT:   COMPILE(ExprStmt, node);    break;
        case S_BLOCK_STMT:  COMPILE(BlockStmt, node);   break;
    }
}

ByteCode& ASTCompiler::compile(StmtVec& program)
{
    code.clear();

    try
    {
        for (StmtUP& node : program)
            compileStmt(node);
    }
    catch (CompileError& error)
    {
        error.report();
        code.clear();
    }

    return code;
}

#endif