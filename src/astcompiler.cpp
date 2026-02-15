#ifdef COMP_AST

#include "linearTable.h"
#include "../include/astcompiler.h"
#include "../include/bytecode.h"
#include "../include/common.h"
#include "../include/error.h"
#include "../include/linear_alloc.h"
#include "../include/natives.h"
#include "../include/opcodes.h"
#include "../include/utils.h"
#include "../include/vartable.h"

constexpr bool accessFix = false;
constexpr bool accessVar = true;

class ASTCompVarsWrapper
{
    public:
        linearTable<VarEntry, ui8, VarHasher> vars;
        linearTable<ui8, bool> access;

        ASTCompVarsWrapper() = default;
};

class ASTCompLoopLabels
{
    public:
        linearTable<std::string_view, std::vector<ui64>> labels;

        ASTCompLoopLabels() = default;
};

ASTCompiler::ASTCompiler() :
    previousReg(0), scope(0),
    varsWrapper(new ASTCompVarsWrapper),
    labelsWrapper(new ASTCompLoopLabels),
    endJumps(nullptr), breakJumps(nullptr),
    continueJumps(nullptr), hitError(false),
    errorCount(0) {}

ASTCompiler::~ASTCompiler()
{
    delete varsWrapper;
    delete labelsWrapper;
}

