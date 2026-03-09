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

ASTCompiler::ASTCompiler(ASTCompiler* comp) :
    scopeCompiler(comp),
    varsWrapper(new ASTCompVarsWrapper),
    labelsWrapper(new ASTCompLoopLabels)
{
    previousReg = (comp == nullptr ? Natives::FuncType::NUM_FUNCS : 0);
    depth = (comp == nullptr ? 0 : comp->depth + 1);
    if (depth == 0) // Global scope compiler.
    {
        for (ui8 i = 0; i < Natives::FuncType::NUM_FUNCS; i++)
        {
            defVar(
                std::string(Natives::funcNames[i]),
                i,
                accessFix // For now.
            );
        }
    }
}

ASTCompiler::~ASTCompiler()
{
    delete varsWrapper;
    delete labelsWrapper;
}

inline void ASTCompiler::defVar(std::string name, ui8 reg, bool access)
{
    varsWrapper->vars[{ name, scope }] = reg;
    varsWrapper->access[reg] = access;
    if (scope != 0) varScopes.top().push_back(name);
}

inline bool ASTCompiler::getAccess(ui8 reg)
{
    bool* ret = varsWrapper->access.get(reg);
    ASSERT(ret != nullptr,
        "Variable registered with no access field.");
    return *ret;
}

inline ASTCompiler::VarInfo ASTCompiler::getVarInfo(const Token& token)
{
    for (ui8 i = 0; i <= scope; i++)
    {
        VarEntry entry(token.text, scope - i);
        ui8* slot = varsWrapper->vars.get(entry);
        if (slot != nullptr)
            return { slot, static_cast<ui8>(scope - i), depth,
                getAccess(*slot) };
    }

    if (scopeCompiler == nullptr)
        return { nullptr, 0, 0 , false };
    return scopeCompiler->getVarInfo(token);
}

inline ASTCompiler::VarInfo ASTCompiler::getVarInfo(ExprUP& node)
{
    VarExpr* var = static_cast<VarExpr*>(node.release());
    VarInfo position = getVarInfo(var->name);
    node.reset(var);
    return position;
}

inline void ASTCompiler::popScope()
{
    auto& scopeVec = varScopes.top();
    for (std::string& var : scopeVec)
        varsWrapper->vars.remove({var, scope});
}

DEF(VarDecl)
{
    VarInfo pos = getVarInfo(node->name);
    if ((pos.slot != nullptr)
        && (pos.scope == scope)
        && (pos.depth == depth))
    {
        #if ALLOW_REDEFS
            ui8 varSlot = *(pos.slot);
            ui8 reg = previousReg;
            if (node->init != nullptr)
            {
                compileExpr(node->init);
                // We only declare in the current function scope.
                code.addOp(OP_SET_VAR, depth, varSlot, reg);
            }
            else
                code.loadReg(varSlot, OP_NULL, depth);
            return;
            
        #else
            REPORT_ERROR(node->name,
                "Variable '" + std::string(node->name.text)
                + "' is already defined in this scope.");
        #endif
    }

    ui8 varSlot = previousReg;
    if (node->init != nullptr)
        compileExpr(node->init);
    else
    {
        code.loadReg(varSlot, OP_NULL, depth);
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
    VarInfo pos = getVarInfo(node->name);
    bool redefined = false;
    if ((pos.slot != nullptr)
        && (pos.scope == scope)
        && (pos.depth == depth))
    {
        #if ALLOW_REDEFS
            redefined = true;
        #else
            REPORT_ERROR(node->name,
                "Object '" + std::string(node->name.text)
                + "' is already defined in this scope.");
        #endif
    }

    size_t size = node->params.size();
    if (size > PARAMETER_MAX)
    {
        REPORT_ERROR(node->params[PARAMETER_MAX],
            "Too many parameters in function.");
    }

    ui8 varSlot = (redefined ? *(pos.slot) : previousReg);
    std::string name = std::string(node->name.text);
    if (!redefined)
    {
        defVar(name, varSlot, accessFix); // Temporarily.
        reserveReg();
    }

    ASTCompiler miniCompiler(this);
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
    funcCode.depth = miniCompiler.depth;
    this->hitError = miniCompiler.hitError;
    Object func = ALLOC(Function, ObjDealloc<Function>, name,
        funcCode, size);
    // We only declare in the current function scope.
    code.loadRegConst(func, varSlot, depth);
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
    compileStmt(node->elseClause); // Will do nothing if elseClause == nullptr.

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
    compileStmt(node->elseClause); // Will do nothing if elseClause == nullptr.

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
    code.addOp(OP_EQUAL, caseReg, matchReg);
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
    if (node->expr == nullptr) return;
    
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

DEF(TupleExpr)
{
    constexpr int TUPLE_GROUP = 5;
    
    ui8 tupleReg = previousReg;
    code.addOp(OP_TUPLE, tupleReg);
    reserveReg();

    ui8 count = 0;
    ui8 startReg = previousReg;
    auto emitTuple = [this, tupleReg, &count, startReg] {
        code.addOp(OP_EXT_TUPLE, tupleReg, startReg, count);
        previousReg = startReg;
        count = 0;
    };

    for (ExprUP& entry : node->entries)
    {
        compileExpr(entry);
        if (++count == TUPLE_GROUP)
            emitTuple();
    }

    if (count > 0) emitTuple();
}

void ASTCompiler::compoundAssign(UP(AssignExpr)& node, const VarInfo& pos)
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

        case TOK_AMP_EQ:        op = OP_AND;            break;
        case TOK_BAR_EQ:        op = OP_OR;             break;
        case TOK_UARROW_EQ:     op = OP_XOR;            break;
        case TOK_TILDE_EQ:      op = OP_COMP;           break;
        case TOK_LSHIFT_EQ:     op = OP_SHIFT_L;        break;
        case TOK_RSHIFT_EQ:     op = OP_SHIFT_R;        break;
        default: UNREACHABLE();
    }

    ui8 slot = *(pos.slot);
    code.addOp(op, pos.depth, slot, reg);
    code.addOp(OP_GET_VAR, pos.depth, reg, slot);
}

