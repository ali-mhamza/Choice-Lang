#ifndef COMP_AST

#include "linearTable.h"
#include "../include/compiler.h"
#include "../include/common.h"
#include "../include/error.h"
#include "../include/linear_alloc.h"
#include "../include/natives.h"
#include "../include/object.h"
#include "../include/opcodes.h"
#include "../include/token.h"
#include "../include/utils.h"
#include "../include/vartable.h"
#include <string_view>

constexpr bool accessFix = false;
constexpr bool accessVar = true;

class TokCompVarsWrapper
{
    public:
        linearTable<VarEntry, ui8, VarHasher> vars;
        linearTable<ui8, bool> access;

        TokCompVarsWrapper() = default;
};

class TokCompLoopLabels
{
    public:
        linearTable<std::string_view, std::vector<ui64>> labels;

        TokCompLoopLabels() = default;
};

Compiler::Compiler(Compiler* comp) :
    scopeCompiler(comp),
    varsWrapper(new TokCompVarsWrapper),
    labelsWrapper(new TokCompLoopLabels),
    exprPrint(inRepl)
{
    if (comp == nullptr)
    {
        this->previousReg = Natives::FuncType::NUM_FUNCS;
        this->depth = 0;
        for (ui8 i = 0; i < Natives::FuncType::NUM_FUNCS; i++)
        {
            this->defVar(
                std::string(Natives::funcNames[i]),
                i,
                accessFix // For now.
            );
        }
    }
    else
    {
        this->previousReg = 0;
        this->depth = comp->depth + 1;
    }
}

Compiler::~Compiler()
{
    delete varsWrapper;
    delete labelsWrapper;
}

inline void Compiler::defVar(std::string name, ui8 reg, bool access)
{
    varsWrapper->vars[{name, scope}] = reg;
    varsWrapper->access[reg] = access;
    if (scope != 0) varScopes.top().push_back(name);
}

inline Compiler::VarInfo Compiler::getVarInfo(const Token& token)
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

inline bool Compiler::getAccess(ui8 reg)
{
    return *(varsWrapper->access.get(reg));
}

inline void Compiler::popScope()
{
    auto& scopeVec = varScopes.top();
    for (std::string& var : scopeVec)
        varsWrapper->vars.remove({var, scope});
}

inline void Compiler::nextTok()
{
    if (currentTok.type != TOK_EOF)
    {
        previousTok = currentTok;
        currentTok = *(++it);
    }
}

inline bool Compiler::checkTok(TokenType type)
{
    return (currentTok.type == type);
}

inline bool Compiler::consumeTok(TokenType type)
{
    if (checkTok(type))
    {
        nextTok();
        return true;
    }

    return false;
}

template<typename... Type>
inline bool Compiler::consumeToks(Type... toks)
{
    for (TokenType type : {toks ...})
    {
        if (checkTok(type))
            return consumeTok(type);
    }

    return false;
}

inline bool Compiler::matchError(TokenType type, std::string_view message)
{
    if (!consumeTok(type))
    {
        hitError = true;
        if (!syntaxError && (errorCount < COMPILE_ERROR_MAX))
        {
            CompileError(currentTok, std::string(message)).report();
            syntaxError = true;
            errorCount++;
        }
        else if (errorCount == COMPILE_ERROR_MAX)
            FORMAT_PRINT("COMPILATION ERROR MAXIMUM REACHED.\n");
        errorCount++;
        return false;
    }

    return true;
}

inline bool Compiler::consumeType()
{
    for (int i = TOK_INT; i <= TOK_ANY; i++)
    {
        TokenType type = static_cast<TokenType>(i);
        if (checkTok(type))
            return consumeTok(type);
    }

    return false;
}

void Compiler::reset()
{
    while (!checkTok(TOK_EOF))
    {
        if ((previousTok.type == TOK_SEMICOLON)
            || (previousTok.type == TOK_RIGHT_BRACE))
                return;

        switch (currentTok.type)
        {
            case TOK_IF:        case TOK_ELIF:      case TOK_ELSE:
            case TOK_WHILE:     case TOK_FOR:       case TOK_REPEAT:
            case TOK_UNTIL:     case TOK_BREAK:     case TOK_CONT:
            case TOK_MATCH:     case TOK_IS:        case TOK_FALL:
            case TOK_END:       case TOK_MAKE:      case TOK_FIX:
            case TOK_FUNC:      case TOK_RETURN:
            case TOK_IDENTIFIER:
            case TOK_LEFT_BRACE: 
                return;
            default:
                nextTok();
        }
    }
}

inline void Compiler::matchType(std::string_view message /* = "" */)
{
    if (!consumeType())
        REPORT_SYNTAX(currentTok, std::string(message));
}

