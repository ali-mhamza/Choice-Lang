#include "../include/astcompiler.h"
#include "../include/astnodes.h"
#include "../include/common.h"
#include "../include/config.h"
#include "../include/error.h"
#include "../include/escape_seq.h"
#include "../include/linear_alloc.h"
#include "../include/natives.h"
#include "../include/object.h"
#include "../include/opcodes.h"
#include "../include/token.h"
#include "../include/utils.h"
#include <cctype>
#include <limits>
#include <vector>

using namespace AST::Statement;
using namespace AST::Expression;

/* General macros. */

#define DEF(type) void ASTCompiler::compile##type(const type* node)
#define COMPILE(type)                                   \
    do {                                                \
        auto* ptr = static_cast<type*>(node.get());     \
        compile##type(ptr);                             \
    } while (false)

#define REPORT_ERROR(...)                                           \
    do {                                                            \
        hitError = true;                                            \
        if (errorCount > COMPILE_ERROR_MAX) return;                 \
        if (errorCount == COMPILE_ERROR_MAX)                        \
            CH_PRINT("COMPILATION ERROR MAXIMUM REACHED.\n");       \
        else                                                        \
            CompileError{__VA_ARGS__}.report();                     \
        errorCount++;                                               \
        return;                                                     \
    } while (false)

#define REPORT_ERROR_NO_RETURN(...)                                 \
    do {                                                            \
        hitError = true;                                            \
        if (errorCount == COMPILE_ERROR_MAX)                        \
            CH_PRINT("COMPILATION ERROR MAXIMUM REACHED.\n");       \
        else if (errorCount < COMPILE_ERROR_MAX)                    \
            CompileError{__VA_ARGS__}.report();                     \
        errorCount++;                                               \
    } while (false)

constexpr bool accessFix{false};
constexpr bool accessVar{true};

constexpr bool getVar{true};
constexpr bool setVar{false};

/* Constructors/destructors. */

ASTCompiler::ASTCompiler(ASTCompiler* comp) :
    scopeCompiler{comp},
    depth{static_cast<ui8>(comp == nullptr ? 0 : comp->depth + 1)}
{
    if (depth == 0) // Global scope compiler.
    {
        for (const auto* func : Natives::funcNames)
            defVar(func, previousReg++, accessFix); // For now.
    }
}

ASTCompiler::~ASTCompiler() = default;

/* Compilation helpers. */

void ASTCompiler::addVariableOp(
    bool type,
    const VarInfo& info,
    ui8 dest,
    ui8 src
)
{
    if (info.type == GLOBAL)
    {
        code.addOp((type == getVar ? OP_GET_GLOBAL : OP_SET_GLOBAL),
            dest, src);
    }
    else if (info.type == LOCAL)
    {
        code.addOp((type == getVar ? OP_GET_LOCAL : OP_SET_LOCAL),
            dest, src);
    }
    else
    {
        code.addOp((type == getVar ? OP_GET_CELL : OP_SET_CELL),
            dest, src);
    }
}

// We pass an std::string instead of std::string_view
// since the line containing the variable's text will
// likely be destroyed soon after (if using the REPL),
// and thus we must take ownership of the string first
// to avoid invalidating the view.
void ASTCompiler::defVar(const std::string& name, ui8 reg, bool access)
{
    (*varLocations)[{ name, scope }] = reg;
    (*varAccess)[reg] = access;
    if (scope != 0) varScopes.top().push_back(name);
}

bool ASTCompiler::getAccess(ui8 reg) const
{
    bool* ret{varAccess->get(reg)};
    CH_ASSERT(ret != nullptr,
        "Variable registered with no access field.");
    return *ret;
}

ASTCompiler::LocalInfo ASTCompiler::getScopeLocal(const Token& token) const
{
    VarEntry entry{token.text, scope};
    ui8* slot{varLocations->get(entry)};
    if (slot != nullptr)
        return { true, *slot };

    return { false };
}

