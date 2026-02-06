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
        linearTable<std::string, std::vector<ui64>> labels;

        TokCompLoopLabels() = default;
};

Compiler::Compiler() :
    previousReg(0), scope(0),
    varsWrapper(new TokCompVarsWrapper),
    labelsWrapper(new TokCompLoopLabels),
    inMatch(false), fall(false), endJumps(nullptr),
    breakJumps(nullptr), continueJumps(nullptr)
{
    varScopes.emplace();
}

Compiler::~Compiler()
{
    delete varsWrapper;
    delete labelsWrapper;
}

void Compiler::defVar(std::string name, ui8 reg)
{
    varsWrapper->vars[{name, scope}] = reg;
    varScopes.top().push_back(name);
}

void Compiler::defAccess(ui8 reg, bool access)
{
    varsWrapper->access[reg] = access;
}

ui8* Compiler::getVarSlot(const Token& token)
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

bool Compiler::getAccess(ui8 reg)
{
    return *(varsWrapper->access.get(reg));
}

void Compiler::popScope()
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

// Basic implementation.
inline void Compiler::matchError(TokenType type, std::string_view message)
{
    if (!consumeTok(type))
        throw CompileError(currentTok, std::string(message));
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

inline void Compiler::matchType(std::string_view message /* = "" */)
{
    if (!consumeType())
        throw CompileError(currentTok, std::string(message));
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
        code.addOp(op, firstOper, firstOper, secondOper);
        freeReg();
    }
}

void Compiler::declaration()
{
    if (consumeToks(TOK_MAKE, TOK_FIX))
        varDecl();
    else
        statement();
}

void Compiler::varDecl()
{
    TokenType declType = previousTok.type;
    consumeTok(TOK_DEF); // In case it's there.
    matchError(TOK_IDENTIFIER, "Expect variable name.");
    Token name = previousTok;

    ui8* check = getVarSlot(name);
    if (check != nullptr)
    {
         throw CompileError(name, "Variable '" + std::string(name.text)
            + "' is already defined in this scope.");
    }

    if (consumeTok(TOK_COLON))
        matchType("Expect variable type.");

    ui8 slot = previousReg;
    if (consumeTok(TOK_EQUAL))
        expression();
    else if (declType == TOK_FIX)
        throw CompileError(currentTok,
            "Initializer required for fixed-value variable.");
    else
    {
        code.loadReg(slot, OP_NULL);
        reserveReg();
    }
    matchError(TOK_SEMICOLON, "Expect ';' after variable declaration.");

    defVar(
        std::string(name.text),
        slot
    );
    defAccess(slot,
        declType == TOK_MAKE ? accessVar : accessFix);
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
    else if (consumeTok(TOK_LEFT_BRACE))
        blockStmt();
    else if (consumeTok(TOK_BREAK))
        breakStmt();
    else if (consumeTok(TOK_CONT))
    {
        matchError(TOK_SEMICOLON, "Expect ';' after 'continue'.");
        this->continueJumps->push_back(code.addJump(OP_JUMP));
    }
    else if (consumeTok(TOK_FALL))
    {
        if (!inMatch)
            throw CompileError(previousTok, "Invalid instruction 'fallthrough'" \
                " outside of match-is structure.");
        matchError(TOK_SEMICOLON, "Expect ';' after 'fallthrough'.");
        if (!checkTok(TOK_IS) && !checkTok(TOK_RIGHT_BRACE))
            throw CompileError(currentTok,
                "Cannot have a statement following a 'fallthrough' instruction.");
        fall = true;
    }
    else if (consumeTok(TOK_END))
    {
        if (!inMatch)
            throw CompileError(previousTok,
                "Invalid instruction 'end' outside of match-is structure.");
        matchError(TOK_SEMICOLON, "Expect ';' after 'end'.");
        this->endJumps->push_back(code.addJump(OP_JUMP));
    }
    else
        exprStmt();
}

void Compiler::ifStmt()
{
    matchError(TOK_LEFT_PAREN, "Expect '(' after 'if'.");
    ui8 reg = previousReg;
    expression();
    matchError(TOK_RIGHT_PAREN, "Expect ')' after condition.");

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
        matchError(TOK_IDENTIFIER, "Expect loop label after ':'.");
        label = previousTok;
        this->labelsWrapper->labels.add(
            std::string(label.text),
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
            std::string(label.text)
        );

        for (ui64 jump : *vec)
            code.patchJump(jump);
    }
}