DEF(AssignExpr)
{
    VarInfo pos = getVarInfo(node->target);
    // Temporarily assuming regular variables only.
    if (pos.slot == nullptr)
    {
        VarExpr* temp = static_cast<VarExpr*>(node->target.get());
        REPORT_ERROR(temp->name, "Undefined variable '"
            + std::string(temp->name.text) + "'.");
    }
    else if (pos.access == accessFix)
        REPORT_ERROR(node->oper,
            "Cannot assign to a fixed-value variable.");

    if (node->oper.type != TOK_EQUAL)
    {
        compoundAssign(node, pos);
        return;
    }

    ui8 reg = previousReg;
    compileExpr(node->value);
    code.addOp(OP_SET_VAR, pos.depth, *(pos.slot), reg);
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

    code.addOp(op, firstOper, secondOper);
    if ((node->oper == TOK_NOT) || (node->oper == TOK_GT_EQ)
        || (node->oper == TOK_LT_EQ) || (node->oper == TOK_BANG_EQ))
            code.addOp(OP_NOT, firstOper);
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
        case TOK_AMP:       op = OP_AND;    break;
        case TOK_BAR:       op = OP_OR;     break;
        case TOK_UARROW:    op = OP_XOR;    break;
        default: UNREACHABLE();
    }

    code.addOp(op, depth, firstOper, secondOper);
    freeReg();
}

DEF(ShiftExpr)
{
    ui8 firstOper = previousReg;
    compileExpr(node->left);

    ui8 secondOper = previousReg;
    compileExpr(node->right);

    code.addOp(node->oper == TOK_RIGHT_SHIFT ?
        OP_SHIFT_R : OP_SHIFT_L, depth, firstOper, secondOper);
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

    code.addOp(op, depth, firstOper, secondOper);
    freeReg();
}

// Temporarily only dealing with simple identifier
// variables; will need to extend later.

void ASTCompiler::_crementExpr(UP(UnaryExpr)& node)
{
    if (node->expr->type != E_VAR_EXPR)
        REPORT_ERROR(node->oper,
            "Invalid increment/decrement target.");

    VarInfo pos = getVarInfo(node->expr);
    if (pos.slot == nullptr)
    {
        VarExpr* temp = static_cast<VarExpr*>(node->expr.get());
        REPORT_ERROR(temp->name, "Undefined variable '"
            + std::string(temp->name.text) + "'.");
    }
    else if (pos.access == accessFix)
        REPORT_ERROR(node->oper,
            "Cannot modify a fixed-value variable.");

    ui8 slot = *(pos.slot);
    if (node->prev) // Load the original value.
        code.addOp(OP_GET_VAR, pos.depth, previousReg, slot);
    code.addOp(node->oper.type == TOK_INCR ? OP_INCR : OP_DECR,
        pos.depth, slot);
    if (!node->prev) // Load the new value.
        code.addOp(OP_GET_VAR, pos.depth, previousReg, slot);

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
        case TOK_TILDE: op = OP_COMP;       break;
        default: UNREACHABLE();
    }

    if (op == OP_COMP) // OP_COMP requires a depth argument.
        code.addOp(op, depth, firstOper);
    else
        code.addOp(op, firstOper);
    // We don't free a register since unary
    // operators don't use any extra registers.
    // They apply an operator directly onto a
    // register.
}