ASTCompiler::VarInfo ASTCompiler::resolveVariable(const Token& token)
{
    // Check if variable is local first.
    for (ui8 i{0}; i <= scope; i++)
    {
        VarEntry entry{token.text, static_cast<ui8>(scope - i)};
        ui8* slot{varLocations->get(entry)};
        if (slot != nullptr)
        {
            VarType type{LOCAL};
            if ((depth == 0) && (entry.scope == 0))
                type = GLOBAL;
            return { true, *slot, type, getAccess(*slot) };
        }
    }

    // Check enclosing non-global scopes.
    if (scopeCompiler != nullptr)
    {
        VarInfo info{scopeCompiler->resolveVariable(token)};
        if (info.found)
        {
            info.inCell = (info.type == CELL);
            info.slot = captureVariable(token, info);
            // Local variables in enclosing scopes become cells in
            // the current scope.
            if (info.type == LOCAL) info.type = CELL;
            return info;
        }
    }

    return { false };
}

ui8 ASTCompiler::captureVariable(const Token& token, const VarInfo& info)
{
    if (info.type == GLOBAL)
        return info.slot;

    std::string name{token.text};
    ui8* index{captureNames.get(name)};
    if (index != nullptr) // Already captured -> don't capture again.
        return *index;

    ui8 cellIndex{static_cast<ui8>(captures.size())};
    captureNames[name] = cellIndex;
    captures.push_back({ info.slot, info.inCell });
    return cellIndex;
}

void ASTCompiler::pushScope()
{
    scope++;
    scopeStart = previousReg;
    varScopes.emplace();
    code.addOp(OP_ENTER_SCOPE, scopeStart);
}

void ASTCompiler::popScope()
{
    auto& scopeVars{varScopes.top()};
    for (std::string& var : scopeVars)
        varLocations->remove({var, scope});

    varScopes.pop();
    scope--;
    previousReg = scopeStart;
    code.addOp(OP_EXIT_SCOPE);
}

void ASTCompiler::patchLoopLabelJumps(const Token& label, bool patchBreaks)
{
    if (label.type == TOK_EOF) return;

    if (patchBreaks)
    {
        auto* vec{breakLabels->get(label.text)};
        for (ui64 jump : *vec)
            code.patchJump(jump);
        // Breaks are always patched at the very end.
        breakLabels->remove(label.text);
        continueLabels->remove(label.text);
    }
    else
    {
        auto* vec{continueLabels->get(label.text)};
        for (ui64 jump : *vec)
            code.patchJump(jump);
    }
}

/* String helper. */

std::string ASTCompiler::parseStringToken(const Token& token)
{
	auto size{token.text.size() - 2};
	const auto text{token.text.substr(1, size)};
    auto it{text.begin()};
    auto end{text.end()};

    std::string str{};
	str.reserve(size);

    std::string errorMsg{};
    bool reportedError{false};

	while (it < end)
	{
		if ((*it == '\\') && (it < end - 1))
		{
            if (parseCharSequence(str, it, end)
                || parseNumericSequence(str, it, end, errorMsg)
                || parseUnicodeSequence(str, it, end, errorMsg))
            {
                continue;
            }
            else if (!reportedError && !errorMsg.empty())
            {
                REPORT_ERROR_NO_RETURN(token, errorMsg);
                reportedError = true;
            }
		}

		str.push_back(*it);
        it++;
	}

    return str;
}

/* AST node compilation functions. */

DEF(VarDecl)
{
    LocalInfo localInfo{getScopeLocal(node->name)};
    if (localInfo.found)
    {
        if (inRepl && (depth == 0) && (scope == 0))
        {
            ui8 reg{previousReg};
            if (node->init != nullptr)
            {
                compileExpr(node->init);
                // Always a local variable.
                code.addOp(OP_SET_LOCAL, localInfo.slot, reg);
            }
            else
                code.loadReg(localInfo.slot, OP_NULL);
            return;
        }

        REPORT_ERROR(node->name,
            "Variable '" + std::string(node->name.text)
            + "' is already defined in this scope.");
    }

    ui8 varSlot{previousReg};
    // Define first, since initializer could be a lambda
    // that references the variable.
    defVar(std::string(node->name.text), varSlot,
        node->declType == TOK_MAKE ? accessVar : accessFix);

    if (node->init != nullptr)
        compileExpr(node->init);
    else
    {
        code.loadReg(varSlot, OP_NULL);
        reserveReg();
    }
}

