#include "../include/astcompiler.h"
#include "../include/astnodes.h"
#include "../include/common.h"
#include "../include/config.h"
#include "../include/diagnostic.h"
#include "../include/escape_seq.h"
#include "../include/linear_alloc.h"
#include "../include/natives.h"
#include "../include/object.h"
#include "../include/opcodes.h"
#include "../include/token.h"
#include "../include/utils.h"
#include <climits>
#include <string_view>
#include <utility>
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

#define REPORT_ERROR(...)           \
    do {                            \
        reportError(__VA_ARGS__);   \
        return;                     \
    } while (false)

constexpr bool accessFix{false};
constexpr bool accessVar{true};

constexpr bool getVar{true};
constexpr bool setVar{false};

/* Constructors/destructors. */

ASTCompiler::ASTCompiler(ASTCompiler* comp) :
    scopeCompiler{comp},
    depth{static_cast<u8>(comp == nullptr ? 0 : comp->depth + 1)}
{
    if (depth == 0) // Global scope compiler.
    {
        for (const auto* func : Natives::funcNames)
            defVar(func, nextReg++, accessFix); // For now.
    }
    else
        this->id = scopeCompiler->id;
}

ASTCompiler::~ASTCompiler() = default;

std::vector<ASTCompiler::DeclarationPair> ASTCompiler::declaredVars{};
bool ASTCompiler::clearDeclaredVars{false};
u8 ASTCompiler::clearIndex{0};

/* Compilation helpers. */

void ASTCompiler::emitVariableOp(bool type, const VarInfo& info, u8 dest, u8 src)
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
void ASTCompiler::defVar(const std::string& name, u8 reg, bool access)
{
    (*varLocations)[{ name, scope }] = reg;
    (*varAccess)[reg] = access;

    if (scope != 0)
        varScopes.top().push_back(name);
    else if (inRepl && (depth == 0) && (scope == 0))
        declaredVars.push_back({ name, reg });
}

void ASTCompiler::removeVar(const std::string& name, u8 reg)
{
    varLocations->remove({ name, scope });
    varAccess->remove(reg);

    if (scope != 0)
        varScopes.top().pop_back();
    else if (inRepl && (depth == 0) && (scope == 0))
        declaredVars.pop_back();
}

void ASTCompiler::clearDeclarations()
{
    if (clearDeclaredVars)
    {
        while (clearIndex < declaredVars.size())
        {
            const auto& pair{declaredVars[clearIndex++]};

            varLocations->remove({ pair.name, scope });
            varAccess->remove(pair.reg);
        }
    }

    declaredVars.clear();
    clearDeclaredVars = false;
    clearIndex = 0;
}

bool ASTCompiler::getAccess(u8 reg) const
{
    bool* ret{varAccess->get(reg)};
    CH_ASSERT(ret != nullptr,
        "Variable registered with no access field.");
    return *ret;
}

ASTCompiler::LocalInfo ASTCompiler::getScopeLocal(const Token& token) const
{
    VarEntry entry{token.text, scope};
    u8* slot{varLocations->get(entry)};
    if (slot != nullptr)
        return { true, *slot };

    return { false };
}

