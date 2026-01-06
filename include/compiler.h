#pragma once
#include "bytecode.h"
#include "common.h"
#include "main_utils.h"
#include "token.h"
#include <string_view>
#include <string>
#include <vector>

class TokCompVarsWrapper;

class Compiler
{
    #define GET_STR(tok) \
        normalizeNewlines( \
            (tok).text.substr(1, (tok).text.size() - 2) \
        )
    
    private:
        ByteCode code;
        // const vT& tokens;
        Token previousTok;
        Token currentTok;
        vT::const_iterator it;
        ui8 previousReg;
        ui8 scope; // Our current lexical scope depth.
        std::vector<std::vector<std::string>> varScopes;
        TokCompVarsWrapper* varsWrapper;

        // For registers.
        // Defined here for increased likelihood of inlining.

        inline void freeReg() { previousReg--; }
        inline void reserveReg() { previousReg++; }

        // For variables.

        void defVar(std::string name, ui8 reg);
        void defAccess(ui8 reg, bool access);
        ui8* getVarSlot(const Token& token);
        bool getAccess(ui8 reg);
        void popScope();

        // Utilities.

        inline void nextTok();
        inline bool checkTok(TokenType type);
        inline bool consumeTok(TokenType type);
        template<typename... Type>
        inline bool consumeToks(Type... toks);
        inline void matchError(TokenType type, std::string_view message);
        inline bool consumeType();
        // Bring the compiler back to a proper state.
        void reset();
        inline void matchType(std::string_view message = "");

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
        void ifStmt();
        void whileStmt();
        void repeatStmt();
        void returnStmt();
        void blockStmt();
        void exprStmt();

        // Expressions.

        void expression();
        void compoundAssign(TokenType oper, ui8 slot); // Helper.
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
        void _crementExpr(TokenType oper);
        void unary();
        void exponent();
        void call();
        void ifExpr();
        void primary();
    
    public:
        Compiler();
        ~Compiler();

        ByteCode& compile(const vT& tokens);
};