void ASTCompiler::funcBodyHelper(
    const vT& params,
    const StmtUP& body,
    const ui8 funcReg,
    const std::string& name
)
{
    ASTCompiler miniCompiler{this};
    for (const Token& param : params)
    {
        ui8 reg{miniCompiler.previousReg};
        LocalInfo info{miniCompiler.getScopeLocal(param)};
        if (info.found)
            REPORT_ERROR(param, "Parameter with the same name already in use.");
        miniCompiler.defVar(std::string(param.text), reg, accessVar);
        miniCompiler.reserveReg();
    }
    miniCompiler.compileStmt(body);
    miniCompiler.code.addOp(OP_VOID, 0);
    miniCompiler.code.addOp(OP_RETURN, 0);

    ByteCode& funcCode{miniCompiler.code};
    if (miniCompiler.hitError)
        this->hitError = true;

    Object func{};
    ui8 arity{static_cast<ui8>(params.size())};
    if (name.empty()) // Compiling a lambda.
        func = CH_ALLOC(Function, funcCode, arity);
    else
        func = CH_ALLOC(Function, name, funcCode, arity);

    // We only declare in the current function scope.
    code.loadRegConst(func, funcReg);
    if (!miniCompiler.captures.empty())
        code.addOp(OP_CLOSURE, funcReg);

    for (const auto& info : miniCompiler.captures)
    {
        // Capture object in register [slot] from enclosing scope,
        // or reuse the cell at index [slot] from enclosing scope.
        code.addOp((info.inCell ? OP_CAPTURE_CELL : OP_CAPTURE_VAL),
            funcReg, info.slot);
    }
}

DEF(FuncDecl)
{
    LocalInfo localInfo{getScopeLocal(node->name)};
    bool redefined{false};
    if (localInfo.found)
    {
        if (inRepl && (depth == 0) && (scope == 0))
            redefined = true;
        else
        {
            REPORT_ERROR(node->name,
                "Object '" + std::string(node->name.text)
                + "' is already defined in this scope.");
        }
    }

    // MAX_SCOPE_DEPTH involves block scopes as well.
    // Fix.
    if (depth + 1 == MAX_SCOPE_DEPTH)
        REPORT_ERROR(node->name, "Maximum function scope depth reached.");

    if (node->params.size() > PARAMETER_MAX)
    {
        REPORT_ERROR(node->params[PARAMETER_MAX],
            "Too many parameters in function.");
    }

    ui8 varSlot{redefined ? localInfo.slot : previousReg};
    std::string name{node->name.text};
    if (!redefined)
    {
        defVar(name, varSlot, accessFix); // Temporarily.
        reserveReg();
    }

    funcBodyHelper(node->params, node->body, varSlot, name);
}

DEF(ClassDecl) { (void) node; }

