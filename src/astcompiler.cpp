#include "chainTable.h"
#include "../include/astcompiler.h"
#include "../include/bytecode.h"
#include "../include/common.h"
#include "../include/error.h"
#include "../include/opcodes.h"
#include "../include/vartable.h"

class ASTCompVarsWrapper
{
    public:
        chainTable<VarEntry, ui8, VarHasher> vars;

        ASTCompVarsWrapper() = default;
};

ASTCompiler::ASTCompiler() :
    previousReg(0), currentReg(1),
    lastVarReg(0), scope(0), varScopes(1) // For global scope.
{
    varsWrapper = new ASTCompVarsWrapper;
}

ASTCompiler::~ASTCompiler()
{
    delete varsWrapper;
}

void ASTCompiler::defVar(std::string name, ui8 reg , ui8 scope)
{
    varsWrapper->vars[{name, scope}] = reg;
    varScopes.back().push_back(name);
}

ui8* ASTCompiler::getVarSlot(const Token& token, ui8 scope)
{
    VarEntry entry(token.text, scope);
    return varsWrapper->vars.get(entry);
}

ui8* ASTCompiler::getVarSlot(ExprUP& node, ui8 scope)
{
    VarExpr* var = static_cast<VarExpr*>(node.release());
    UP(VarExpr) varUP = UP(VarExpr)(var);
    VarEntry entry(varUP->name.text, scope);
    node.reset(varUP.release());
    return varsWrapper->vars.get(entry);
}

void ASTCompiler::popScope(ui8 scope)
{
    for (std::string& var : varScopes.back())
        varsWrapper->vars.remove({var, scope});
}

void ASTCompiler::freeReg()
{
    previousReg--;
    currentReg--;
}

void ASTCompiler::reserveReg()
{
    previousReg = currentReg;
    currentReg++;
}

DEF(VarDecl)
{
    ui8* slot = getVarSlot(node->name, scope);
    if (slot != nullptr)
    {
        throw CompileError(node->name, "Variable '"
            + std::string(node->name.text)
            + "' is already defined in this scope.");
    }
    
    if (node->init)
        compileExpr(node->init);
    else
    {
        code.loadReg(previousReg, OP_NULL);
        // code.addOp(OP_DEF_VAR, previousReg, OP_NULL);
        reserveReg();
    }

    // We pass an std::string instead of std::string_view
    // since the line containing the variable's text will
    // likely be destroyed soon after (if using the REPL),
    // and thus we must take ownership of the string first
    // to avoid invalidating the view.

    defVar(
        std::string(node->name.text),
        lastVarReg,
        scope
    );
    lastVarReg++;
}

DEF(FunDecl) { (void) node; }
DEF(ClassDecl) { (void) node; }

DEF(ReturnStmt) { (void) node; }
DEF(LoopStmt) { (void) node; }

DEF(ExprStmt)
{
    compileExpr(node->expr);
    freeReg();
}

DEF(BlockStmt)
{
    scope++;
    ui8 origVarReg = lastVarReg;
    varScopes.emplace_back();
    for (StmtUP& stmt : node->block)
        compileStmt(stmt);
    popScope(scope);
    varScopes.pop_back();
    scope--;
    previousReg -= (lastVarReg - origVarReg);
    currentReg -= (lastVarReg - origVarReg);
    lastVarReg = origVarReg;
}

DEF(AssignExpr)
{
    ui8 reg = previousReg;
    compileExpr(node->value);
    ui8* ptr = getVarSlot(node->target, scope);
    // Temporarily assuming regular variables only.
    if (ptr == nullptr)
    {
        VarExpr* temp = static_cast<VarExpr*>(node->target.release());
        UP(VarExpr) varUP = UP(VarExpr)(temp);
        throw CompileError(varUP->name, "Undefined variable '"
            + std::string(varUP->name.text) + "'.");
    }
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

DEF(UnaryExpr)
{
    ui8 firstOper = previousReg;
    compileExpr(node->expr);

    Opcode op;
    switch (node->oper)
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

DEF(VarExpr)
{
    ui8* ptr = getVarSlot(node->name, scope);
    if (ptr == nullptr)
    {
        throw CompileError(node->name, "Undefined variable '"
            + std::string(node->name.text) + "'.");
    }
    // code.addOp(OP_MOVE_R, previousReg, reg);
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
        case E_VAR_EXPR:        COMPILE(VarExpr, node);     break;
        case E_LITERAL_EXPR:    COMPILE(LiteralExpr, node); break;
    }
}

void ASTCompiler::compileStmt(StmtUP& node)
{
    if (node == nullptr) return;

    // ui8 reg = currentReg;
    
    switch (node->type)
    {
        case S_VAR_DECL:    COMPILE(VarDecl, node);     break;
        case S_FUN_DECL:    COMPILE(FunDecl, node);     break;
        case S_CLASS_DECL:  COMPILE(ClassDecl, node);   break;
        case S_RETURN_STMT: COMPILE(ReturnStmt, node);  break;
        case S_LOOP_STMT:   COMPILE(LoopStmt, node);    break;
        case S_EXPR_STMT:   COMPILE(ExprStmt, node);    break;
        case S_BLOCK_STMT:  COMPILE(BlockStmt, node);   break;
    }

    // Reset our registers.
}

#define MIN(a, b) (a < b ? a : b)

ByteCode& ASTCompiler::compile(StmtVec& program)
{
    code.clear();
    try
    {
        for (StmtUP& node : program)
            compileStmt(node);
        if (!program.empty())
            code.addOp(OP_RETURN, static_cast<ui8>(previousReg));
    }
    catch (CompileError& error)
    {
        error.report();
        code.clear();
    }

    return code;
}