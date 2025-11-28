#include "../include/altcompiler.h"
#include "../include/error.h"
#include "../include/object.h"
#include "../include/opcodes.h"
#include "../include/token.h"
#include <cstring>
#include <iostream>
#include <memory>
#include <variant>
using namespace Object;

#define REG_DUMP

AltCompiler::AltCompiler(const vT& tokens) :
    /*tokens(tokens),*/ currentTok(tokens[0]), it(tokens.begin()),
    previousReg(0), currentReg(1)
{
    std::memset(&registers[0], 0, regSize);
}

void AltCompiler::freeReg()
{
    previousReg--;
    currentReg--;
}

void AltCompiler::reserveReg()
{
    previousReg = currentReg;
    currentReg++;
}

void AltCompiler::nextTok()
{
    if (currentTok.type != TOKEN_EOF)
    {
        previousTok = currentTok;
        currentTok = *(++it);
    }
}

bool AltCompiler::checkTok(TokenType type)
{
    return (currentTok.type == type);
}

bool AltCompiler::consumeTok(TokenType type)
{
    if (checkTok(type))
    {
        nextTok();
        return true;
    }

    return false;
}

template<typename... Type>
bool AltCompiler::consumeToks(Type... toks)
{
    for (TokenType type : {toks ...})
    {
        if (checkTok(type))
            return consumeTok(type);
    }

    return false;
}

// Basic implementation.
void AltCompiler::matchError(TokenType type, std::string_view message)
{
    if (!consumeTok(type))
        throw CompileError(currentTok, message);
}

bool AltCompiler::consumeType()
{
    for (int i = TOK_INT; i <= TOK_ANY; i++)
    {
        TokenType type = static_cast<TokenType>(i);
        if (checkTok(type))
            return consumeTok(type);
    }

    return false;
}

void AltCompiler::matchType(std::string_view message /* = "" */)
{
    if (!consumeType())
        throw CompileError(currentTok, message);
}

void AltCompiler::declaration()
{
    if (consumeToks(TOK_MAKE, TOK_FIX))
        varDecl();
    else
        statement();
}

void AltCompiler::varDecl()
{
    Token declType = previousTok;
    consumeTok(TOK_DEF); // In case it's there.

    matchError(TOK_IDENTIFIER, "Expect variable name.");
    // Token name = previousTok;

    if (consumeTok(TOK_COLON))
        matchType("Expect variable type.");

    if (consumeTok(TOK_EQUAL))
    {
        // ...
    }
    else if (declType.type == TOK_FIX)
        throw CompileError(currentTok,
            "Initializer required for fixed-value variable.");
    matchError(TOK_SEMICOLON, "Expect ';' after variable declaration.");
}

void AltCompiler::statement()
{
    exprStmt();
}

void AltCompiler::exprStmt()
{
    expression();
    matchError(TOK_SEMICOLON, "Expect ';' after expression.");
}

void AltCompiler::expression()
{
    sum();
}

void AltCompiler::sum()
{
    uint8_t firstOper = previousReg;
    product();

    while (consumeToks(TOK_PLUS, TOK_MINUS))
    {
        Token oper = previousTok;
        uint8_t secondOper = previousReg;
        product();

        switch (oper.type)
        {
            case TOK_PLUS:
            {
                code.addOp(OP_ADD, firstOper, firstOper, secondOper);
                // code.addOp(OP_FREE_R, secondOper);
                break;
            }
            case TOK_MINUS:
            {
                code.addOp(OP_SUB, firstOper, firstOper, secondOper);
                // code.addOp(OP_FREE_R, secondOper);
                break;
            }
            default: ;
        }

        freeReg();
    }
}

void AltCompiler::product()
{
    uint8_t firstOper = previousReg;
    primary();

    while (consumeToks(TOK_STAR, TOK_SLASH, TOK_PERCENT))
    {
        Token oper = previousTok;
        uint8_t secondOper = previousReg;
        primary();

        switch (oper.type)
        {
            case TOK_STAR:
            {
                code.addOp(OP_MULT, firstOper, firstOper, secondOper);
                // code.addOp(OP_FREE_R, secondOper);
                break;
            }
            case TOK_SLASH:
            {
                code.addOp(OP_DIV, firstOper, firstOper, secondOper);
                // code.addOp(OP_FREE_R, secondOper);
                break;
            }
            case TOK_PERCENT:
            {
                code.addOp(OP_MOD, firstOper, firstOper, secondOper);
                // code.addOp(OP_FREE_R, secondOper);
                break;
            }
            default: ;
        }

        freeReg();
    }
}

void AltCompiler::primary()
{
    if (consumeTok(TOK_NUM_INT))
    {
        long temp = std::get<long>(previousTok.content);
        Int<int8_t> value(static_cast<int8_t>(temp));
        BaseUP ptr = std::make_unique<Int<int8_t>>(value);
        code.loadRegConst(std::move(ptr), previousReg);
        registers[previousReg] = OBJ_INT;
        reserveReg();
    }

    else if (consumeTok(TOK_NUM_DEC))
    {
        Dec<double> value = std::get<double>(previousTok.content);
        BaseUP ptr = std::make_unique<Dec<double>>(value);
        code.loadRegConst(std::move(ptr), previousReg);
        registers[previousReg] = OBJ_DEC;
        reserveReg();
    }

    else if (consumeTok(TOK_STR_LIT))
    {
        String value = std::get<std::string_view>(previousTok.content);
        BaseUP ptr = std::make_unique<String>(value);
        code.loadRegConst(std::move(ptr), previousReg);
        registers[previousReg] = OBJ_STRING;
        reserveReg();
    }

    else if (consumeToks(TOK_TRUE, TOK_FALSE))
    {
        bool value = std::get<bool>(previousTok.content);
        code.loadReg(previousReg, (value ? OP_TRUE : OP_FALSE));
        registers[previousReg] = OBJ_BOOL;
        reserveReg();
    }

    else if (consumeTok(TOK_NULL))
    {
        code.loadReg(previousReg, OP_NULL);
        registers[previousReg] = OBJ_NULL;
        reserveReg();
    }
}

ByteCode& AltCompiler::compile()
{
    try
    {
        while (!checkTok(TOKEN_EOF))
            declaration();
        code.addOp(OP_RETURN, 0); // Temporary register value.
        
        // Tracking the value types we keep in our
        // registers will help us later to implement
        // a basic form of constant folding.
        #ifdef REG_DUMP
        std::cout << "REGISTER: ";
        for (uint8_t i = 0; i < currentReg; i++)
        {
            std::cout << "[";
            switch (registers[i])
            {
                case OBJ_INT:   	std::cout << "INT";     break;
                case OBJ_UINT:  	std::cout << "UINT";    break;
                case OBJ_DEC:       std::cout << "DEC";     break;
                case OBJ_STRING:    std::cout << "STRING";  break;
                case OBJ_BOOL:		std::cout << "BOOL";	break;
                case OBJ_NULL:		std::cout << "NULL";	break;
                default:; // Unreachable.
            }
            std::cout << "]";
        }
        std::cout << "\n";
        #endif
    }
    catch (CompileError& error)
    {
        error.report();
    }

    return code;
}