DEF(IfStmt)
{
    ui8 reg{previousReg};
    compileExpr(node->condition);
    ui64 falseJump{code.addJump(OP_JUMP_FALSE, reg)};
    freeReg();
    compileStmt(node->trueBranch);
    if (node->falseBranch != nullptr)
    {
        ui64 trueJump{code.addJump(OP_JUMP)};
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
    ui8 reg{previousReg};
    ui64 loopStart{code.getLoopStart()};
    if (node->label.type != TOK_EOF)
    {
        breakLabels->add(node->label.text, {});
        continueLabels->add(node->label.text, {});
    }

    std::vector<ui64> breaks{};
    auto* prevBreaks{breakJumps};
    breakJumps = &breaks;

    std::vector<ui64> continues{};
    auto* prevContinues{continueJumps};
    continueJumps = &continues;

    compileExpr(node->condition);
    ui64 falseJump{code.addJump(OP_JUMP_FALSE, reg)};
    freeReg();
    compileStmt(node->body);

    for (ui64 jump : continues)
        code.patchJump(jump);
    patchLoopLabelJumps(node->label, false);
    code.addLoop(loopStart);

    code.patchJump(falseJump);
    compileStmt(node->elseClause); // Will do nothing if elseClause == nullptr.

    for (ui64 jump : breaks)
        code.patchJump(jump);
    patchLoopLabelJumps(node->label, true);

    breakJumps = prevBreaks;
    continueJumps = prevContinues;
}

void ASTCompiler::forLoopHelper(
    const ForStmt* node,
    const ui8 varReg,
    const ui8 iterReg
)
{
    code.addOp(OP_MAKE_ITER, varReg, iterReg);
    ui64 failJump{code.addJump(OP_JUMP)}; // If we fail to construct an iterator.

    ui64 loopStart{code.getLoopStart()};
    ui64 whereJump{0};
    if (node->where != nullptr)
    {
        ui8 whereReg{previousReg};
        compileExpr(node->where);
        whereJump = code.addJump(OP_JUMP_FALSE, whereReg);
        freeReg();
    }

    compileStmt(node->body);

    if (whereJump != 0)
        code.patchJump(whereJump);
    for (ui64 jump : *continueJumps)
        code.patchJump(jump);
    patchLoopLabelJumps(node->label, false);

    constexpr int UPDATE_ITER_OP_SIZE{5};
    ui16 diff{static_cast<ui16>(code.codeSize() - loopStart
        + UPDATE_ITER_OP_SIZE)};
    code.addOp(OP_UPDATE_ITER, varReg, iterReg,
        static_cast<ui8>((diff >> CHAR_BIT) & CODE_MAX),
        static_cast<ui8>(diff & CODE_MAX)
    );

    code.patchJump(failJump);
    compileStmt(node->elseClause); // Will do nothing if elseClause == nullptr.

    for (ui64 jump : *breakJumps)
        code.patchJump(jump);
    patchLoopLabelJumps(node->label, true);
}

DEF(ForStmt)
{
    pushScope();
    if (node->label.type != TOK_EOF)
    {
        breakLabels->add(node->label.text, {});
        continueLabels->add(node->label.text, {});
    }

    std::vector<ui64> breaks{};
    auto* prevBreaks{breakJumps};
    breakJumps = &breaks;

    std::vector<ui64> continues{};
    auto* prevContinues{continueJumps};
    continueJumps = &continues;

    ui8 varReg{previousReg};
    defVar(std::string(node->var.text), varReg, accessFix); // For now.
    reserveReg();

    ui8 iterReg{previousReg};
    compileExpr(node->iter);

    forLoopHelper(node, varReg, iterReg);

    breakJumps = prevBreaks;
    continueJumps = prevContinues;

    popScope();
}

void ASTCompiler::matchCaseHelper(
    const MatchStmt::MatchCase& checkCase,
    const ui8 matchReg,
    ui64& fallJump,
    ui64& emptyJump
)
{
    ui8 caseReg{previousReg};
    compileExpr(checkCase.value);
    code.addOp(OP_EQUAL, caseReg, matchReg);
    ui64 falseJump{code.addJump(OP_JUMP_FALSE, caseReg)};
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
    else if (empty) // Default fallthrough with empty match blocks.
        emptyJump = code.addJump(OP_JUMP);
    else
        this->endJumps->push_back(code.addJump(OP_JUMP));

    code.patchJump(falseJump);
}

DEF(MatchStmt)
{
    ui8 matchReg{previousReg};
    compileExpr(node->matchValue);

    std::vector<ui64> jumps{};
    auto* prevEndJumps{endJumps};
    this->endJumps = &jumps;

    ui64 fallJump{0}; // Invalid jump offset value.
    ui64 emptyJump{0};

    for (const MatchStmt::MatchCase& checkCase : node->cases)
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
    ui64 loopStart{code.getLoopStart()};
    compileStmt(node->body);

    ui8 reg{previousReg};
    compileExpr(node->condition);
    ui64 trueJump{code.addJump(OP_JUMP_TRUE, reg)};
    freeReg();

    code.addLoop(loopStart);
    code.patchJump(trueJump);
}

DEF(ReturnStmt)
{
    ui8 reg{previousReg};
    if (node->expr != nullptr)
        compileExpr(node->expr);
    else
        code.addOp(OP_VOID, reg); // Return void value as default.
    code.addOp(OP_RETURN, reg);

    if (node->expr != nullptr) freeReg();
}

DEF(BreakStmt)
{
    if (node->label.type == TOK_EOF)
        this->breakJumps->push_back(code.addJump(OP_JUMP));
    else
    {
        auto* vec{breakLabels->get(node->label.text)};
        if (vec == nullptr)
        {
            REPORT_ERROR(node->label,
                "Break label is not assigned to any loop.");
        }
        else
            vec->push_back(code.addJump(OP_JUMP));
    }
}

DEF(ContinueStmt)
{
    if (node->label.type == TOK_EOF)
        this->continueJumps->push_back(code.addJump(OP_JUMP));
    else
    {
        auto* vec{continueLabels->get(node->label.text)};
        if (vec == nullptr)
        {
            REPORT_ERROR(node->label,
                "Continue label is not assigned to any loop.");
        }
        else
            vec->push_back(code.addJump(OP_JUMP));
    }
}

DEF(EndStmt)
{
    (void) node;
    this->endJumps->push_back(code.addJump(OP_JUMP));
}

DEF(ExprStmt)
{
    if (node->expr == nullptr) return;
    
    ui8 reg{previousReg};
    compileExpr(node->expr);
    if (inRepl && (node->expr->type != E_ASSIGN_EXPR))
        code.addOp(OP_PRINT_VALID, reg);
    freeReg();
}

DEF(BlockStmt)
{
    pushScope();
    for (const StmtUP& stmt : node->block)
        compileStmt(stmt);
    popScope();
}

DEF(TupleExpr)
{
    constexpr int TUPLE_GROUP{5};
    
    ui8 tupleReg{previousReg};
    code.addOp(OP_TUPLE, tupleReg);
    reserveReg();

    ui8 count{0};
    ui8 startReg{previousReg};
    auto emitTuple = [this, tupleReg, &count, startReg] {
        code.addOp(OP_EXT_TUPLE, tupleReg, startReg, count);
        previousReg = startReg;
        count = 0;
    };

    for (const ExprUP& entry : node->entries)
    {
        compileExpr(entry);
        if (++count == TUPLE_GROUP)
            emitTuple();
    }

    if (count > 0) emitTuple();
}

void ASTCompiler::compoundAssign(
    const AssignExpr* node,
    const VarInfo& info
)
{
    ui8 varReg{previousReg};
    addVariableOp(getVar, info, varReg, info.slot);
    reserveReg();

    ui8 valueReg{previousReg};
    compileExpr(node->value);

    Opcode op{};
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
        default: CH_UNREACHABLE();
    }

    code.addOp(op, varReg, valueReg);
    addVariableOp(setVar, info, info.slot, varReg);
    freeReg(); // Free the temporary register used for the RHS value.
}

