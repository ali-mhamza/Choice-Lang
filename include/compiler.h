#pragma once
#include "bytecode.h"
#include "common.h"
#include "token.h"
#include <string_view>
#include <string>
#include <vector>

class TokCompVarsWrapper;

class Compiler
{
    #define GET_STR(tok) \
        std::string((tok).text.substr(1, (tok).text.size() - 2))
    
    private:
        ByteCode code;
        // const vT& tokens;
        Token previousTok;
        Token currentTok;
        vT::const_iterator it;
        ui8 previousReg;
        ui8 currentReg;
        // The last register that contains a variable (initially 0).
        ui8 lastVarReg;
        ui8 scope; // Our current lexical scope depth.
        std::vector<std::vector<std::string>> varScopes;
        TokCompVarsWrapper* varsWrapper;

        // For registers.

        void freeReg();
        void reserveReg();

        // For variables.

        void defVar(std::string name, ui8 reg);
        ui8* getVarSlot(const Token& token);
        void popScope();

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

        // Condensed compiling function.
        void compileDescent(void (Compiler::*func)(), TokenType tok, Opcode op);

        // Recursive descent parsing functions.

        // Declarations.

        void declaration();
        void varDecl();
        void funDecl();
        void classDecl();

        // Statements.

        void statement();
        void returnStmt();
        void loopStmt();
        void blockStmt();
        void exprStmt();

        // Expressions.

        void expression();
        void assignment();
        void logicOr();
        void logicAnd();
        void equality();
        void comparison();
        void bitOr();
        void bitXor();
        void bitAnd();
        void shift();
        void sum();
        void product();
        void unary();
        void exponent();
        void call();
        void primary();
    
    public:
        Compiler();
        ~Compiler();

        ByteCode& compile(const vT& tokens);
};