void Compiler::compileDescent(void (Compiler::*func)(),
    TokenType tok, Opcode op)
{
    ui8 firstOper = previousReg;
    (this->*func)();

    while (consumeTok(tok))
    {
        ui8 secondOper = previousReg;
        (this->*func)();
        code.addOp(op, depth, firstOper, secondOper);
        freeReg();
    }
}

inline void Compiler::setCompilerData(Compiler* other)
{
    this->inFunc = other->inFunc;
    this->syntaxError = other->syntaxError;
    this->semanticError = other->semanticError;
    this->it = other->it;
    this->previousTok = other->previousTok;
    this->currentTok = other->currentTok;
}

void Compiler::declaration()
{
    if (consumeToks(TOK_MAKE, TOK_FIX))
        varDecl();
    else if (consumeTok(TOK_FUNC))
        funDecl();
    else
        statement();

    // Reset here in case we need to reset
    // while within a block statement.
    if (syntaxError || semanticError)
    {
        reset();
        syntaxError = semanticError = false;
    }
}

void Compiler::varDecl()
{
    TokenType declType = previousTok.type;
    consumeTok(TOK_DEF); // In case it's there.
    MATCH_TOK(TOK_IDENTIFIER, "Expect variable name.");
    Token name = previousTok;

    VarInfo pos = getVarInfo(name);
    if ((pos.slot != nullptr)
        && (pos.scope == scope)
        && (pos.depth == depth))
    {
        #if ALLOW_REDEFS
            ui8 varSlot = *(pos.slot);
            ui8 reg = previousReg;
            if (consumeTok(TOK_EQUAL))
            {
                expression();
                // We only declare in the current function scope.
                code.addOp(OP_SET_VAR, depth, varSlot, reg);
            }
            else
                code.loadReg(varSlot, OP_NULL, depth);
            MATCH_TOK(TOK_SEMICOLON, "Expect ';' after variable declaration.");
            return;
        #else
            REPORT_SEMANTIC(name, "Variable '" + std::string(name.text)
                + "' is already defined in this scope.");
        #endif
    }

    if (consumeTok(TOK_COLON))
        matchType("Expect variable type.");

    ui8 slot = previousReg;
    if (consumeTok(TOK_EQUAL))
        expression();
    else if (declType == TOK_FIX)
    {
        if (currentTok.type == TOK_SEMICOLON)
            REPORT_SEMANTIC(currentTok,
                "Initializer required for fixed-value variable.");
        else
            REPORT_SYNTAX(currentTok,
                "Expect '=' before initializer for fixed-value variable.");
    }
    else
    {
        code.loadReg(slot, OP_NULL, depth);
        reserveReg();
    }
    MATCH_TOK(TOK_SEMICOLON, "Expect ';' after variable declaration.");

    defVar(std::string(name.text), slot,
        declType == TOK_MAKE ? accessVar : accessFix);
}

void Compiler::funDecl()
{
    MATCH_TOK(TOK_IDENTIFIER, "Expect function name.");
    bool redefined = false;
    VarInfo pos = getVarInfo(previousTok);
    if ((pos.slot != nullptr)
        && (pos.scope == scope)
        && (pos.depth == depth))
    {
        #if ALLOW_REDEFS
            redefined = true;
        #else
            REPORT_SEMANTIC(node->name,
                "Object '" + std::string(node->name.text)
                + "' is already defined in this scope.");
        #endif
    }

    std::string name = std::string(previousTok.text);
    MATCH_TOK(TOK_LEFT_PAREN, "Expect '(' after function name.");

    ui8 varSlot = (redefined ? *(pos.slot) : previousReg);
    Compiler miniCompiler(this);
    ui8 count = 0;
    if (!checkTok(TOK_RIGHT_PAREN))
    {
        do {
            MATCH_TOK(TOK_IDENTIFIER, "Expect parameter name.");
            if (count == 255)
                REPORT_SYNTAX(previousTok, "Too many parameters in function.");
            miniCompiler.defVar(std::string(previousTok.text), count++, accessVar);
            miniCompiler.reserveReg();
        } while (consumeTok(TOK_COMMA));
    }
    MATCH_TOK(TOK_RIGHT_PAREN, "Expect ')' to close function signature.");
    MATCH_TOK(TOK_LEFT_BRACE, "Expect '{' before function body.");

    if (!redefined)
    {
        defVar(name, varSlot, accessFix); // Temporarily.
        reserveReg();
    }

    bool prevInFunc = inFunc;
    inFunc = true;
    miniCompiler.setCompilerData(this);
    miniCompiler.blockStmt();
    miniCompiler.code.addOp(OP_VOID, 0);
    miniCompiler.code.addOp(OP_RETURN, 0);

    this->setCompilerData(&miniCompiler);
    inFunc = prevInFunc;
    ByteCode& funcCode = miniCompiler.code;
    funcCode.depth = miniCompiler.depth;
    Object func = ALLOC(Function, ObjDealloc<Function>, name,
        funcCode, count);

    // We only declare in the current function scope.
    code.loadRegConst(func, varSlot, depth);
}