DEF(AssignExpr)
{
    // Temporarily assuming regular variables only.
    auto* temp{static_cast<VarExpr*>(node->target.get())};
    VarInfo info{resolveVariable(temp->name)};

    if (!info.found)
    {
        REPORT_ERROR(temp->name, "Undefined variable '"
            + std::string(temp->name.text) + "'.");
    }
    else if (info.access == accessFix)
        REPORT_ERROR(node->oper, "Cannot assign to a fixed-value variable.");

    if (node->oper.type != TOK_EQUAL)
    {
        compoundAssign(node, info);
        return;
    }

    ui8 reg{previousReg};
    compileExpr(node->value);
    addVariableOp(setVar, info, info.slot, reg);
}

DEF(LogicExpr)
{
    if ((node->oper == TOK_AMP_AMP) || (node->oper == TOK_AND)) // &&, and
    {
        ui8 reg{previousReg};
        compileExpr(node->left);
        ui64 falseJump{code.addJump(OP_JUMP_FALSE, reg)};
        previousReg = reg;
        compileExpr(node->right);
        code.patchJump(falseJump);
    }
    else if ((node->oper == TOK_BAR_BAR) || (node->oper == TOK_OR)) // ||, or
    {
        ui8 reg{previousReg};
        compileExpr(node->left);
        ui64 trueJump{code.addJump(OP_JUMP_TRUE, reg)};
        previousReg = reg;
        compileExpr(node->right);
        code.patchJump(trueJump);
    }
}