inline void ASTCompiler::defVar(std::string name, ui8 reg, bool access)
{
    varsWrapper->vars[{name, scope}] = reg;
    varsWrapper->access[reg] = access;
    if (scope != 0) varScopes.top().push_back(name);
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

inline bool ASTCompiler::getAccess(ui8 reg)
{
    return *(varsWrapper->access.get(reg));
}

inline void ASTCompiler::popScope()
{
    auto& scopeVec = varScopes.top();
    for (std::string& var : scopeVec)
        varsWrapper->vars.remove({var, scope});
}

DEF(VarDecl)
{
    ui8* slot = getVarSlot(node->name);
    if (slot != nullptr)
    {
        #if ALLOW_REDEFS
            ui8 varSlot = *slot;
            ui8 reg = previousReg;
            if (node->init)
            {
                compileExpr(node->init);
                code.addOp(OP_SET_VAR, varSlot, reg);
            }
            else
                code.loadReg(varSlot, OP_NULL);
            return;
            
        #else
            REPORT_ERROR(node->name,
                "Variable '" + std::string(node->name.text)
                + "' is already defined in this scope.");
        #endif
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

    defVar(std::string(node->name.text), varSlot,
        node->declType == TOK_MAKE ? accessVar : accessFix);
}

DEF(FuncDecl)
{
    ui8* slot = getVarSlot(node->name);
    bool redefined = false;
    if (slot != nullptr)
    {
        #if ALLOW_REDEFS
            redefined = true;
        #else
            REPORT_ERROR(node->name,
                "Object '" + std::string(node->name.text)
                + "' is already defined in this scope.");
        #endif
    }

    ui8 varSlot = (redefined ? *slot : previousReg);
    std::string name = std::string(node->name.text);
    ASTCompiler miniCompiler;
    for (Token& param : node->params)
    {
        ui8 reg = miniCompiler.previousReg;
        miniCompiler.defVar(std::string(param.text), reg, accessVar);
        miniCompiler.reserveReg();
    }
    miniCompiler.compileStmt(node->body);
    miniCompiler.code.addOp(OP_VOID, 0);
    miniCompiler.code.addOp(OP_RETURN, 0);

    ByteCode& funcCode = miniCompiler.code;
    Object func = ALLOC(Function, ObjDealloc<Function>, name, funcCode);

    if (redefined)
    {
        code.loadRegConst(func, varSlot);
        return;
    }

    code.loadRegConst(func, varSlot);

    defVar(name, varSlot, accessFix); // Temporarily.
    reserveReg();
}

DEF(ClassDecl) { (void) node; }

DEF(IfStmt)
{
    ui8 reg = previousReg;
    compileExpr(node->condition);
    ui64 falseJump = code.addJump(OP_JUMP_FALSE, reg);
    freeReg();
    compileStmt(node->trueBranch);
    if (node->falseBranch != nullptr)
    {
        ui64 trueJump = code.addJump(OP_JUMP);
        code.patchJump(falseJump);
        if (node->falseBranch != nullptr)
            compileStmt(node->falseBranch);
        code.patchJump(trueJump);
    }
    else
        code.patchJump(falseJump);
}

DEF(WhileStmt)
{
    ui8 reg = previousReg;
    ui64 loopStart = code.getLoopStart();
    if (node->label.type != TOK_EOF)
        this->labelsWrapper->labels.add(
            node->label.text, // Will persist at least as long as compilation takes.
            {}
        );

    std::vector<ui64> breaks;
    auto prevBreaks = breakJumps;
    breakJumps = &breaks;

    std::vector<ui64> continues;
    auto prevContinues = continueJumps;
    continueJumps = &continues;

    compileExpr(node->condition);
    ui64 falseJump = code.addJump(OP_JUMP_FALSE, reg);
    freeReg();
    compileStmt(node->body);

    for (ui64 jump : continues)
        code.patchJump(jump);

    code.addLoop(loopStart);
    code.patchJump(falseJump);

    if (node->elseClause != nullptr)
        compileStmt(node->elseClause);

    for (ui64 jump : breaks)
        code.patchJump(jump);
    if (node->label.type != TOK_EOF)
    {
        auto* vec = this->labelsWrapper->labels.get(
            node->label.text
        );
        for (ui64 jump : *vec)
            code.patchJump(jump);
        this->labelsWrapper->labels.remove(
            node->label.text
        );
    }

    breakJumps = prevBreaks;
    continueJumps = prevContinues;
}

void ASTCompiler::forLoopHelper(UP(ForStmt)& node, ui8 varReg, ui8 iterReg)
{
    code.addOp(OP_MAKE_ITER, varReg, iterReg);
    ui64 failJump = code.addJump(OP_JUMP); // If we fail to construct an iterator.

    ui64 loopStart = code.getLoopStart();
    ui64 whereJump = 0;
    if (node->where != nullptr)
    {
        ui8 whereReg = previousReg;
        compileExpr(node->where);
        whereJump = code.addJump(OP_JUMP_FALSE, whereReg);
    }

    compileStmt(node->body);

    if (whereJump != 0)
        code.patchJump(whereJump);
    for (ui64 jump : *continueJumps)
        code.patchJump(jump);

    ui16 diff = static_cast<ui16>(code.codeSize() - loopStart + 5);
    code.addOp(OP_UPDATE_ITER, varReg, iterReg,
        static_cast<ui8>((diff >> 8) & 0xff),
        static_cast<ui8>(diff & 0xff)
    );

    code.patchJump(failJump);
    if (node->elseClause != nullptr)
        compileStmt(node->elseClause);
    for (ui64 jump : *breakJumps)
        code.patchJump(jump);
    if (node->label.type != TOK_EOF)
    {
        auto* vec = this->labelsWrapper->labels.get(
            node->label.text
        );
        for (ui64 jump : *vec)
            code.patchJump(jump);
        this->labelsWrapper->labels.remove(
            node->label.text
        );
    }
}

DEF(ForStmt)
{
    scope++;
    ui8 origVarReg = previousReg;
    varScopes.emplace();

    if (node->label.type != TOK_EOF)
        this->labelsWrapper->labels.add(
            node->label.text,
            {}
        );
    
    std::vector<ui64> breaks;
    auto prevBreaks = breakJumps;
    breakJumps = &breaks;

    std::vector<ui64> continues;
    auto prevContinues = continueJumps;
    continueJumps = &continues;
    
    ui8 varReg = previousReg;
    defVar(std::string(node->var.text), varReg, accessFix); // For now.
    reserveReg();

    ui8 iterReg = previousReg;
    compileExpr(node->iter);

    forLoopHelper(node, varReg, iterReg);

    breakJumps = prevBreaks;
    continueJumps = prevContinues;

    popScope();
    varScopes.pop();
    scope--;
    previousReg = origVarReg;
}

void ASTCompiler::matchCaseHelper(MatchStmt::matchCase& checkCase,
    const ui8 matchReg, ui64& fallJump, ui64& emptyJump)
{
    ui8 caseReg = previousReg;
    compileExpr(checkCase.value);
    code.addOp(OP_EQUAL, caseReg, matchReg, caseReg);
    ui64 falseJump = code.addJump(OP_JUMP_FALSE, caseReg);
    freeReg();

    if (fallJump != 0) // We skip condition checking during fallthrough.
        code.patchJump(fallJump);
    if (emptyJump != 0)
    {
        code.patchJump(emptyJump);
        emptyJump = 0;
    }
    // We check here since compileStmt will call .release()
    // on the unique_ptr body field, which will make it a
    // nullptr regardless.
    bool empty = (checkCase.body == nullptr);
    compileStmt(checkCase.body); // Can handle empty (nullptr) body.

    // If we have fallthrough, or there's already fallthrough,
    // fall/keep falling.
    if (checkCase.fallthrough || (fallJump != 0))
        fallJump = code.addJump(OP_JUMP);
    else if (empty)
        emptyJump = code.addJump(OP_JUMP);
    else
        this->endJumps->push_back(code.addJump(OP_JUMP));

    code.patchJump(falseJump);
}

DEF(MatchStmt)
{
    ui8 matchReg = previousReg;
    compileExpr(node->matchValue);

    std::vector<ui64> jumps;
    auto prevEndJumps = endJumps;
    this->endJumps = &jumps;

    ui64 fallJump = 0; // Invalid jump offset value.
    ui64 emptyJump = 0;

    for (MatchStmt::matchCase& checkCase : node->cases)
    {
        if (checkCase.value != nullptr)
            matchCaseHelper(checkCase, matchReg, fallJump, emptyJump);
        else // Default case.
            compileStmt(checkCase.body); // No need for any jumps.
    }

    for (ui64 jump : jumps)
        code.patchJump(jump);
    freeReg(); // Remove the match value.

    this->endJumps = prevEndJumps;
}

DEF(RepeatStmt)
{
    ui64 loopStart = code.getLoopStart();
    compileStmt(node->body);
    ui8 reg = previousReg;
    compileExpr(node->condition);
    ui64 trueJump = code.addJump(OP_JUMP_TRUE, reg);
    freeReg();
    code.addLoop(loopStart);
    code.patchJump(trueJump);
}

DEF(ReturnStmt)
{
    ui8 reg = previousReg;
    if (node->expr != nullptr)
        compileExpr(node->expr);
    else
        code.addOp(OP_VOID, reg);
    code.addOp(OP_RETURN, reg);
}

DEF(BreakStmt)
{
    if (node->label.type == TOK_EOF)
        this->breakJumps->push_back(code.addJump(OP_JUMP));
    else
    {
        auto* vec = this->labelsWrapper->labels.get(
            node->label.text
        );
        if (vec == nullptr)
            REPORT_ERROR(node->label,
                "Break label is not assigned to any loop.");
        else
            vec->push_back(code.addJump(OP_JUMP));
    }
}

DEF(ContinueStmt)
{
    this->continueJumps->push_back(code.addJump(OP_JUMP));
}

DEF(EndStmt)
{
    this->endJumps->push_back(code.addJump(OP_JUMP));
}

DEF(ExprStmt)
{
    ui8 reg = previousReg;
    ExprType type = node->expr->type;
    compileExpr(node->expr);
    if (inRepl && (type != E_ASSIGN_EXPR))
        code.addOp(OP_PRINT_VALID, reg);
    freeReg();
}

DEF(BlockStmt)
{
    scope++;
    ui8 origVarReg = previousReg;
    varScopes.emplace();
    for (StmtUP& stmt : node->block)
        compileStmt(stmt);
    popScope();
    varScopes.pop();
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
        REPORT_ERROR(varUP->name, "Undefined variable '"
            + std::string(varUP->name.text) + "'.");
    }
    else if (getAccess(*ptr) == accessFix)
        REPORT_ERROR(node->oper,
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
    if ((node->oper == TOK_AMP_AMP) || (node->oper == TOK_AND)) // &&, and
    {
        ui8 reg = previousReg;
        compileExpr(node->left);
        ui64 falseJump = code.addJump(OP_JUMP_FALSE, reg);
        previousReg = reg;
        compileExpr(node->right);
        code.patchJump(falseJump);
    }
    else if ((node->oper == TOK_BAR_BAR) || (node->oper == TOK_OR)) // ||, or
    {
        ui8 reg = previousReg;
        compileExpr(node->left);
        ui64 trueJump = code.addJump(OP_JUMP_TRUE, reg);
        previousReg = reg;
        compileExpr(node->right);
        code.patchJump(trueJump);
    }
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
        case TOK_IN:
        case TOK_NOT: // not in
            op = OP_IN;
            break;
        case TOK_EQ_EQ:
        case TOK_BANG_EQ:
            op = OP_EQUAL;
            break;
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
    if ((node->oper == TOK_NOT) || (node->oper == TOK_GT_EQ)
        || (node->oper == TOK_LT_EQ) || (node->oper == TOK_BANG_EQ))
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
        REPORT_ERROR(node->oper,
            "Invalid increment/decrement target.");
    ui8* ptr = getVarSlot(node->expr);
    // Temporarily assuming regular variables only.
    if (ptr == nullptr)
    {
        VarExpr* temp = static_cast<VarExpr*>(node->expr.release());
        UP(VarExpr) varUP = UP(VarExpr)(temp);
        REPORT_ERROR(varUP->name, "Undefined variable '"
            + std::string(varUP->name.text) + "'.");
    }
    else if (getAccess(*ptr) == accessFix)
        REPORT_ERROR(node->oper,
            "Cannot modify a fixed-value variable.");

    if (node->prev) // Load the original value.
        code.addOp(OP_GET_VAR, previousReg, *ptr);
    code.addOp(node->oper.type == TOK_INCR ? OP_INCREMENT : OP_DECREMENT,
        *ptr, *ptr);
    if (!node->prev) // Load the new value.
        code.addOp(OP_GET_VAR, previousReg, *ptr);
    reserveReg();
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
        case TOK_BANG:
        case TOK_NOT:   op = OP_NOT;        break;
        case TOK_TILDE: op = OP_BIT_COMP;   break;
        default: UNREACHABLE();
    }

    code.addOp(op, firstOper, firstOper);
    // We don't free a register since unary
    // operators don't use any extra registers.
    // They apply an operator directly onto a
    // register.
}

DEF(CallExpr)
{
    // For now only assuming:
    // [identfier][!](args...)
    VarExpr* var = static_cast<VarExpr*>(node->callee.get());

    ui8 location;
    if (node->builtin)
    {
        auto find = Natives::builtins.find(var->name.text);
        if (find == Natives::builtins.end())
            REPORT_ERROR(var->name, "No builtin '"
                + std::string(var->name.text) + "' function.");
        location = static_cast<ui8>(find->second);
    }
    else
    {
        ui8* ptr = getVarSlot(var->name);
        if (ptr == nullptr)
            REPORT_ERROR(var->name, "Undefined variable '"
                + std::string(var->name.text) + "'.");
        location = *ptr;
    }

    ui8 argsStart = previousReg;
    for (ExprUP& arg : node->args)
        compileExpr(arg);

    ui8 size = static_cast<ui8>(node->args.size());
    code.addOp(node->builtin ? OP_CALL_NAT : OP_CALL_DEF,
        location, argsStart, size);

    // Skip arguments.
    // Our return value replaces the first argument.
    if (size == 0)
        reserveReg();
    else
        previousReg -= size - 1;
}

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
        REPORT_ERROR(node->name, "Undefined variable '"
            + std::string(node->name.text) + "'.");
    code.addOp(OP_GET_VAR, previousReg, *ptr);
    reserveReg();
}