void Compiler::statement()
{
    if (consumeTok(TOK_IF))
        ifStmt();
    else if (consumeTok(TOK_WHILE))
        whileStmt();
    else if (consumeTok(TOK_FOR))
        forStmt();
    else if (consumeTok(TOK_MATCH))
        matchStmt();
    else if (consumeTok(TOK_REPEAT))
        repeatStmt();
    else if (consumeTok(TOK_RETURN))
        returnStmt();
    else if (consumeTok(TOK_BREAK))
        breakStmt();
    else if (consumeTok(TOK_LEFT_BRACE))
        blockStmt();
    else if (consumeTok(TOK_CONT))
    {
        MATCH_TOK(TOK_SEMICOLON, "Expect ';' after 'continue'.");
        this->continueJumps->push_back(code.addJump(OP_JUMP));
    }
    else if (consumeTok(TOK_FALL))
    {
        if (!inMatch)
            REPORT_SEMANTIC(previousTok, "Invalid instruction 'fallthrough'" \
                " outside of match-is structure.");
        MATCH_TOK(TOK_SEMICOLON, "Expect ';' after 'fallthrough'.");
        if (!checkTok(TOK_IS) && !checkTok(TOK_RIGHT_BRACE))
            REPORT_SEMANTIC(currentTok,
                "Cannot have a statement following a 'fallthrough' instruction.");
        fall = true;
    }
    else if (consumeTok(TOK_END))
    {
        if (!inMatch)
            REPORT_SEMANTIC(previousTok,
                "Invalid instruction 'end' outside of match-is structure.");
        MATCH_TOK(TOK_SEMICOLON, "Expect ';' after 'end'.");
        this->endJumps->push_back(code.addJump(OP_JUMP));
    }
    else
        exprStmt();
}

void Compiler::ifStmt()
{
    MATCH_TOK(TOK_LEFT_PAREN, "Expect '(' after 'if'.");
    ui8 reg = previousReg;
    expression();
    MATCH_TOK(TOK_RIGHT_PAREN, "Expect ')' after condition.");

    ui64 falseJump = code.addJump(OP_JUMP_FALSE, reg);
    freeReg();
    statement();

    if (consumeToks(TOK_ELIF, TOK_ELSE))
    {
        ui64 trueJump = code.addJump(OP_JUMP);
        code.patchJump(falseJump);
        
        if (previousTok.type == TOK_ELIF)
            ifStmt();
        else if (previousTok.type == TOK_ELSE)
            statement();
        code.patchJump(trueJump);
    }
    else
        code.patchJump(falseJump);
}

void Compiler::whileLoopHelper(ui64 loopStart, ui64 falseJump)
{
    Token label;
    if (consumeTok(TOK_COLON))
    {
        MATCH_TOK(TOK_IDENTIFIER, "Expect loop label after ':'.");
        label = previousTok;
        this->labelsWrapper->labels.add(
            label.text, // Will persist at least as long as compilation takes.
            {}
        );
    }

    statement();

    for (ui64 jump : *continueJumps)
        code.patchJump(jump);

    code.addLoop(loopStart);
    code.patchJump(falseJump);

    if (consumeTok(TOK_ELSE))
        statement();

    for (ui64 jump : *breakJumps)
        code.patchJump(jump);

    if (label.type != TOK_EOF)
    {
        auto* vec = this->labelsWrapper->labels.get(
            label.text
        );
        for (ui64 jump : *vec)
            code.patchJump(jump);
        this->labelsWrapper->labels.remove(
            label.text
        );
    }
}

void Compiler::whileStmt()
{
    MATCH_TOK(TOK_LEFT_PAREN, "Expect '(' after 'while'.");
    ui8 reg = previousReg;
    ui64 loopStart = code.getLoopStart();

    std::vector<ui64> breaks;
    auto prevBreaks = breakJumps;
    breakJumps = &breaks;

    std::vector<ui64> continues;
    auto prevContinues = continueJumps;
    continueJumps = &continues;

    expression();
    ui64 falseJump = code.addJump(OP_JUMP_FALSE, reg);
    freeReg();
    MATCH_TOK(TOK_RIGHT_PAREN, "Expect ')' after condition.");

    whileLoopHelper(loopStart, falseJump);

    breakJumps = prevBreaks;
    continueJumps = prevContinues;
}