DEF(CompareExpr)
{
    ui8 firstOper{previousReg};
    compileExpr(node->left);

    ui8 secondOper{previousReg};
    compileExpr(node->right);

    Opcode op{};
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
        default: CH_UNREACHABLE();
    }

    code.addOp(op, firstOper, secondOper);
    if ((node->oper == TOK_NOT) || (node->oper == TOK_GT_EQ)
        || (node->oper == TOK_LT_EQ) || (node->oper == TOK_BANG_EQ))
            code.addOp(OP_NOT, firstOper);
    freeReg();
}

DEF(BitExpr)
{
    ui8 firstOper{previousReg};
    compileExpr(node->left);

    ui8 secondOper{previousReg};
    compileExpr(node->right);

    Opcode op{};
    switch (node->oper)
    {
        case TOK_AMP:       op = OP_AND;    break;
        case TOK_BAR:       op = OP_OR;     break;
        case TOK_UARROW:    op = OP_XOR;    break;
        default: CH_UNREACHABLE();
    }

    code.addOp(op, firstOper, secondOper);
    freeReg();
}

DEF(ShiftExpr)
{
    ui8 firstOper{previousReg};
    compileExpr(node->left);

    ui8 secondOper{previousReg};
    compileExpr(node->right);

    code.addOp(node->oper == TOK_RIGHT_SHIFT ?
        OP_SHIFT_R : OP_SHIFT_L, firstOper, secondOper);
    freeReg();
}

DEF(BinaryExpr)
{
    ui8 firstOper{previousReg};
    compileExpr(node->left);

    ui8 secondOper{previousReg};
    compileExpr(node->right);

    Opcode op{};
    switch (node->oper)
    {
        case TOK_PLUS:      op = OP_ADD;    break;
        case TOK_MINUS:     op = OP_SUB;    break;
        case TOK_STAR:      op = OP_MULT;   break;
        case TOK_SLASH:     op = OP_DIV;    break;
        case TOK_PERCENT:   op = OP_MOD;    break;
        case TOK_STAR_STAR: op = OP_POWER;  break;
        default: CH_UNREACHABLE();
    }

    code.addOp(op, firstOper, secondOper);
    freeReg();
}

// Temporarily only dealing with simple identifier
// variables; will need to extend later.

void ASTCompiler::_crementExpr(const UnaryExpr* node)
{
    if (node->expr->type != E_VAR_EXPR)
        REPORT_ERROR(node->oper, "Invalid increment/decrement target.");

    auto* temp{static_cast<VarExpr*>(node->expr.get())};
    VarInfo info{resolveVariable(temp->name)};
    if (!info.found)
    {
        REPORT_ERROR(temp->name, "Undefined variable '"
            + std::string(temp->name.text) + "'.");
    }
    else if (info.access == accessFix)
        REPORT_ERROR(node->oper, "Cannot modify a fixed-value variable.");

    // We copy the variable into two temporary register slots:
    // [x][.][.][.][...] -> [...][x][x]
    // We then increment/decrement the second register:
    // [x][x + 1/x - 1]
    // We then store the new value in the variable's location:
    // [x + 1/x - 1][...][...][x][x + 1/x - 1]
    // For post-increment, we move the value in the second
    // register into the first one:
    // [x + 1/x - 1][x + 1/x - 1]
    // For pre-increment, we do nothing (previous value is in
    // the correct location).
    // In both cases, the result ends up in the first register,
    // which is the only reserved register.

    addVariableOp(getVar, info, previousReg, info.slot);
    reserveReg();

    addVariableOp(getVar, info, previousReg, info.slot);
    code.addOp((node->oper.type == TOK_INCR ?
        OP_INCR : OP_DECR), previousReg);
    addVariableOp(setVar, info, info.slot, previousReg);

    if (!node->prev)
    {
        code.addOp(OP_MOVE_R, static_cast<ui8>(previousReg - 1),
            previousReg);
    }
}