DEF(LiteralExpr)
{
    Token tok = node->value;

    if (tok.type == TOK_NUM)
    {
        Object obj = tok.content.i;
        code.loadRegConst(obj, previousReg);
        reserveReg();
    }

    else if (tok.type == TOK_NUM_DEC)
    {
        Object obj = tok.content.d;
        code.loadRegConst(obj, previousReg);
        reserveReg();
    }

    else if (tok.type == TOK_STR_LIT)
    {
        Object obj = ALLOC(String, ObjDealloc<String>, GET_STR(tok));
        code.loadRegConst(obj, previousReg);
        reserveReg();
    }

    else if (tok.type == TOK_RANGE)
    {
        Object obj = ALLOC(Range, ObjDealloc<Range>, constructRange(tok.text));
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
        case E_ASSIGN_EXPR:     COMPILE(AssignExpr);    break;
        case E_LOGIC_EXPR:      COMPILE(LogicExpr);     break;
        case E_COMPARE_EXPR:    COMPILE(CompareExpr);   break;
        case E_BIT_EXPR:        COMPILE(BitExpr);       break;
        case E_SHIFT_EXPR:      COMPILE(ShiftExpr);     break;
        case E_BINARY_EXPR:     COMPILE(BinaryExpr);    break;
        case E_UNARY_EXPR:      COMPILE(UnaryExpr);     break;
        case E_CALL_EXPR:       COMPILE(CallExpr);      break;
        case E_IF_EXPR:         COMPILE(IfExpr);        break;
        case E_VAR_EXPR:        COMPILE(VarExpr);       break;
        case E_LITERAL_EXPR:    COMPILE(LiteralExpr);   break;
    }
}