void Compiler::whileStmt()
{
    matchError(TOK_LEFT_PAREN, "Expect '(' after 'while'.");
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
    matchError(TOK_RIGHT_PAREN, "Expect ')' after condition.");

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

    matchError(TOK_RIGHT_PAREN, "Expect ')' after condition.");

    Token label; // Default: TOK_EOF.
    if (consumeTok(TOK_COLON))
    {
        matchError(TOK_IDENTIFIER, "Expect loop label after ':'.");
        label = previousTok;
        this->labelsWrapper->labels.add(
            std::string(label.text),
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
            std::string(label.text)
        );
        for (ui64 jump : *vec)
            code.patchJump(jump);
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

    matchError(TOK_LEFT_PAREN, "Expect '(' after 'for'.");
    if (!consumeTok(TOK_IDENTIFIER))
        throw CompileError(currentTok, "Expect loop variable identifier.");
    Token var = previousTok;
    ui8 varReg = previousReg;
    defVar(std::string(var.text), varReg);
    defAccess(varReg, accessFix); // For now.
    reserveReg();

    matchError(TOK_IN, "Expect 'in' keyword after loop identifier.");
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
        throw CompileError(errorToken, "Case value must be a literal.");

    ui8 caseReg = previousReg;
    primary(); // Compile the literal.
    code.addOp(OP_EQUAL, caseReg, matchReg, caseReg);
    ui64 falseJump = code.addJump(OP_JUMP_FALSE, caseReg);
    freeReg();
    matchError(TOK_COLON, "Expect ':' before case body.");

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
    
    matchError(TOK_LEFT_PAREN, "Expect '(' before match value.");
    ui8 matchReg = previousReg;
    expression();
    matchError(TOK_RIGHT_PAREN, "Expect ')' after match value.");
    matchError(TOK_LEFT_BRACE, "Expect '{' before match cases.");

    int caseCount = 0;
    ui64 fallJump = 0; // Invalid jump offset value.
    ui64 emptyJump = 0;

    std::vector<ui64> jumps;
    auto prevEndJumps = endJumps;
    endJumps = &jumps;

    while (!checkTok(TOK_RIGHT_BRACE) && !checkTok(TOK_EOF))
    {
        if (caseCount == MATCH_CASES_MAX)
            throw CompileError(currentTok,
                "Too many cases in match-is structure.");
        
        matchError(TOK_IS, "Expect 'is' before case value.");
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
        if (defaultCase)
        {
            if (consumeTok(TOK_IS))
                throw CompileError(previousTok,
                    "Cannot have another case after the default case.");
            else
                break;
        }
    }

    matchError(TOK_RIGHT_BRACE, "Expect '}' after match-is structure.");
    for (ui64 jump : jumps)
        code.patchJump(jump);
    freeReg(); // Remove the match value.

    inMatch = prevInMatch;
    endJumps = prevEndJumps;
}

void Compiler::repeatStmt()
{
    matchError(TOK_LEFT_BRACE, "Expect '{' before 'repeat' block.");
    ui64 loopStart = code.getLoopStart();
    blockStmt(); // Will consume the '}'.

    matchError(TOK_UNTIL, "Expect 'until' condition after 'repeat'.");
    matchError(TOK_LEFT_PAREN, "Expect '(' before 'until' condition.");
    ui8 reg = previousReg;
    expression();
    matchError(TOK_RIGHT_PAREN, "Expect ')' after 'until' condition.");
    matchError(TOK_SEMICOLON, "Expect ';' after repeat-until block.");

    ui64 trueJump = code.addJump(OP_JUMP_TRUE, reg);
    freeReg();
    code.addLoop(loopStart);
    code.patchJump(trueJump);
}

void Compiler::breakStmt()
{
    if (consumeTok(TOK_IDENTIFIER))
    {
        const Token& label = previousTok;
        auto* vec = this->labelsWrapper->labels.get(
            std::string(label.text)
        );

        if (vec == nullptr)
            throw CompileError(label, "Break label is not assigned to any loop.");
        vec->push_back(code.addJump(OP_JUMP));
    }
    else
        this->breakJumps->push_back(code.addJump(OP_JUMP));

    matchError(TOK_SEMICOLON, "Expect ';' after 'break'.");
}

void Compiler::blockStmt()
{
    scope++;
    varScopes.emplace();
    ui8 origVarReg = previousReg;
    while (!checkTok(TOK_RIGHT_BRACE) && !checkTok(TOK_EOF))
        declaration();
    matchError(TOK_RIGHT_BRACE, "Expect '}' after block.");
    popScope();
    varScopes.pop();
    scope--;
    previousReg = origVarReg;
}

void Compiler::exprStmt()
{
    expression();
    matchError(TOK_SEMICOLON, "Expect ';' after expression.");
    freeReg();
}

void Compiler::expression()
{
    assignment();
}

void Compiler::compoundAssign(TokenType oper, ui8 slot)
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
        if (firstTok.type != TOK_IDENTIFIER)
            throw CompileError(firstTok, "Invalid assignment target.");
        
        nextTok();
        ui8* slot = getVarSlot(previousTok);
        if (slot != nullptr)
        {
            nextTok();
            bool access = getAccess(*slot);
            if (access == accessFix)
                throw CompileError(
                    secondTok, "Cannot assign to a fixed-value variable."
                );
            if (secondTok.type != TOK_EQUAL)
                compoundAssign(secondTok.type, *slot);
            else
            {
                ui8 value = previousReg;
                expression(); // Does not consume the ';'.
                code.addOp(OP_SET_VAR, *slot, value);
            }
        }
        else
            throw CompileError(previousTok, "Undefined variable '"
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

        code.addOp(OP_EQUAL, firstOper, firstOper, secondOper);
        if (oper == TOK_BANG_EQ)
            code.addOp(OP_NOT, firstOper, firstOper);
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
                code.addOp(OP_IN, firstOper, firstOper, secondOper);
                break;
            case TOK_GT:
            case TOK_LT_EQ:
                code.addOp(OP_GT, firstOper, firstOper, secondOper);
                break;
            case TOK_LT:
            case TOK_GT_EQ:
                code.addOp(OP_LT, firstOper, firstOper, secondOper);
                break;
            default: UNREACHABLE();
        }

        if ((oper == TOK_NOT) || (oper == TOK_GT_EQ)
            || (oper == TOK_LT_EQ))
                code.addOp(OP_NOT, firstOper, firstOper);

        freeReg();
    }
}