DEF(CallExpr)
{
    if (node->callee == nullptr) return;
    ui8 location, depth;

    if (node->builtin)
    {   
        VarExpr* var = static_cast<VarExpr*>(node->callee.get());
        auto find = Natives::builtins.find(var->name.text);
        if (find == Natives::builtins.end())
            REPORT_ERROR(var->name, "No builtin '"
                + std::string(var->name.text) + "' function.");
        location = static_cast<ui8>(find->second);
    }
    else if (node->callee->type == E_VAR_EXPR)
    {
        VarExpr* var = static_cast<VarExpr*>(node->callee.get());
        VarInfo pos = getVarInfo(var->name);
        if (pos.slot == nullptr)
            REPORT_ERROR(var->name, "Undefined variable '"
                + std::string(var->name.text) + "'.");
        location = *(pos.slot);
        depth = pos.depth;
    }
    else
    {
        location = previousReg;
        depth = this->depth;
        compileExpr(node->callee);
    }

    ui8 argsStart = previousReg;
    for (ExprUP& arg : node->args)
        compileExpr(arg);

    ui8 size = static_cast<ui8>(node->args.size());
    if (node->builtin)
        code.addOp(OP_CALL_NAT, location, argsStart, size);
    else
        code.addOp(OP_CALL_DEF, depth, location, argsStart, size);

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

DEF(LambdaExpr)
{
    size_t size = node->params.size();
    if (size > PARAMETER_MAX)
    {
        REPORT_ERROR(node->params[PARAMETER_MAX],
            "Too many parameters in lambda.");
    }

    ASTCompiler miniCompiler(this);
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
    funcCode.depth = miniCompiler.depth;
    this->hitError = miniCompiler.hitError;
    Object func = ALLOC(Function, ObjDealloc<Function>, funcCode,
        size);
    code.loadRegConst(func, previousReg, depth);
    reserveReg();
}

DEF(ListExpr)
{
    ui8 listReg = previousReg;
    code.addOp(OP_LIST, listReg);
    reserveReg();

    ui8 count = 0;
    ui8 startReg = previousReg;
    auto emitList = [this, listReg, &count, startReg] {
        code.addOp(OP_EXT_LIST, listReg, startReg, count);
        previousReg = startReg;
        count = 0;
    };

    for (ExprUP& entry : node->entries)
    {
        compileExpr(entry);
        if (++count == LIST_ENTRY_GROUP)
            emitList();
    }

    if (count > 0) emitList();
}

DEF(VarExpr)
{
    VarInfo pos = getVarInfo(node->name);
    if (pos.slot == nullptr)
        REPORT_ERROR(node->name, "Undefined variable '"
            + std::string(node->name.text) + "'.");
    code.addOp(OP_GET_VAR, pos.depth, previousReg, *(pos.slot));
    reserveReg();
}

DEF(LiteralExpr)
{
    Token tok = node->value;

    if (tok.type == TOK_NUM)
    {
        Object obj = tok.content.i;
        code.loadRegConst(obj, previousReg, depth);
        reserveReg();
    }

    else if (tok.type == TOK_NUM_DEC)
    {
        Object obj = tok.content.d;
        code.loadRegConst(obj, previousReg, depth);
        reserveReg();
    }

    else if (tok.type == TOK_STR_LIT)
    {
        Object obj = ALLOC(String, ObjDealloc<String>, GET_STR(tok));
        code.loadRegConst(obj, previousReg, depth);
        reserveReg();
    }

    else if (tok.type == TOK_RANGE)
    {
        Object obj = ALLOC(Range, ObjDealloc<Range>, constructRange(tok.text));
        code.loadRegConst(obj, previousReg, depth);
        reserveReg();
    }

    else if ((tok.type == TOK_TRUE) || (tok.type == TOK_FALSE))
    {
        bool value = tok.content.b;
        code.loadReg(previousReg, (value ? OP_TRUE : OP_FALSE), depth);
        reserveReg();
    }

    else if (tok.type == TOK_NULL)
    {
        code.loadReg(previousReg, OP_NULL, depth);
        reserveReg();
    }
}

void ASTCompiler::compileExpr(ExprUP& node)
{
    if (node == nullptr) return;
    
    switch (node->type)
    {
        case E_TUPLE_EXPR:      COMPILE(TupleExpr);     break;
        case E_ASSIGN_EXPR:     COMPILE(AssignExpr);    break;
        case E_LOGIC_EXPR:      COMPILE(LogicExpr);     break;
        case E_COMPARE_EXPR:    COMPILE(CompareExpr);   break;
        case E_BIT_EXPR:        COMPILE(BitExpr);       break;
        case E_SHIFT_EXPR:      COMPILE(ShiftExpr);     break;
        case E_BINARY_EXPR:     COMPILE(BinaryExpr);    break;
        case E_UNARY_EXPR:      COMPILE(UnaryExpr);     break;
        case E_CALL_EXPR:       COMPILE(CallExpr);      break;
        case E_IF_EXPR:         COMPILE(IfExpr);        break;
        case E_LAMBDA_EXPR:     COMPILE(LambdaExpr);    break;
        case E_LIST_EXPR:       COMPILE(ListExpr);      break;
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