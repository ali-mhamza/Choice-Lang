#include "../include/compiler.h"
#include "../include/common.h"
#include "../include/error.h"
#include "../include/object.h"
#include "../include/opcodes.h"
#include "../include/token.h"
#include <cstring>
#include <iostream>
#include <memory>
#include <variant>

Compiler::Compiler() :
    previousReg(0), currentReg(1) {}

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

void Compiler::nextTok()
{
    if (currentTok.type != TOKEN_EOF)
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
    // Token name = previousTok;

    if (consumeTok(TOK_COLON))
        matchType("Expect variable type.");

    if (consumeTok(TOK_EQUAL))
    {
        // ...
    }
    else if (declType == TOK_FIX)
        throw CompileError(currentTok,
            "Initializer required for fixed-value variable.");
    matchError(TOK_SEMICOLON, "Expect ';' after variable declaration.");
}

void Compiler::statement()
{
    exprStmt();
}

void Compiler::exprStmt()
{
    expression();
    matchError(TOK_SEMICOLON, "Expect ';' after expression.");
}

void Compiler::expression()
{
    sum();
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
    primary();

    while (consumeToks(TOK_STAR, TOK_SLASH, TOK_PERCENT))
    {
        TokenType oper = previousTok.type;
        ui8 secondOper = previousReg;
        primary();

        Opcode op;
        switch (oper)
        {
            case TOK_STAR:      op = OP_MULT;   break;
            case TOK_SLASH:     op = OP_DIV;    break;
            case TOK_PERCENT:   op = OP_MOD;    break;
            default: UNREACHABLE();
        }
        code.addOp(op, firstOper, firstOper, secondOper);
        // code.addOp(OP_FREE_R, secondOper);

        freeReg();
    }
}

void Compiler::primary()
{
    if (consumeTok(TOK_NUM))
    {
        Object obj{previousTok.content.i};
        code.loadRegConst(obj, previousReg);
        reserveReg();
    }

    else if (consumeTok(TOK_NUM_DEC))
    {
        Object obj{previousTok.content.d};
        code.loadRegConst(obj, previousReg);
        reserveReg();
    }
    
    else if (consumeTok(TOK_STR_LIT))
    {
        HeapObj* ptr = new String(GET_STR(previousTok));
        Object obj{ptr};
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
}

ByteCode& Compiler::compile(const vT& tokens)
{
    currentTok = tokens[0];
    it = tokens.begin();
    try
    {
        while (!checkTok(TOKEN_EOF))
            declaration();
        code.addOp(OP_RETURN, static_cast<ui8>(0)); // Temporary register value.
    }
    catch (CompileError& error)
    {
        error.report();
    }

    return code;
}