DEF(UnaryExpr)
{
    if ((node->oper.type == TOK_INCR) || (node->oper.type == TOK_DECR))
    {
        _crementExpr(node);
        return;
    }

    ui8 firstOper{previousReg};
    compileExpr(node->expr);

    Opcode op{};
    switch (node->oper.type)
    {
        case TOK_MINUS: op = OP_NEG;        break;
        case TOK_BANG:
        case TOK_NOT:   op = OP_NOT;        break;
        case TOK_TILDE: op = OP_COMP;       break;
        default: CH_UNREACHABLE();
    }

    code.addOp(op, firstOper);
    // We don't free a register since unary
    // operators don't use any extra registers.
    // They apply an operator directly onto a
    // register.
}

DEF(CallExpr)
{
    if (node->callee == nullptr) return;

    ui8 location{};
    if (node->builtin)
    {
        auto* var{static_cast<VarExpr*>(node->callee.get())};
        auto find{Natives::builtins.find(var->name.text)};
        if (find == Natives::builtins.end())
        {
            REPORT_ERROR(var->name, "No builtin '"
                + std::string(var->name.text) + "' function.");
        }
        location = static_cast<ui8>(find->second);
        reserveReg(); // Reserve a register in place of the function object.
    }
    else
    {
        location = previousReg;
        compileExpr(node->callee); // Will reserve a register.
    }

    ui8 argsStart{previousReg};
    for (const ExprUP& arg : node->args)
        compileExpr(arg);

    ui8 size{static_cast<ui8>(node->args.size())};
    code.addOp((node->builtin ? OP_CALL_NAT : OP_CALL_DEF),
        location, argsStart, size);

    // For user-defined functions, the return value replaces the
    // function object.
    // For built-ins, we place the return value in the empty register
    // reserved above.
    previousReg -= size;
}

DEF(IfExpr)
{
    ui8 reg{previousReg};
    compileExpr(node->condition);
    ui64 falseJump{code.addJump(OP_JUMP_FALSE, reg)};
    freeReg();

    ui8 current{previousReg};
    compileExpr(node->trueExpr);
    ui64 trueJump{code.addJump(OP_JUMP)};
    code.patchJump(falseJump);

    previousReg = current;
    compileExpr(node->falseExpr);
    code.patchJump(trueJump);
}

DEF(LambdaExpr)
{
    if (node->params.size() > PARAMETER_MAX)
    {
        REPORT_ERROR(node->params[PARAMETER_MAX],
            "Too many parameters in lambda.");
    }

    funcBodyHelper(node->params, node->body, previousReg, 
        std::string());
    reserveReg();
}

DEF(ComprehensionExpr)
{
    ui8 listReg{previousReg};
    code.addOp(OP_LIST, listReg);
    reserveReg();

    pushScope();
    ui8 varReg{previousReg};
    defVar(std::string(node->var.text), varReg, accessFix); // For now.
    reserveReg();

    ui8 iterReg{previousReg};
    compileExpr(node->iter);

    code.addOp(OP_MAKE_ITER, varReg, iterReg);
    ui64 failJump{code.addJump(OP_JUMP)}; // If we fail to construct an iterator.

    ui64 loopStart{code.getLoopStart()};
    ui64 whereJump{0};
    if (node->where != nullptr)
    {
        ui8 whereReg{previousReg};
        compileExpr(node->where);
        whereJump = code.addJump(OP_JUMP_FALSE, whereReg);
        freeReg();
    }

    ui8 result{previousReg};
    compileExpr(node->expr);
    code.addOp(OP_EXT_LIST, listReg, result, ui8(1));

    if (whereJump != 0)
        code.patchJump(whereJump);

    constexpr int UPDATE_ITER_OP_SIZE{5};
    ui16 diff{static_cast<ui16>(code.codeSize() - loopStart
        + UPDATE_ITER_OP_SIZE)};
    code.addOp(OP_UPDATE_ITER, varReg, iterReg,
        static_cast<ui8>((diff >> CHAR_BIT) & CODE_MAX),
        static_cast<ui8>(diff & CODE_MAX)
    );

    code.patchJump(failJump);
    popScope();
}