void Compiler::forLoopHelper(ui8 varReg, ui8 iterReg)
{
    code.addOp(OP_MAKE_ITER, varReg, iterReg);
    ui64 failJump = code.addJump(OP_JUMP); // If we fail to construct an iterator.

    ui64 loopStart = code.getLoopStart();
    ui64 whereJump = 0;
    if (consumeTok(TOK_WHERE))
    {
        ui8 whereReg = previousReg;
        expression();
        whereJump = code.addJump(OP_JUMP_FALSE, whereReg);
    }

    MATCH_TOK(TOK_RIGHT_PAREN, "Expect ')' after condition.");

    Token label; // Default: TOK_EOF.
    if (consumeTok(TOK_COLON))
    {
        MATCH_TOK(TOK_IDENTIFIER, "Expect loop label after ':'.");
        label = previousTok;
        this->labelsWrapper->labels.add(
            label.text,
            {}
        );
    }

    statement();

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
    if (consumeTok(TOK_ELSE))
        statement();
    for (ui64 jump : *breakJumps)
        code.patchJump(jump);
    if (label.type != TOK_EOF)
    {
        auto* vec = this->labelsWrapper->labels.get(
            label.text
        );
        for (ui64 jump : *vec)
            code.patchJump(jump);
        this->labelsWrapper->labels.remove(
            label.text
        );
    }
}

void Compiler::forStmt()
{
    scope++;
    ui8 origVarReg = previousReg;
    varScopes.emplace();

    std::vector<ui64> breaks;
    auto prevBreaks = breakJumps;
    breakJumps = &breaks;

    std::vector<ui64> continues;
    auto prevContinues = continueJumps;
    continueJumps = &continues;

    MATCH_TOK(TOK_LEFT_PAREN, "Expect '(' after 'for'.");
    MATCH_TOK(TOK_IDENTIFIER, "Expect loop variable identifier.");
    Token var = previousTok;
    ui8 varReg = previousReg;
    defVar(std::string(var.text), varReg, accessFix); // For now.
    reserveReg();

    MATCH_TOK(TOK_IN, "Expect 'in' keyword after loop identifier.");
    ui8 iterReg = previousReg;
    expression();

    forLoopHelper(varReg, iterReg);

    breakJumps = prevBreaks;
    continueJumps = prevContinues;

    popScope();
    varScopes.pop();
    scope--;
    previousReg = origVarReg;
}

ui64 Compiler::matchCaseHelper(const ui8 matchReg, ui64& fallJump,
    ui64& emptyJump)
{
    ui64 retJump = 0;
    Token errorToken = currentTok;
    if (!IS_LITERAL(errorToken.type))
    {
        // Must report manually since function is not void.
        hitError = true;
        if (!semanticError && (errorCount < COMPILE_ERROR_MAX))
        {
            CompileError(errorToken, "Case value must be a literal.").report();
            semanticError = true;
            errorCount++;
        }
        return UINT64_MAX;
    }

    ui8 caseReg = previousReg;
    primary(); // Compile the literal.
    code.addOp(OP_EQUAL, caseReg, matchReg);
    ui64 falseJump = code.addJump(OP_JUMP_FALSE, caseReg);
    freeReg();
    if (!matchError(TOK_COLON, "Expect ':' before case body.")) return UINT64_MAX;

    if (fallJump != 0) // We skip condition checking during fallthrough.
        code.patchJump(fallJump);
    if (emptyJump != 0)
    {
        code.patchJump(emptyJump);
        emptyJump = 0;
    }

    bool empty = false;
    if (!checkTok(TOK_IS) && !checkTok(TOK_RIGHT_BRACE))
        statement();
    else
        empty = true;

    if (fall || (fallJump != 0))
        fallJump = code.addJump(OP_JUMP);
    else if (empty)
        emptyJump = code.addJump(OP_JUMP);
    else
        retJump = code.addJump(OP_JUMP);

    code.patchJump(falseJump);
    return retJump;
}

void Compiler::matchStmt()
{
    bool prevInMatch = inMatch;
    inMatch = true;
    
    MATCH_TOK(TOK_LEFT_PAREN, "Expect '(' before match value.");
    ui8 matchReg = previousReg;
    expression();
    MATCH_TOK(TOK_RIGHT_PAREN, "Expect ')' after match value.");
    MATCH_TOK(TOK_LEFT_BRACE, "Expect '{' before match cases.");

    int caseCount = 0;
    ui64 fallJump = 0; // Invalid jump offset value.
    ui64 emptyJump = 0;

    std::vector<ui64> jumps;
    auto prevEndJumps = endJumps;
    endJumps = &jumps;

    while (!checkTok(TOK_RIGHT_BRACE) && !checkTok(TOK_EOF))
    {
        if (caseCount == MATCH_CASES_MAX)
            REPORT_SEMANTIC(currentTok,
                "Too many cases in match-is structure.");
        
        MATCH_TOK(TOK_IS, "Expect 'is' before case value.");
        bool defaultCase = false;

        ui64 retJump = 0;
        if (consumeTok(TOK_QMARK))
        {
            defaultCase = true;
            statement();
        }
        else
            retJump = matchCaseHelper(matchReg, fallJump, emptyJump);
        caseCount++;

        fall = false;
        if (retJump != 0)
            jumps.push_back(retJump);
        else if (retJump == UINT64_MAX)
            return;
        if (defaultCase)
        {
            if (consumeTok(TOK_IS))
                REPORT_SEMANTIC(previousTok,
                    "Cannot have another case after the default case.");
            else
                break;
        }
    }

    MATCH_TOK(TOK_RIGHT_BRACE, "Expect '}' after match-is structure.");
    for (ui64 jump : jumps)
        code.patchJump(jump);
    freeReg(); // Remove the match value.

    inMatch = prevInMatch;
    endJumps = prevEndJumps;
}