void ASTCompiler::compileStmt(StmtUP& node)
{
    if (node == nullptr) return;
    
    switch (node->type)
    {
        case S_VAR_DECL:    COMPILE(VarDecl);       break;
        case S_FUNC_DECL:   COMPILE(FuncDecl);      break;
        case S_CLASS_DECL:  COMPILE(ClassDecl);     break;
        case S_IF_STMT:     COMPILE(IfStmt);        break;
        case S_WHILE_STMT:  COMPILE(WhileStmt);     break;
        case S_FOR_STMT:    COMPILE(ForStmt);       break;
        case S_MATCH_STMT:  COMPILE(MatchStmt);     break;
        case S_REPEAT_STMT: COMPILE(RepeatStmt);    break;
        case S_RETURN_STMT: COMPILE(ReturnStmt);    break;
        case S_BREAK_STMT:  COMPILE(BreakStmt);     break;
        case S_CONT_STMT:   COMPILE(ContinueStmt);  break;
        case S_END_STMT:    COMPILE(EndStmt);       break;
        case S_EXPR_STMT:   COMPILE(ExprStmt);      break;
        case S_BLOCK_STMT:  COMPILE(BlockStmt);     break;
    }
}

ByteCode& ASTCompiler::compile(StmtVec& program)
{
    code.clear();
    hitError = false;
    // Inherit errorCount from parser.

    for (StmtUP& node : program)
        compileStmt(node);

    if (hitError) code.clear();
    return code;
}

#endif