void Compiler::bitOr()
{
    compileDescent(&Compiler::bitXor, TOK_BAR, OP_BIT_OR);
}

void Compiler::bitXor()
{
    compileDescent(&Compiler::bitAnd, TOK_UARROW, OP_BIT_XOR);
}

void Compiler::bitAnd()
{
    compileDescent(&Compiler::shift, TOK_AMP, OP_BIT_AND);
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

        code.addOp(oper == TOK_RIGHT_SHIFT ? OP_BIT_SHIFT_R
            : OP_BIT_SHIFT_L, firstOper, firstOper, secondOper);
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
            firstOper, firstOper, secondOper);
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
        code.addOp(op, firstOper, firstOper, secondOper);

        freeReg();
    }
}

void Compiler::_crementExpr(TokenType oper, bool prev)
{
    // Temporarily assuming only regular identifier variables.
    matchError(TOK_IDENTIFIER, "Invalid increment/decrement target.");
    ui8* slot = getVarSlot(previousTok);
    if (slot != nullptr)
    {
        bool access = getAccess(*slot);
        if (access == accessFix)
            throw CompileError(
                previousTok, "Cannot modify a fixed-value variable."
            );
        else
        {
            if (prev) // Post-increment/decrement operator(s).
            {
                consumeToks(TOK_INCR, TOK_DECR); // Skip the operator token.
                code.addOp(OP_GET_VAR, previousReg, *slot);
            }
            code.addOp(oper == TOK_INCR ? OP_INCREMENT : OP_DECREMENT,
                *slot, *slot);
            if (!prev)
                code.addOp(OP_GET_VAR, previousReg, *slot);
            reserveReg();
        }
    }
    else
        throw CompileError(previousTok, "Undefined variable '"
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
            case TOK_TILDE: op = OP_BIT_COMP;   break;
            default: UNREACHABLE();
        }

        code.addOp(op, firstOper, firstOper);
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
    const Token& firstTok = currentTok;
    const Token& secondTok = *(it + 1);

    if ((firstTok.type == TOK_IDENTIFIER)
        && ((secondTok.type == TOK_BANG) || (secondTok.type == TOK_LEFT_PAREN)))
    {
        consumeTok(TOK_IDENTIFIER);
        Token funcName = previousTok;
        nextTok();
        bool builtin = (previousTok.type == TOK_BANG ?
            (matchError(TOK_LEFT_PAREN, "Invalid placement for token '!'."), true)
            : false);

        ui8 location;
        if (builtin)
        {
            Natives::FuncType func = Natives::builtins.find(funcName.text)->second;
            location = static_cast<ui8>(func);
        }
        else
            location = *(getVarSlot(funcName));

        ui8 startReg = previousReg;
        ui8 argCount = 0;
        while (!checkTok(TOK_RIGHT_PAREN) && !checkTok(TOK_EOF))
        {
            do {
                expression();
                argCount++;
            } while (consumeTok(TOK_COMMA));
        }
        matchError(TOK_RIGHT_PAREN, "Expect ')' following function arguments.");

        code.addOp(builtin ? OP_CALL_NAT : OP_CALL_DEF,
            location, startReg, argCount);

        previousReg -= argCount - 1;
    }
    else
        post();
}