void Compiler::repeatStmt()
{
    MATCH_TOK(TOK_LEFT_BRACE, "Expect '{' before 'repeat' block.");
    ui64 loopStart = code.getLoopStart();
    blockStmt(); // Will consume the '}'.

    MATCH_TOK(TOK_UNTIL, "Expect 'until' condition after 'repeat'.");
    MATCH_TOK(TOK_LEFT_PAREN, "Expect '(' before 'until' condition.");
    ui8 reg = previousReg;
    expression();
    MATCH_TOK(TOK_RIGHT_PAREN, "Expect ')' after 'until' condition.");
    MATCH_TOK(TOK_SEMICOLON, "Expect ';' after repeat-until block.");

    ui64 trueJump = code.addJump(OP_JUMP_TRUE, reg);
    freeReg();
    code.addLoop(loopStart);
    code.patchJump(trueJump);
}

void Compiler::returnStmt()
{
    if (!inFunc)
        REPORT_SEMANTIC(previousTok,
            "Cannot use 'return' outside a function.");
    ui8 reg = previousReg;
    if (checkTok(TOK_SEMICOLON))
        code.addOp(OP_VOID, reg);
    else
        expression();
    MATCH_TOK(TOK_SEMICOLON, "Expect ';' after return statement.");
    code.addOp(OP_RETURN, reg);
}

void Compiler::breakStmt()
{
    if (consumeTok(TOK_IDENTIFIER))
    {
        const Token& label = previousTok;
        auto* vec = this->labelsWrapper->labels.get(
            label.text
        );

        if (vec == nullptr)
            REPORT_SEMANTIC(label, "Break label is not assigned to any loop.");
        vec->push_back(code.addJump(OP_JUMP));
    }
    else
        this->breakJumps->push_back(code.addJump(OP_JUMP));

    MATCH_TOK(TOK_SEMICOLON, "Expect ';' after 'break'.");
}

void Compiler::blockStmt()
{
    scope++;
    varScopes.emplace();
    ui8 origVarReg = previousReg;
    while (!checkTok(TOK_RIGHT_BRACE) && !checkTok(TOK_EOF))
        declaration();
    MATCH_TOK(TOK_RIGHT_BRACE, "Expect '}' after block.");
    popScope();
    varScopes.pop();
    scope--;
    previousReg = origVarReg;
}

void Compiler::exprStmt()
{
    ui8 reg = previousReg;
    expression();
    if (exprPrint)
        code.addOp(OP_PRINT_VALID, reg);
    MATCH_TOK(TOK_SEMICOLON, "Expect ';' after expression.");
    freeReg();
}

void Compiler::expression()
{
    assignment();
}