DEF(ListExpr)
{
    ui8 listReg{previousReg};
    code.addOp(OP_LIST, listReg);
    reserveReg();

    ui8 count{0};
    ui8 startReg{previousReg};
    auto emitList = [this, listReg, &count, startReg] {
        code.addOp(OP_EXT_LIST, listReg, startReg, count);
        previousReg = startReg;
        count = 0;
    };

    for (const ExprUP& entry : node->entries)
    {
        compileExpr(entry);
        if (++count == LIST_ENTRY_GROUP)
            emitList();
    }

    if (count > 0) emitList();
}

DEF(VarExpr)
{
    VarInfo info{resolveVariable(node->name)};
    if (!info.found)
    {
        REPORT_ERROR(node->name, "Undefined variable '"
            + std::string(node->name.text) + "'.");
    }

    addVariableOp(getVar, info, previousReg, info.slot);
    reserveReg();
}

DEF(LiteralExpr)
{
    #define GET_RAW_STR(tok) \
        std::string{tok.text.substr(2, tok.text.size() - 3)}

    const Token& tok{node->value};

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
        Object obj{CH_ALLOC(String, parseStringToken(tok))};
        code.loadRegConst(obj, previousReg);
        reserveReg();
    }

    else if (tok.type == TOK_RAW_STR)
    {
        Object obj{CH_ALLOC(String, GET_RAW_STR(tok))};
        code.loadRegConst(obj, previousReg);
        reserveReg();
    }

    else if (tok.type == TOK_RANGE)
    {
        Object obj{CH_ALLOC(Range, constructRange(tok.text))};
        code.loadRegConst(obj, previousReg);
        reserveReg();
    }

    else if ((tok.type == TOK_TRUE) || (tok.type == TOK_FALSE))
    {
        bool value{tok.content.b};
        code.loadReg(previousReg, (value ? OP_TRUE : OP_FALSE));
        reserveReg();
    }

    else if (tok.type == TOK_NULL)
    {
        code.loadReg(previousReg, OP_NULL);
        reserveReg();
    }

    #undef GET_STR
}

/* General compilation functions. */

void ASTCompiler::compileExpr(const ExprUP& node)
{
    if (node == nullptr) return;
    
    switch (node->type)
    {
        case E_TUPLE_EXPR:      COMPILE(TupleExpr);         break;
        case E_ASSIGN_EXPR:     COMPILE(AssignExpr);        break;
        case E_LOGIC_EXPR:      COMPILE(LogicExpr);         break;
        case E_COMPARE_EXPR:    COMPILE(CompareExpr);       break;
        case E_BIT_EXPR:        COMPILE(BitExpr);           break;
        case E_SHIFT_EXPR:      COMPILE(ShiftExpr);         break;
        case E_BINARY_EXPR:     COMPILE(BinaryExpr);        break;
        case E_UNARY_EXPR:      COMPILE(UnaryExpr);         break;
        case E_CALL_EXPR:       COMPILE(CallExpr);          break;
        case E_IF_EXPR:         COMPILE(IfExpr);            break;
        case E_LAMBDA_EXPR:     COMPILE(LambdaExpr);        break;
        case E_COMPREHEN_EXPR:  COMPILE(ComprehensionExpr); break;
        case E_LIST_EXPR:       COMPILE(ListExpr);          break;
        case E_VAR_EXPR:        COMPILE(VarExpr);           break;
        case E_LITERAL_EXPR:    COMPILE(LiteralExpr);       break;
    }
}

void ASTCompiler::compileStmt(const StmtUP& node)
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

Function* ASTCompiler::compile(const StmtVec& program)
{
    code.clear();
    // Inherit hitError and errorCount from parser.

    for (const StmtUP& node : program)
        compileStmt(node);

    if (hitError) code.clear();
    return CH_ALLOC(Function, code, 0);
}

#undef DEF
#undef COMPILE
#undef REPORT_ERROR
#undef REPORT_ERROR_NO_RETURN