#pragma once
#include "bytecode.h"
#include "common.h"
#include "token.h"
#include <string_view>

class Compiler
{
    #define GET_TOK_V(token, type) GETV(token.content, type)
    #define VAL_PTR(val, type) std::make_unique<type>(val)
    #define TOK_VAL_PTR(token, origType, newType) \
		VAL_PTR(GET_TOK_V(token, origType), newType)
    
    private:
        ByteCode code;
        // const vT& tokens;
        Token previousTok;
        Token currentTok;
        vT::const_iterator it;
        static constexpr int regSize = 256;
        ui8 previousReg;
        ui8 currentReg;

        // For registers.
        void freeReg();
        void reserveReg();

        // Utilities.

        void nextTok();
        bool checkTok(TokenType type);
        bool consumeTok(TokenType type);
        template<typename... Type>
        bool consumeToks(Type... toks);
        void matchError(TokenType type, std::string_view message);
        bool consumeType();
        // Bring the compiler back to a proper state.
        void reset();
        void matchType(std::string_view message = "");

        // Recursive descent parsing functions.

        void declaration();

        // Declarations.

        void varDecl();

        void statement();
        void exprStmt();

        void expression();
        void sum();
        void product();
        void primary();
    
    public:
        Compiler();
        ByteCode& compile(const vT& tokens);
};