void Compiler::compoundAssign(TokenType oper, const VarInfo& pos)
{
    ui8 reg = previousReg;
    expression();

    Opcode op;
    switch (oper)
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

// For the time being, we just look for direct
// identifier names.
// This would need to be modified when other types
// of "variables" are introduced, like class fields.

void Compiler::assignment()
{
    // Bit of a hack.
    const Token& firstTok = currentTok;
    const Token& secondTok = *(it + 1);

    if (IS_ASSIGN(secondTok.type))
    {
        exprPrint = false;
        
        if (firstTok.type != TOK_IDENTIFIER)
            REPORT_SEMANTIC(firstTok, "Invalid assignment target.");
        
        nextTok();
        VarInfo pos = getVarInfo(previousTok);
        if (pos.slot != nullptr)
        {
            nextTok();
            if (pos.access == accessFix)
                REPORT_SEMANTIC(
                    secondTok, "Cannot assign to a fixed-value variable."
                );
            if (secondTok.type != TOK_EQUAL)
                compoundAssign(secondTok.type, pos);
            else
            {
                ui8 value = previousReg;
                expression(); // Does not consume the ';'.
                code.addOp(OP_SET_VAR, pos.depth, *(pos.slot), value);
            }
        }
        else
            REPORT_SEMANTIC(previousTok, "Undefined variable '"
                + std::string(previousTok.text) + "'.");
    }
    else
        logicOr();
}

void Compiler::logicOr()
{
    ui8 reg = previousReg;
    logicAnd();

    while (consumeToks(TOK_BAR_BAR, TOK_OR))
    {
        ui64 trueJump = code.addJump(OP_JUMP_TRUE, reg);
        previousReg = reg;
        logicAnd();
        code.patchJump(trueJump);
    }
}

void Compiler::logicAnd()
{
    ui8 reg = previousReg;
    equality();

    while (consumeToks(TOK_AMP_AMP, TOK_AND))
    {
        ui64 falseJump = code.addJump(OP_JUMP_FALSE, reg);
        previousReg = reg;
        equality();
        code.patchJump(falseJump);
    }
}

void Compiler::equality()
{
    ui8 firstOper = previousReg;
    comparison();

    while (consumeToks(TOK_EQ_EQ, TOK_BANG_EQ))
    {
        TokenType oper = previousTok.type;
        ui8 secondOper = previousReg;
        comparison();

        code.addOp(OP_EQUAL, firstOper, secondOper);
        if (oper == TOK_BANG_EQ)
            code.addOp(OP_NOT, firstOper);
        freeReg();
    }
}

void Compiler::comparison()
{
    ui8 firstOper = previousReg;
    bitOr();

    while (consumeToks(TOK_GT, TOK_GT_EQ, TOK_LT, TOK_LT_EQ,
        TOK_IN) || (consumeTok(TOK_NOT) && checkTok(TOK_IN)))
    {
        TokenType oper = previousTok.type;
        if (oper == TOK_NOT) nextTok();
        ui8 secondOper = previousReg;
        bitOr();

        switch (oper)
        {
            case TOK_IN:
            case TOK_NOT: // not in
                code.addOp(OP_IN, firstOper, secondOper);
                break;
            case TOK_GT:
            case TOK_LT_EQ:
                code.addOp(OP_GT, firstOper, secondOper);
                break;
            case TOK_LT:
            case TOK_GT_EQ:
                code.addOp(OP_LT, firstOper, secondOper);
                break;
            default: UNREACHABLE();
        }

        if ((oper == TOK_NOT) || (oper == TOK_GT_EQ)
            || (oper == TOK_LT_EQ))
                code.addOp(OP_NOT, firstOper);

        freeReg();
    }
}

void Compiler::bitOr()
{
    compileDescent(&Compiler::bitXor, TOK_BAR, OP_OR);
}

void Compiler::bitXor()
{
    compileDescent(&Compiler::bitAnd, TOK_UARROW, OP_XOR);
}

void Compiler::bitAnd()
{
    compileDescent(&Compiler::shift, TOK_AMP, OP_AND);
}

void Compiler::shift()
{
    ui8 firstOper = previousReg;
    sum();

    while (consumeToks(TOK_RIGHT_SHIFT, TOK_LEFT_SHIFT))
    {
        TokenType oper = previousTok.type;
        ui8 secondOper = previousReg;
        sum();

        code.addOp(oper == TOK_RIGHT_SHIFT ? OP_SHIFT_R: OP_SHIFT_L,
            depth, firstOper, secondOper);
        freeReg();
    }
}

void Compiler::sum()
{
    ui8 firstOper = previousReg;
    product();

    while (consumeToks(TOK_PLUS, TOK_MINUS))
    {
        TokenType oper = previousTok.type;
        ui8 secondOper = previousReg;
        product();

        code.addOp(oper == TOK_PLUS ? OP_ADD : OP_SUB,
            depth, firstOper, secondOper);
        freeReg();
    }
}

void Compiler::product()
{
    ui8 firstOper = previousReg;
    unary();

    while (consumeToks(TOK_STAR, TOK_SLASH, TOK_PERCENT))
    {
        TokenType oper = previousTok.type;
        ui8 secondOper = previousReg;
        unary();

        Opcode op;
        switch (oper)
        {
            case TOK_STAR:      op = OP_MULT;   break;
            case TOK_SLASH:     op = OP_DIV;    break;
            case TOK_PERCENT:   op = OP_MOD;    break;
            default: UNREACHABLE();
        }
        code.addOp(op, depth, firstOper, secondOper);

        freeReg();
    }
}

void Compiler::_crementExpr(TokenType oper, bool prev)
{
    // Temporarily assuming only regular identifier variables.
    MATCH_TOK(TOK_IDENTIFIER, "Invalid increment/decrement target.");
    VarInfo pos = getVarInfo(previousTok);
    if (pos.slot != nullptr)
    {
        if (pos.access == accessFix)
            REPORT_SEMANTIC(
                previousTok, "Cannot modify a fixed-value variable."
            );
        else
        {
            ui8 slot = *(pos.slot);
            if (prev) // Post-increment/decrement operator(s).
            {
                consumeToks(TOK_INCR, TOK_DECR); // Skip the operator token.
                code.addOp(OP_GET_VAR, pos.depth, previousReg, slot);
            }
            code.addOp(oper == TOK_INCR ? OP_INCR : OP_DECR,
                pos.depth, slot);
            if (!prev)
                code.addOp(OP_GET_VAR, pos.depth, previousReg, slot);
            reserveReg();
        }
    }
    else
        REPORT_SEMANTIC(previousTok, "Undefined variable '"
            + std::string(previousTok.text) + "'.");
}

void Compiler::unary()
{
    if (consumeToks(TOK_INCR, TOK_DECR))
        _crementExpr(previousTok.type, false);
    else if (consumeToks(TOK_MINUS, TOK_BANG, TOK_NOT, TOK_TILDE))
    {
        TokenType oper = previousTok.type;
        ui8 firstOper = previousReg;
        exponent();

        Opcode op;
        switch (oper)
        {
            case TOK_MINUS: op = OP_NEGATE;     break;
            case TOK_BANG:
            case TOK_NOT:   op = OP_NOT;        break;
            case TOK_TILDE: op = OP_COMP;       break;
            default: UNREACHABLE();
        }

        if (op == OP_COMP)
            code.addOp(op, depth, firstOper);
        else
            code.addOp(op, firstOper);
        // We don't free a register since unary
        // operators don't use any extra registers.
        // They apply an operator directly onto a
        // register.
    }
    else
        exponent();
}

void Compiler::exponent()
{
    compileDescent(&Compiler::call, TOK_STAR_STAR, OP_POWER);
}

void Compiler::call()
{
    ui8 reg = previousReg;
    const Token tok = currentTok;
    bool identifierCall = false; // Dummy value.
    if (tok.type != TOK_EOF)
    {
        const Token& next = *(it + 1);
        identifierCall = ((tok.type == TOK_IDENTIFIER)
            && ((next.type == TOK_BANG) || (next.type == TOK_LEFT_PAREN)));
    }
    post();

    if (consumeToks(TOK_BANG, TOK_LEFT_PAREN))
    {
        bool builtin = false;
        if (previousTok.type == TOK_BANG)
        {
            if (!identifierCall)
                REPORT_SEMANTIC(tok, "Built-in functions must be called by name.");
            MATCH_TOK(TOK_LEFT_PAREN, "Invalid placement for token '!'.");
            builtin = true;
        }

        ui8 location, depth;

        if (builtin)
        {
            auto find = Natives::builtins.find(tok.text);
            if (find == Natives::builtins.end())
                REPORT_SEMANTIC(tok, "No builtin '" + std::string(tok.text)
                    + "' function.");
            location = static_cast<ui8>(find->second);
        }
        else if (identifierCall)
        {
            VarInfo pos = getVarInfo(tok);
            if (pos.slot == nullptr)
                REPORT_SEMANTIC(tok, "Undefined variable '"
                    + std::string(tok.text) + "'.");
            location = *(pos.slot);
            depth = pos.depth;
        }
        else
        {
            location = reg;
            depth = this->depth;
        }

        ui8 startReg = previousReg;
        ui8 argCount = 0;
        while (!checkTok(TOK_RIGHT_PAREN) && !checkTok(TOK_EOF))
        {
            do {
                expression();
                argCount++;
            } while (consumeTok(TOK_COMMA));
        }
        MATCH_TOK(TOK_RIGHT_PAREN, "Expect ')' following function arguments.");

        if (builtin)
            code.addOp(OP_CALL_NAT, location, startReg, argCount);
        else
            code.addOp(OP_CALL_DEF, depth, location, startReg, argCount);

        // Skip arguments.
        // Our return value replaces the first argument.
        if (argCount == 0)
            reserveReg();
        else
            previousReg -= argCount - 1;
    }
}

void Compiler::post()
{
    const Token& firstTok = currentTok;
    const Token& secondTok = *(it + 1);

    // Will need to extend for non-identifier operands later.
    if ((firstTok.type == TOK_IDENTIFIER)
        && ((secondTok.type == TOK_INCR) || (secondTok.type == TOK_DECR)))
            _crementExpr(secondTok.type, true);
    else
        primary();
}

void Compiler::ifExpr()
{
    ui8 reg = previousReg;
    MATCH_TOK(TOK_LEFT_PAREN, "Expect '(' before condition.");
    expression();
    MATCH_TOK(TOK_RIGHT_PAREN, "Expect ')' after condition.");
    ui64 falseJump = code.addJump(OP_JUMP_FALSE, reg);
    freeReg();

    MATCH_TOK(TOK_LEFT_BRACE, "Expect '{' before conditional expression.");
    ui8 current = previousReg;
    expression();
    MATCH_TOK(TOK_RIGHT_BRACE, "Expect '}' after conditional expression.");
    ui64 trueJump = code.addJump(OP_JUMP);
    code.patchJump(falseJump);

    previousReg = current;
    
    if (consumeTok(TOK_ELIF))
        ifExpr();
    else if (consumeTok(TOK_ELSE))
    {
        MATCH_TOK(TOK_LEFT_BRACE, "Expect '{' before conditional expression.");
        expression();
        MATCH_TOK(TOK_RIGHT_BRACE, "Expect '}' after conditional expression.");
    }
    else
        REPORT_SEMANTIC(currentTok,
            "A conditional expression must have a false-case branch.");

    code.patchJump(trueJump);
}

void Compiler::lambda()
{
    Compiler miniCompiler(this);
    ui8 count = 0;
    if (!checkTok(TOK_RIGHT_PAREN))
    {
        do {
            MATCH_TOK(TOK_IDENTIFIER, "Expect parameter name.");
            if (count == 255)
                REPORT_SYNTAX(previousTok, "Too many parameters in function.");
            miniCompiler.defVar(std::string(previousTok.text), count++, accessVar);
            miniCompiler.reserveReg();
        } while (consumeTok(TOK_COMMA));
    }
    MATCH_TOK(TOK_BAR, "Expect '|' after lambda parameters.");
    MATCH_TOK(TOK_LEFT_BRACE, "Expect '{' before lambda body.");

    bool prevInFunc = inFunc;
    inFunc = true;
    miniCompiler.setCompilerData(this);
    miniCompiler.blockStmt();
    miniCompiler.code.addOp(OP_VOID, 0);
    miniCompiler.code.addOp(OP_RETURN, 0);

    this->setCompilerData(&miniCompiler);
    inFunc = prevInFunc;
    ByteCode& funcCode = miniCompiler.code;
    funcCode.depth = miniCompiler.depth;
    Object func = ALLOC(Function, ObjDealloc<Function>, funcCode, count);

    // We only declare in the current function scope.
    code.loadRegConst(func, previousReg, depth);
    reserveReg();
}

void Compiler::list()
{
    ui8 listReg = previousReg;
    code.addOp(OP_LIST, listReg);
    reserveReg();

    ui8 count = 0;
    ui8 startReg = previousReg;
    auto emitList = [this, listReg, &count, startReg] {
        code.addOp(OP_EXTEND_LIST, listReg, startReg, count);
        previousReg = startReg;
        count = 0;
    };

    if (!checkTok(TOK_RIGHT_BRACKET))
    {
        do {
            expression();
            if (++count == LIST_ENTRY_GROUP)
                emitList();
        } while (consumeTok(TOK_COMMA));
    }

    if (count > 0) emitList();
    MATCH_TOK(TOK_RIGHT_BRACKET, "Expect ']' to conclude list literal.");
}

void Compiler::primary()
{    
    if (consumeToks(TOK_NUM, TOK_NUM_DEC, TOK_STR_LIT))
    {
        Object obj;
        switch (previousTok.type)
        {
            case TOK_NUM:       obj = previousTok.content.i;    break;
            case TOK_NUM_DEC:   obj = previousTok.content.d;    break;
            case TOK_STR_LIT:
                obj = ALLOC(String, ObjDealloc<String>, GET_STR(previousTok));
                break;
            default: UNREACHABLE();
        }
        code.loadRegConst(obj, previousReg, depth);
        reserveReg();
    }

    else if (consumeTok(TOK_RANGE))
    {
        Object obj = ALLOC(Range, ObjDealloc<Range>, constructRange(previousTok.text));
        code.loadRegConst(obj, previousReg, depth);
        reserveReg();
    }

    else if (consumeToks(TOK_TRUE, TOK_FALSE))
    {
        bool value = previousTok.content.b;
        code.loadReg(previousReg, (value ? OP_TRUE : OP_FALSE), depth);
        reserveReg();
    }

    else if (consumeTok(TOK_NULL))
    {
        code.loadReg(previousReg, OP_NULL, depth);
        reserveReg();
    }

    else if (consumeTok(TOK_IDENTIFIER))
    {
        VarInfo pos = getVarInfo(previousTok);
        if (pos.slot != nullptr)
        {
            code.addOp(OP_GET_VAR, pos.depth, previousReg, *(pos.slot));
            reserveReg();
        }
        else
            REPORT_SEMANTIC(previousTok, "Undefined variable '"
                + std::string(previousTok.text) + "'.");
    }

    else if (consumeTok(TOK_LEFT_PAREN))
    {
        expression();
        MATCH_TOK(TOK_RIGHT_PAREN, "Expect closing parenthesis ')'.");
    }

    else if (consumeTok(TOK_IF))
        ifExpr();
    
    else if (consumeTok(TOK_BAR))
        lambda();

    else if (consumeTok(TOK_LEFT_BRACKET))
        list();
    
    else
        REPORT_SYNTAX(currentTok, "Invalid token in current position.");
}

ByteCode& Compiler::compile(const vT& tokens)
{
    code.clear(); // In case we want to reuse the compiler.
    if (tokens.empty()) return code;

    it = tokens.begin();
    currentTok = tokens[0];
    syntaxError = false;
    semanticError = false;
    hitError = false;
    errorCount = 0;

    while (!checkTok(TOK_EOF))
        declaration();

    if (hitError) code.clear();
    return code;
}

#endif