ASTCompiler::VarInfo ASTCompiler::resolveVariable(const Token& token)
{
    // Check if variable is local first.
    for (u8 i{0}; i <= scope; i++)
    {
        VarEntry entry{token.text, static_cast<u8>(scope - i)};
        u8* slot{varLocations->get(entry)};
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

u8 ASTCompiler::captureVariable(const Token& token, const VarInfo& info)
{
    if (info.type == GLOBAL)
        return info.slot;

    std::string name{token.text};
    u8* index{captureNames.get(name)};
    if (index != nullptr) // Already captured -> don't capture again.
        return *index;

    u8 cellIndex{static_cast<u8>(captures.size())};
    captureNames[name] = cellIndex;
    captures.push_back({ info.slot, info.inCell });
    return cellIndex;
}

void ASTCompiler::pushScope()
{
    scope++;
    scopeStart = nextReg;
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
    nextReg = scopeStart;
    code.addOp(OP_EXIT_SCOPE);
}

void ASTCompiler::startDeclaration()
{
    if (inRepl) code.addOp(OP_DEF_START, declaredVars.size());
}

void ASTCompiler::endDeclaration()
{
    if (inRepl) code.addOp(OP_DEF_END);
}

void ASTCompiler::patchLoopLabelJumps(const Token& label, bool patchBreaks)
{
    if (label.type == TOK_EOF) return;

    if (patchBreaks)
    {
        auto* vec{breakLabels->get(label.text)};
        for (u64 jump : *vec)
            code.patchJump(jump);
        // Breaks are always patched at the very end.
        breakLabels->remove(label.text);
        continueLabels->remove(label.text);
    }
    else
    {
        auto* vec{continueLabels->get(label.text)};
        for (u64 jump : *vec)
            code.patchJump(jump);
    }
}

/* String helper. */

std::string ASTCompiler::parseStringToken(
    const Token& token,
    size_t start,
    size_t offset
)
{
    #define REPORT(CURRENT, OFF, LEN)                                   \
        reportPartError(pair.first, token,                              \
            start + static_cast<u64>((CURRENT) - text.begin() - (OFF)), \
            (LEN), pair.second                                          \
        )

	auto size{token.text.size() - offset};
    if (size == 0) return std::string{}; // Empty string.

	const auto text{token.text.substr(start, size)};
    auto it{text.begin()};
    auto end{text.end()};

    std::string str{};
	str.reserve(size);

    // Skip leading or trailing newlines.
    if (it[0] == '\n') it++;
    if (end[-1] == '\n') end--;

    bool reportedError{false};
    ErrorPair pair{std::make_pair(static_cast<DiagCode>(0), "")};
    auto current{it};

    // Keep as inequality check in case 'end' becomes before 'it'.
	while (it < end)
	{
	    current = it;
		if ((*it == '\\') && (it < end - 1))
		{
            if (parseCharSequence(str, it, end)
                || parseNumericSequence(str, it, end, pair)
                || parseUnicodeSequence(str, it, end, pair))
            {
                continue;
            }
            else if (!reportedError && (static_cast<u8>(pair.first) != 0))
            {
                if (pair.first == HIT_OCTAL_CHAR_MAX)
                    REPORT(it, 3, 3);
                else if (pair.first == INVALID_UTF_CODEPOINT)
                    REPORT(current + 3, 0, it - (current + 3) - 1);
                else
                    REPORT(it, 0, 1);
                reportedError = true;
            }
		}

		str.push_back(*it);
        it++;
	}

    return str;

    #undef REPORT
}

/* Error reporting. */

void ASTCompiler::reportError(
    DiagCode code,
    const Token& token,
    std::string_view message
)
{
    diagEngine.source = ErrorSource::COMPILER;
    hitError = true;
    if ((code == WRONG_TOKEN_FOUND) || (code == WRONG_CHAR_FOUND))
        code = (token.type == TOK_EOF) ? UNEXPECTED_INPUT_END : code;
    diagEngine.recordError(id, code, token, std::string{message});
}

void ASTCompiler::reportPart(
    bool isError,
    DiagCode code,
    u64 offset,
    u64 length,
    std::string_view message
)
{
    diagEngine.source = ErrorSource::COMPILER;

    if (isError)
        diagEngine.recordError(id, code, offset, length, std::string{message});
    else
        diagEngine.recordWarning(id, code, offset, length, std::string{message});
}

void ASTCompiler::reportPartError(
    DiagCode code,
    const Token& token,
    u64 offset,
    u64 length,
    std::string_view message
)
{
    hitError = true;
    if ((code == WRONG_TOKEN_FOUND) || (code == WRONG_CHAR_FOUND))
        code = (token.type == TOK_EOF) ? UNEXPECTED_INPUT_END : code;
    reportPart(true, code, token.byteOffset + offset, length, message);
}

/* AST node compilation functions. */

DEF(VarDecl)
{
    startDeclaration();

    LocalInfo localInfo{getScopeLocal(node->name)};
    if (localInfo.found)
    {
        if (inRepl && (depth == 0) && (scope == 0))
        {
            u8 reg{nextReg};
            if (node->init != nullptr)
            {
                compileExpr(node->init);
                // Always a local variable.
                code.addOp(OP_SET_LOCAL, localInfo.slot, reg);
            }
            else
                code.loadReg(localInfo.slot, OP_NULL);
            endDeclaration();
            return;
        }

        REPORT_ERROR(VAR_ALREADY_DEFINED, node->name);
    }

    std::string varName{node->name.text};
    u8 varSlot{nextReg};
    // Define first, since initializer could be a lambda
    // that references the variable.
    defVar(varName, varSlot,
        node->declType == TOK_MAKE ? accessVar : accessFix);

    bool inError{hitError};
    if (node->init != nullptr)
        compileExpr(node->init);
    else
    {
        code.loadReg(varSlot, OP_NULL);
        reserveReg();
    }

    if (!inError && hitError) removeVar(varName, varSlot);
    endDeclaration();
}

void ASTCompiler::funcBodyHelper(
    const vT& params,
    const StmtUP& body,
    const u8 funcReg,
    const std::string& name
)
{
    ASTCompiler miniCompiler{this};
    // The number of "parameter" tokens that aren't identifiers.
    u8 removeCount{0};
    for (auto it{params.begin()}; it != params.end(); it++)
    {
        bool access{accessVar};
        if (it->type == TOK_FIX)
        {
            access = accessFix;
            removeCount++;
            it++;
        }

        const Token& param{*it};
        u8 reg{miniCompiler.nextReg};
        LocalInfo info{miniCompiler.getScopeLocal(param)};
        if (info.found)
            REPORT_ERROR(PARAM_ALREADY_DEFINED, param);
        miniCompiler.defVar(std::string(param.text), reg, access);
        miniCompiler.reserveReg();
    }
    miniCompiler.compileStmt(body);
    miniCompiler.code.addOp(OP_VOID, 0);
    miniCompiler.code.addOp(OP_RETURN, 0);

    ByteCode& funcCode{miniCompiler.getCode()};
    if (miniCompiler.hitError)
        this->hitError = true;

    Object func{};
    u8 arity{static_cast<u8>(params.size() - removeCount)};
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
    startDeclaration();

    LocalInfo localInfo{getScopeLocal(node->name)};
    bool redefined{false};
    if (localInfo.found)
    {
        if (inRepl && (depth == 0) && (scope == 0))
            redefined = true;
        else
            REPORT_ERROR(FUNC_ALREADY_DEFINED, node->name);
    }

    // MAX_SCOPE_DEPTH involves block scopes as well.
    // Fix.
    if (depth + 1 == MAX_SCOPE_DEPTH)
        REPORT_ERROR(HIT_SCOPE_MAX, node->name);

    if (node->params.size() > PARAMETER_MAX)
        REPORT_ERROR(HIT_PARAM_MAX, node->params[PARAMETER_MAX]);

    u8 varSlot{redefined ? localInfo.slot : nextReg};
    std::string name{node->name.text};
    if (!redefined)
    {
        defVar(name, varSlot, accessFix); // Temporarily.
        reserveReg();
    }

    bool inError{hitError};
    funcBodyHelper(node->params, node->body, varSlot, name);
    if (!inError && hitError) removeVar(name, varSlot);
    endDeclaration();
}

DEF(ClassDecl) { (void) node; }

DEF(IfStmt)
{
    u8 reg{nextReg};
    compileExpr(node->condition);
    u64 falseJump{code.addJump(OP_JUMP_FALSE, reg)};
    freeReg();
    compileStmt(node->trueBranch);
    if (node->falseBranch != nullptr)
    {
        u64 trueJump{code.addJump(OP_JUMP)};
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
    u8 reg{nextReg};
    u64 loopStart{code.getLoopStart()};
    if (node->label.type != TOK_EOF)
    {
        breakLabels->add(node->label.text, {});
        continueLabels->add(node->label.text, {});
    }

    std::vector<u64> breaks{};
    auto* prevBreaks{breakJumps};
    breakJumps = &breaks;

    std::vector<u64> continues{};
    auto* prevContinues{continueJumps};
    continueJumps = &continues;

    compileExpr(node->condition);
    u64 falseJump{code.addJump(OP_JUMP_FALSE, reg)};
    freeReg();
    compileStmt(node->body);

    // Patch current scope "continue" jumps.
    for (u64 jump : continues)
        code.patchJump(jump);

    // Patch nested scope "continue" jumps.
    patchLoopLabelJumps(node->label, false);
    code.addLoop(loopStart);

    code.patchJump(falseJump);
    compileStmt(node->elseClause); // Will do nothing if elseClause == nullptr.

    // Patch current scope "break" jumps.
    for (u64 jump : breaks)
        code.patchJump(jump);

    // Patch nested scope "break" jumps.
    patchLoopLabelJumps(node->label, true);

    breakJumps = prevBreaks;
    continueJumps = prevContinues;
}

void ASTCompiler::forLoopHelper(
    const ForStmt* node,
    const u8 varReg,
    const u8 iterReg
)
{
    code.addOp(OP_MAKE_ITER, varReg, iterReg);
    u64 failJump{code.addJump(OP_JUMP)}; // If we fail to construct an iterator.

    u64 loopStart{code.getLoopStart()};
    u64 whereJump{0};
    if (node->where != nullptr)
    {
        u8 whereReg{nextReg};
        compileExpr(node->where);
        whereJump = code.addJump(OP_JUMP_FALSE, whereReg);
        freeReg();
    }

    compileStmt(node->body);

    if (whereJump != 0)
        code.patchJump(whereJump);
    // Patch current scope "continue" jumps.
    for (u64 jump : *continueJumps)
        code.patchJump(jump);

    // Patch nested scope "continue" jumps.
    patchLoopLabelJumps(node->label, false);

    constexpr int UPDATE_ITER_OP_SIZE{5};
    u16 diff{static_cast<u16>(code.codeSize() - loopStart
        + UPDATE_ITER_OP_SIZE)};
    code.addOp(OP_UPDATE_ITER, varReg, iterReg,
        static_cast<u8>((diff >> CHAR_BIT) & CODE_MAX),
        static_cast<u8>(diff & CODE_MAX)
    );

    code.patchJump(failJump);
    compileStmt(node->elseClause); // Will do nothing if elseClause == nullptr.

    // Patch current scope "break" jumps.
    for (u64 jump : *breakJumps)
        code.patchJump(jump);

    // Patch nested scope "break" jumps.
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

    std::vector<u64> breaks{};
    auto* prevBreaks{breakJumps};
    breakJumps = &breaks;

    std::vector<u64> continues{};
    auto* prevContinues{continueJumps};
    continueJumps = &continues;

    u8 varReg{nextReg};
    defVar(std::string{node->var.text}, varReg, accessFix); // For now.
    reserveReg();

    u8 iterReg{nextReg};
    compileExpr(node->iter);

    forLoopHelper(node, varReg, iterReg);

    breakJumps = prevBreaks;
    continueJumps = prevContinues;

    popScope();
}

void ASTCompiler::matchCaseHelper(
    const MatchStmt::MatchCase& checkCase,
    const u8 matchReg,
    u64& fallJump,
    u64& emptyJump
)
{
    u8 caseReg{nextReg};
    compileExpr(checkCase.value);
    code.addOp(OP_EQUAL, caseReg, matchReg);
    u64 falseJump{code.addJump(OP_JUMP_FALSE, caseReg)};
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
        endJumps->push_back(code.addJump(OP_JUMP));

    code.patchJump(falseJump);
}

DEF(MatchStmt)
{
    u8 matchReg{nextReg};
    compileExpr(node->matchValue);

    std::vector<u64> jumps{};
    auto* prevEndJumps{endJumps};
    endJumps = &jumps;

    u64 fallJump{0}; // Invalid jump offset value.
    u64 emptyJump{0};

    for (const auto& checkCase : node->cases)
    {
        if (checkCase.value != nullptr)
            matchCaseHelper(checkCase, matchReg, fallJump, emptyJump);
        else // Default case.
            compileStmt(checkCase.body); // No need for any jumps.
    }

    for (u64 jump : jumps)
        code.patchJump(jump);
    freeReg(); // Remove the match value.

    endJumps = prevEndJumps;
}

DEF(RepeatStmt)
{
    if (node->label.type != TOK_EOF)
    {
        breakLabels->add(node->label.text, {});
        continueLabels->add(node->label.text, {});
    }

    std::vector<u64> breaks{};
    auto* prevBreaks{breakJumps};
    breakJumps = &breaks;

    std::vector<u64> continues{};
    auto* prevContinues{continueJumps};
    continueJumps = &continues;

    u64 loopStart{code.getLoopStart()};
    compileStmt(node->body);

    // Patch current scope "continue" jumps.
    for (u64 jump : continues)
        code.patchJump(jump);
    // Patch nested scope "continue" jumps.
    patchLoopLabelJumps(node->label, false);

    u8 reg{nextReg};
    compileExpr(node->condition);
    u64 trueJump{code.addJump(OP_JUMP_TRUE, reg)};
    freeReg();

    code.addLoop(loopStart);
    code.patchJump(trueJump);

    // Patch current scope "break" jumps.
    for (u64 jump : breaks)
        code.patchJump(jump);

    // Patch nested scope "break" jumps.
    patchLoopLabelJumps(node->label, true);

    breakJumps = prevBreaks;
    continueJumps = prevContinues;
}

DEF(ReturnStmt)
{
    u8 reg{nextReg};
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
            REPORT_ERROR(INVALID_LOOP_LABEL, node->label);
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
            REPORT_ERROR(INVALID_LOOP_LABEL, node->label);
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

    u8 reg{nextReg};
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

void ASTCompiler::compoundAssign(
    const AssignExpr* node,
    const VarInfo& info
)
{
    u8 varReg{nextReg};
    emitVariableOp(getVar, info, varReg, info.slot);
    reserveReg();

    u8 valueReg{nextReg};
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
    emitVariableOp(setVar, info, info.slot, varReg);
    freeReg(); // Free the temporary register used for the RHS value.
}

DEF(AssignExpr)
{
    // Temporarily assuming regular variables only.
    auto* temp{static_cast<VarExpr*>(node->target.get())};
    VarInfo info{resolveVariable(temp->name)};

    if (!info.found)
        REPORT_ERROR(VAR_NOT_DEFINED, temp->name);
    else if (info.access == accessFix)
        REPORT_ERROR(ASSIGN_CONST_VARIABLE, node->oper);

    if (node->oper.type != TOK_EQUAL)
    {
        compoundAssign(node, info);
        return;
    }

    u8 reg{nextReg};
    compileExpr(node->value);
    addVariableOp(setVar, info, info.slot, reg);
}

DEF(LogicExpr)
{
    if ((node->oper == TOK_AMP_AMP) || (node->oper == TOK_AND)) // &&, and
    {
        u8 reg{nextReg};
        compileExpr(node->left);
        u64 falseJump{code.addJump(OP_JUMP_FALSE, reg)};
        nextReg = reg;
        compileExpr(node->right);
        code.patchJump(falseJump);
    }
    else if ((node->oper == TOK_BAR_BAR) || (node->oper == TOK_OR)) // ||, or
    {
        u8 reg{nextReg};
        compileExpr(node->left);
        u64 trueJump{code.addJump(OP_JUMP_TRUE, reg)};
        nextReg = reg;
        compileExpr(node->right);
        code.patchJump(trueJump);
    }
}

DEF(CompareExpr)
{
    u8 firstOper{nextReg};
    compileExpr(node->left);

    u8 secondOper{nextReg};
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
    u8 firstOper{nextReg};
    compileExpr(node->left);

    u8 secondOper{nextReg};
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
    u8 firstOper{nextReg};
    compileExpr(node->left);

    u8 secondOper{nextReg};
    compileExpr(node->right);

    code.addOp(node->oper == TOK_RIGHT_SHIFT ?
        OP_SHIFT_R : OP_SHIFT_L, firstOper, secondOper);
    freeReg();
}

DEF(BinaryExpr)
{
    u8 firstOper{nextReg};
    compileExpr(node->left);

    u8 secondOper{nextReg};
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
        case TOK_DOT_DOT:   op = OP_RANGE;  break;
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
        REPORT_ERROR(INVALID_INCR_DECR_TARGET, node->oper);

    auto* temp{static_cast<VarExpr*>(node->expr.get())};
    VarInfo info{resolveVariable(temp->name)};
    if (!info.found)
        REPORT_ERROR(VAR_NOT_DEFINED, temp->name);
    else if (info.access == accessFix)
        REPORT_ERROR(MOD_CONST_VARIABLE, node->oper);

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

    emitVariableOp(getVar, info, nextReg, info.slot);
    reserveReg();

    emitVariableOp(getVar, info, nextReg, info.slot);
    code.addOp((node->oper.type == TOK_INCR ?
        OP_INCR : OP_DECR), nextReg);
    emitVariableOp(setVar, info, info.slot, nextReg);

    if (!node->prev)
    {
        code.addOp(OP_MOVE_R, static_cast<u8>(nextReg - 1),
            nextReg);
    }
}

DEF(UnaryExpr)
{
    if ((node->oper.type == TOK_INCR) || (node->oper.type == TOK_DECR))
    {
        _crementExpr(node);
        return;
    }

    u8 firstOper{nextReg};
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

    u8 location{};
    if (node->builtin)
    {
        auto* var{static_cast<VarExpr*>(node->callee.get())};
        auto find{Natives::builtins.find(var->name.text)};
        if (find == Natives::builtins.end())
            REPORT_ERROR(BUILTIN_NOT_FOUND, var->name);
        location = static_cast<u8>(find->second);
        reserveReg(); // Reserve a register in place of the function object.
    }
    else
    {
        location = nextReg;
        compileExpr(node->callee); // Will reserve a register.
    }

    u8 argsStart{nextReg};
    for (const ExprUP& arg : node->args)
        compileExpr(arg);

    u8 size{static_cast<u8>(node->args.size())};
    code.addOp((node->builtin ? OP_CALL_NAT : OP_CALL_DEF),
        location, argsStart, size);

    // For user-defined functions, the return value replaces the
    // function object.
    // For built-ins, we place the return value in the empty register
    // reserved above.
    nextReg = argsStart;
}

DEF(IfExpr)
{
    u8 reg{nextReg};
    compileExpr(node->condition);
    u64 falseJump{code.addJump(OP_JUMP_FALSE, reg)};
    freeReg();

    u8 current{nextReg};
    compileExpr(node->trueExpr);
    u64 trueJump{code.addJump(OP_JUMP)};
    code.patchJump(falseJump);

    nextReg = current;
    compileExpr(node->falseExpr);
    code.patchJump(trueJump);
}

DEF(LambdaExpr)
{
    if (node->params.size() > PARAMETER_MAX)
        REPORT_ERROR(HIT_PARAM_MAX, node->params[PARAMETER_MAX]);

    funcBodyHelper(node->params, node->body, nextReg,
        std::string());
    reserveReg();
}

DEF(ComprehensionExpr)
{
    u8 listReg{nextReg};
    code.addOp(OP_LIST, listReg);
    reserveReg();

    pushScope();
    u8 varReg{nextReg};
    defVar(std::string(node->var.text), varReg, accessFix); // For now.
    reserveReg();

    u8 iterReg{nextReg};
    compileExpr(node->iter);

    code.addOp(OP_MAKE_ITER, varReg, iterReg);
    u64 failJump{code.addJump(OP_JUMP)}; // If we fail to construct an iterator.

    u64 loopStart{code.getLoopStart()};
    u64 whereJump{0};
    if (node->where != nullptr)
    {
        u8 whereReg{nextReg};
        compileExpr(node->where);
        whereJump = code.addJump(OP_JUMP_FALSE, whereReg);
        freeReg();
    }

    u8 result{nextReg};
    compileExpr(node->expr);
    code.addOp(OP_EXT_LIST, listReg, result, u8(1));

    if (whereJump != 0)
        code.patchJump(whereJump);

    constexpr int UPDATE_ITER_OP_SIZE{5};
    u16 diff{static_cast<u16>(code.codeSize() - loopStart
        + UPDATE_ITER_OP_SIZE)};
    code.addOp(OP_UPDATE_ITER, varReg, iterReg,
        static_cast<u8>((diff >> CHAR_BIT) & CODE_MAX),
        static_cast<u8>(diff & CODE_MAX)
    );

    code.patchJump(failJump);
    popScope();
}

DEF(ListExpr)
{
    u8 listReg{nextReg};
    code.addOp(OP_LIST, listReg);
    reserveReg();

    u8 count{0};
    u8 startReg{nextReg};
    auto emitList = [this, listReg, &count, startReg] {
        code.addOp(OP_EXT_LIST, listReg, startReg, count);
        nextReg = startReg;
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

DEF(ReferenceExpr)
{
    VarInfo info{resolveVariable(node->name)};
    if (!info.found)
    {
        REPORT_ERROR(VAR_NOT_DEFINED, node->name,
            "cannot construct reference to undefined variable");
    }

    if (info.access == accessFix)
    {
        reportPart(false, REF_TO_CONST_VAR, node->operOffset, 1,
            "variable may be modified through this reference");
    }

    code.addOp(OP_MAKE_REF, nextReg, static_cast<u8>(info.type),
        info.slot);
    reserveReg();
}

DEF(VarExpr)
{
    VarInfo info{resolveVariable(node->name)};
    if (!info.found)
        REPORT_ERROR(VAR_NOT_DEFINED, node->name);

    emitVariableOp(getVar, info, nextReg, info.slot);
    reserveReg();
}

DEF(StringPartExpr)
{
    size_t start{};
    size_t offset{};

    switch (node->part.type)
    {
        case TOK_INTER_START:
            start = 1, offset = 1;
            break;
        case TOK_INTER_PART:
            start = 0, offset = 0;
            break;
        case TOK_INTER_END:
            start = 0, offset = 1;
            break;
        default:
            CH_UNREACHABLE();
    }

    Object obj{CH_ALLOC(String, parseStringToken(node->part, start, offset))};
    code.loadRegConst(obj, nextReg);
    reserveReg();
}

DEF(FormatExpr)
{
    u8 partsBegin{nextReg};
    for (const ExprUP& part : node->parts)
        compileExpr(part);

    code.addOp(OP_FORMAT_STR, partsBegin,
        static_cast<u8>(node->parts.size()));

    // Free all registers except first one (containing the
    // final string).
    nextReg = partsBegin + 1;
}

[[nodiscard]]
static std::string getRawString(const std::string_view& text)
{
    size_t start{sizeof("r\"") - 1};
    size_t sizeOffset{start + sizeof("\"") - 1};

    // Skip leading or trailing newlines.
    if (text[start] == '\n')
    {
        start++;
        sizeOffset++;
    }
    if (text[text.size() - 2] == '\n')
        sizeOffset++;

    return std::string{text.substr(start, text.size() - sizeOffset)};
}

DEF(LiteralExpr)
{
    const Token& tok{node->value};

    if (tok.type == TOK_NUM)
    {
        Object obj{tok.content.i};
        code.loadRegConst(obj, nextReg);
        reserveReg();
    }

    else if (tok.type == TOK_NUM_DEC)
    {
        Object obj{tok.content.d};
        code.loadRegConst(obj, nextReg);
        reserveReg();
    }

    else if (tok.type == TOK_STR_LIT)
    {
        Object obj{CH_ALLOC(String, parseStringToken(tok, 1, 2))};
        code.loadRegConst(obj, nextReg);
        reserveReg();
    }

    else if (tok.type == TOK_RAW_STR)
    {
        Object obj{CH_ALLOC(String, getRawString(tok.text))};
        code.loadRegConst(obj, nextReg);
        reserveReg();
    }

    else if ((tok.type == TOK_TRUE) || (tok.type == TOK_FALSE))
    {
        bool value{tok.content.b};
        code.loadReg(nextReg, (value ? OP_TRUE : OP_FALSE));
        reserveReg();
    }

    else if (tok.type == TOK_NULL)
    {
        code.loadReg(nextReg, OP_NULL);
        reserveReg();
    }
}

/* General compilation functions. */

void ASTCompiler::compileExpr(const ExprUP& node)
{
    if (node == nullptr) return;

    u64 lastIndex{metadata.size()};
    metadata.push_back(DebugRange{
        code.codeSize(), 0, node->sourceStart, node->sourceEnd
    });

    switch (node->type)
    {
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
        case E_REF_EXPR:        COMPILE(ReferenceExpr);     break;
        case E_VAR_EXPR:        COMPILE(VarExpr);           break;
        case E_STR_PART_EXPR:   COMPILE(StringPartExpr);    break;
        case E_FORMAT_EXPR:     COMPILE(FormatExpr);        break;
        case E_LITERAL_EXPR:    COMPILE(LiteralExpr);       break;
    }

    metadata[lastIndex].byteEnd = code.codeSize();
}

void ASTCompiler::compileStmt(const StmtUP& node)
{
    if (node == nullptr) return;

    u64 lastIndex{metadata.size()};
    metadata.push_back(DebugRange{
        code.codeSize(), 0, node->sourceStart, node->sourceEnd
    });

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

    metadata[lastIndex].byteEnd = code.codeSize();
}

ByteCode& ASTCompiler::getCode()
{
    code.setDebugData(id, metadata);
    return code;
}

Function* ASTCompiler::compile(FileID id, const StmtVec& program)
{
    this->id = id;
    code.clear();
    metadata.clear();
    clearDeclarations();
    // Inherit hitError and errorCount from parser.

    for (const StmtUP& node : program)
        compileStmt(node);

    if (hitError)
    {
        code.clear();
        clearDeclaredVars = true; // Will clear on the next run.
    }
    else
        code.addOp(OP_HALT);

    return CH_ALLOC(Function, getCode(), 0);
}

#undef DEF
#undef COMPILE
#undef REPORT_ERROR