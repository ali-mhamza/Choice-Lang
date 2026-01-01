#ifndef COMP_AST

#include "chainTable.h"
#include "../include/compiler.h"
#include "../include/common.h"
#include "../include/error.h"
#include "../include/object.h"
#include "../include/opcodes.h"
#include "../include/token.h"
#include "../include/vartable.h"
#include <cstring>
#include <iostream>
#include <memory>
#include <variant>

class TokCompVarsWrapper
{
    public:
        chainTable<VarEntry, ui8, VarHasher> vars;

        TokCompVarsWrapper() = default;
};

Compiler::Compiler() :
    previousReg(0), currentReg(1), lastVarReg(0),
    scope(0), varScopes(1),
    varsWrapper(new TokCompVarsWrapper) {}

Compiler::~Compiler()
{
    delete varsWrapper;
}

void Compiler::freeReg()
{
    previousReg--;
    currentReg--;
}

void Compiler::reserveReg()
{
    previousReg = currentReg;
    currentReg++;
}

void Compiler::defVar(std::string name, ui8 reg)
{
    varsWrapper->vars[{name, scope}] = reg;
    varScopes.back().push_back(name);
}

ui8* Compiler::getVarSlot(const Token& token)
{
    VarEntry entry(token.text, scope);
    return varsWrapper->vars.get(entry);
}

void Compiler::popScope()
{
    for (std::string& var : varScopes.back())
        varsWrapper->vars.remove({var, scope});
}

void Compiler::nextTok()
{
    if (currentTok.type != TOK_EOF)
    {
        previousTok = currentTok;
        currentTok = *(++it);
    }
}

bool Compiler::checkTok(TokenType type)
{
    return (currentTok.type == type);
}

bool Compiler::consumeTok(TokenType type)
{
    if (checkTok(type))
    {
        nextTok();
        return true;
    }

    return false;
}

template<typename... Type>
bool Compiler::consumeToks(Type... toks)
{
    for (TokenType type : {toks ...})
    {
        if (checkTok(type))
            return consumeTok(type);
    }

    return false;
}

// Basic implementation.
void Compiler::matchError(TokenType type, std::string_view message)
{
    if (!consumeTok(type))
        throw CompileError(currentTok, std::string(message));
}

bool Compiler::consumeType()
{
    for (int i = TOK_INT; i <= TOK_ANY; i++)
    {
        TokenType type = static_cast<TokenType>(i);
        if (checkTok(type))
            return consumeTok(type);
    }

    return false;
}

void Compiler::matchType(std::string_view message /* = "" */)
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

    if (consumeTok(TOK_COLON))
        matchType("Expect variable type.");

    if (consumeTok(TOK_EQUAL))
        expression();
    else if (declType == TOK_FIX)
        throw CompileError(currentTok,
            "Initializer required for fixed-value variable.");
    else
    {
        code.loadReg(lastVarReg, OP_NULL);
        reserveReg();
    }
    matchError(TOK_SEMICOLON, "Expect ';' after variable declaration.");

    defVar(
        std::string(name.text),
        lastVarReg
    );
    lastVarReg++;
}

void Compiler::statement()
{
    if (consumeTok(TOK_LEFT_BRACE))
        blockStmt();
    else
        exprStmt();
}

void Compiler::blockStmt()
{
    scope++;
    ui8 origVarReg = lastVarReg;
    while (!checkTok(TOK_RIGHT_BRACE) && !checkTok(TOK_EOF))
        declaration();
    matchError(TOK_RIGHT_BRACE, "Expect '}' after block.");
    popScope();
    scope--;
    previousReg -= (lastVarReg - origVarReg);
    currentReg -= (lastVarReg - origVarReg);
    lastVarReg = origVarReg;
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

void Compiler::assignment()
{
    // For the time being, we just look for direct
    // identifier names.
    // This would need to be modified when other types
    // of "variables" are introduced, like class fields.

    // Bit of a hack.
    const Token& firstTok = currentTok;
    const Token& secondTok = *(it + 1);

    if ((firstTok.type == TOK_IDENTIFIER)
        && (secondTok.type == TOK_EQUAL))
    {
        nextTok();
        ui8* slot = getVarSlot(previousTok);
        nextTok();
        if (slot != nullptr)
        {
            ui8 value = previousReg;
            expression(); // Does not consume the ';'.
            code.addOp(OP_SET_VAR, *slot, value);
        }
        freeReg();
    }
    else
        logicOr();
}

void Compiler::logicOr()
{
    compileDescent(&Compiler::logicAnd, TOK_BAR_BAR, OP_OR);
}

void Compiler::logicAnd()
{
    compileDescent(&Compiler::equality, TOK_AMP_AMP, OP_AND);
}

void Compiler::equality()
{
    compileDescent(&Compiler::comparison, TOK_EQ_EQ, OP_EQUAL);
}

void Compiler::comparison()
{
    ui8 firstOper = previousReg;
    bitOr();

    while (consumeToks(TOK_GT, TOK_GT_EQ, TOK_LT, TOK_LT_EQ))
    {
        TokenType oper = previousTok.type;
        ui8 secondOper = previousReg;
        bitOr();

        switch (oper)
        {
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

        if ((oper == TOK_GT_EQ) || (oper == TOK_LT_EQ))
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

void Compiler::unary()
{
    if (consumeToks(TOK_MINUS, TOK_BANG, TOK_TILDE))
    {
        TokenType oper = previousTok.type;
        ui8 firstOper = previousReg;
        exponent();

        Opcode op;
        switch (oper)
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
    else
        exponent();
}

void Compiler::exponent()
{
    compileDescent(&Compiler::primary, TOK_STAR_STAR, OP_POWER);
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
                obj = static_cast<HeapObj*>(new String(GET_STR(previousTok)));
                break;
            default: UNREACHABLE();
        }
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
    }
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
        if (!tokens.empty())
            code.addOp(OP_RETURN, static_cast<ui8>(previousReg));
    }
    catch (CompileError& error)
    {
        error.report();
    }

    return code;
}

#endif