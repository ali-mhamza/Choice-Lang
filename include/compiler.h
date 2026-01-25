#pragma once
#include "bytecode.h"
#include "common.h"
#include "main_utils.h"
#include "token.h"
#include <string_view>
#include <string>
#include <vector>

class TokCompVarsWrapper;
class TokCompLoopLabels;

class Compiler
{
    #define GET_STR(tok) \
        normalizeNewlines( \
            (tok).text.substr(1, (tok).text.size() - 2) \
        )
    
    private:
        ByteCode code;
        Token previousTok;
        Token currentTok;
        vT::const_iterator it;
        ui8 previousReg;
        ui8 scope; // Our current lexical scope depth.
        std::vector<std::vector<std::string>> varScopes;
        TokCompVarsWrapper* varsWrapper;
        TokCompLoopLabels* labelsWrapper;
        std::vector<ui64> *endJumps, *breakJumps, *continueJumps;
        bool inMatch, fall;

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
        void whileLoopHelper(ui64 loopStart, ui64 falseJump);
        void whileStmt();
        void forLoopHelper(ui8 varReg, ui8 iterReg);
        void forStmt();
        ui64 matchCaseHelper(const ui8 matchReg, ui64& fallJump,
            ui64& emptyJump); // For individual cases.
        void matchStmt();
        void repeatStmt();
        void returnStmt();
        void breakStmt();
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
        // prev: Whether or not it evaluates to the previous
        // value in the register (like with post-increment/
        // decrement operators) or the new value.
        void _crementExpr(TokenType oper, bool prev);
        void unary();
        void exponent();
        void call();
        void post(); // Post-increment/decrement.
        void ifExpr();
        void primary();
    
    public:
        Compiler();
        ~Compiler();

        ByteCode& compile(const vT& tokens);
};