void Compiler::post()
{
    const Token& firstTok = currentTok;
    const Token& secondTok = *(it + 1);

    if ((firstTok.type == TOK_IDENTIFIER)
        && ((secondTok.type == TOK_INCR) || (secondTok.type == TOK_DECR)))
            _crementExpr(secondTok.type, true);
    else
        primary();
}

void Compiler::ifExpr()
{
    ui8 reg = previousReg;
    matchError(TOK_LEFT_PAREN, "Expect '(' before condition.");
    expression();
    matchError(TOK_RIGHT_PAREN, "Expect ')' after condition.");
    ui64 falseJump = code.addJump(OP_JUMP_FALSE, reg);
    freeReg();

    matchError(TOK_LEFT_BRACE, "Expect '{' before conditional expression.");
    ui8 current = previousReg;
    expression();
    matchError(TOK_RIGHT_BRACE, "Expect '}' after conditional expression.");
    ui64 trueJump = code.addJump(OP_JUMP);
    code.patchJump(falseJump);

    previousReg = current;
    
    if (consumeTok(TOK_ELIF))
        ifExpr();
    else if (consumeTok(TOK_ELSE))
    {
        matchError(TOK_LEFT_BRACE, "Expect '{' before conditional expression.");
        expression();
        matchError(TOK_RIGHT_BRACE, "Expect '}' after conditional expression.");
    }
    else
        throw CompileError(currentTok,
            "A conditional expression must have a false-case branch.");

    code.patchJump(trueJump);
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
        code.loadRegConst(obj, previousReg);
        reserveReg();
    }

    else if (consumeTok(TOK_RANGE))
    {
        Object obj = ALLOC(Range, ObjDealloc<Range>, constructRange(previousTok.text));
        code.loadRegConst(obj, previousReg);
        reserveReg();
    }

    else if (consumeToks(TOK_TRUE, TOK_FALSE))
    {
        bool value = previousTok.content.b;
        code.loadReg(previousReg, (value ? OP_TRUE : OP_FALSE));
        reserveReg();
    }

    else if (consumeTok(TOK_NULL))
    {
        code.loadReg(previousReg, OP_NULL);
        reserveReg();
    }

    else if (consumeTok(TOK_IDENTIFIER))
    {
        ui8* slot = getVarSlot(previousTok);
        if (slot != nullptr)
        {
            code.addOp(OP_GET_VAR, previousReg, *slot);
            reserveReg();
        }
        else
            throw CompileError(previousTok, "Undefined variable '"
                + std::string(previousTok.text) + "'.");
    }

    else if (consumeTok(TOK_LEFT_PAREN))
    {
        expression();
        matchError(TOK_RIGHT_PAREN, "Expect closing parenthesis ')'.");
    }

    else if (consumeTok(TOK_IF))
        ifExpr();
    
    else
        throw CompileError(currentTok, "Invalid token in current position.");
}

ByteCode& Compiler::compile(const vT& tokens)
{
    currentTok = tokens[0];
    it = tokens.begin();
    code.clear();

    try
    {
        while (!checkTok(TOK_EOF))
            declaration();
    }
    catch (CompileError& error)
    {
        error.report();
        code.clear();
    }

    return code;
}

#endif