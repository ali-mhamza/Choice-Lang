#include "../include/compiler.h"
#include "../include/error.h"
#include "../include/object.h"
#include "../include/opcodes.h"
#include "../include/token.h"
#include <iostream>
#include <memory>
#include <variant>
using namespace Object;

Compiler::Compiler(const vT& tokens) :
    /*tokens(tokens),*/ currentTok(tokens[0]),
    it(tokens.begin()) {}

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
        throw CompileError(currentTok, message);
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
        throw CompileError(currentTok, message);
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
    Token declType = previousTok;
    consumeTok(TOK_DEF); // In case it's there.

    matchError(TOK_IDENTIFIER, "Expect variable name.");
    // Token name = previousTok;

    if (consumeTok(TOK_COLON))
        matchType("Expect variable type.");

    if (consumeTok(TOK_EQUAL))
        expression();
    else if (declType.type == TOK_FIX)
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
    product();
    while (consumeToks(TOK_PLUS, TOK_MINUS))
    {
        Token oper = previousTok;
        product();

        switch (oper.type)
        {
            case TOK_PLUS:  code.addByte(OP_ADD); break;
            case TOK_MINUS: code.addByte(OP_SUB); break;
            default: ;
        }
    }
}

void Compiler::product()
{
    primary();
    while (consumeToks(TOK_STAR, TOK_SLASH, TOK_PERCENT))
    {
        Token oper = previousTok;
        primary();

        switch (oper.type)
        {
            case TOK_STAR:      code.addByte(OP_MULT); break;
            case TOK_SLASH:     code.addByte(OP_DIV); break;
            case TOK_PERCENT:   code.addByte(OP_MOD); break;
            default: ;
        }
    }
}

void Compiler::primary()
{
    if (consumeTok(TOK_NUM_INT))
    {
        long temp = std::get<long>(previousTok.content);
        Int<int8_t> value(static_cast<int8_t>(temp));
        BaseUP ptr = std::make_unique<Int<int8_t>>(value);
        code.addConst(std::move(ptr));
    }

    else if (consumeTok(TOK_NUM_DEC))
    {
        Dec<double> value = std::get<double>(previousTok.content);
        BaseUP ptr = std::make_unique<Dec<double>>(value);
        code.addConst(std::move(ptr));
    }

    else if (consumeTok(TOK_STR_LIT))
    {
        String value = std::get<std::string_view>(previousTok.content);
        BaseUP ptr = std::make_unique<String>(value);
        code.addConst(std::move(ptr));
    }

    else if (consumeToks(TOK_TRUE, TOK_FALSE))
    {
        bool value = std::get<bool>(previousTok.content);
        code.addByte(value ? OP_TRUE : OP_FALSE);
    }

    else if (consumeTok(TOK_NULL))
        code.addByte(OP_NULL);
}

ByteCode& Compiler::compile()
{
    try
    {
        while (!checkTok(TOKEN_EOF))
            declaration();
        code.addByte(OP_RETURN);
    }
    catch (CompileError& error)
    {
        error.report();
    }
    return code;
}