#pragma once
#include "bytecode.h"
#include "common.h"
#include "main_utils.h"
#include "token.h"
#include <stack>
#include <string>
#include <string_view>
#include <vector>

#ifndef ALLOW_REDEFS
    #define ALLOW_REDEFS 1
#endif

class TokCompVarsWrapper;
class TokCompLoopLabels;

class Compiler
{
    // Cannot propagate returns, so we make them macros instead.
    #undef REPORT_SYNTAX
    #define REPORT_SYNTAX(...)                                          \
        do {                                                            \
            hitError = true;                                            \
            if (syntaxError || (errorCount > COMPILE_ERROR_MAX))        \
                return;                                                 \
            else if (errorCount == COMPILE_ERROR_MAX)                   \
            {                                                           \
                FORMAT_PRINT("COMPILATION ERROR MAXIMUM REACHED.\n");   \
                errorCount++;                                           \
                return;                                                 \
            }                                                           \
            CompileError(__VA_ARGS__).report();                         \
            syntaxError = true;                                         \
            errorCount++;                                               \
            return;                                                     \
        } while (false)

    #undef REPORT_SEMANTIC
    #define REPORT_SEMANTIC(...)                                        \
        do {                                                            \
            hitError = true;                                            \
            if (semanticError || (errorCount > COMPILE_ERROR_MAX))      \
                return;                                                 \
            else if (errorCount == COMPILE_ERROR_MAX)                   \
            {                                                           \
                FORMAT_PRINT("COMPILATION ERROR MAXIMUM REACHED.\n");   \
                errorCount++;                                           \
                return;                                                 \
            }                                                           \
            CompileError(__VA_ARGS__).report();                         \
            semanticError = true;                                       \
            errorCount++;                                               \
            return;                                                     \
        } while (false)

    #define MATCH_TOK(...)                      \
        if (!matchError(__VA_ARGS__)) return;

    #define GET_STR(tok)                                \
        normalizeNewlines(                              \
            (tok).text.substr(1, (tok).text.size() - 2) \
        )

    private:
        struct VarInfo
        {
            ui8* slot;
            ui8 scope; // Block scope depth.
            ui8 depth; // Function scope depth.
            bool access;
        };

        ByteCode code;
        Compiler* scopeCompiler;

        Token previousTok;
        Token currentTok;
        vT::const_iterator it;
        ui8 previousReg;
        ui8 scope{0}; // Our current lexical scope depth.
        ui8 depth; // Our current function scope depth.

        std::stack<std::vector<std::string>> varScopes;
        TokCompVarsWrapper* varsWrapper;
        TokCompLoopLabels* labelsWrapper;
        std::vector<ui64>* endJumps{nullptr};
        std::vector<ui64>* breakJumps{nullptr};
        std::vector<ui64>* continueJumps{nullptr};

        bool inMatch{false}, inFunc{false}, fall{false}; // For structures.
        bool syntaxError{false}, semanticError{false}; // We are currently in an error state.
        bool hitError{false}; // Some error was encountered while compiling.
        int errorCount{0};
        bool exprPrint;

        // For registers.
        // Defined here for increased likelihood of inlining.

        inline void freeReg() { previousReg--; }
        inline void reserveReg() { previousReg++; }

        // For variables.

        inline void defVar(std::string name, ui8 reg, bool access);
        inline VarInfo getVarInfo(const Token& token);
        inline bool getAccess(ui8 reg);
        inline void popScope();

        // Utilities.

        inline void nextTok();
        inline bool checkTok(TokenType type);
        inline bool consumeTok(TokenType type);
        template<typename... Type>
        inline bool consumeToks(Type... toks);
        // Reports an error and returns false if the token type was
        // not matched.
        // Return value *may* be used to return early while parsing.
        inline bool matchError(TokenType type, std::string_view message);
        inline bool consumeType();
        // Bring the compiler back to a proper state.
        void reset();
        inline void matchType(std::string_view message = "");

        // Condensed compiling function.
        void compileDescent(void (Compiler::*func)(), TokenType tok, Opcode op);

        // To set up function compilers.
        inline void setCompilerData(Compiler* other);

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
        void compoundAssign(TokenType oper, const VarInfo& pos); // Helper.
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
        void lambda();
        void list();
        void primary();
    
    public:
        Compiler(Compiler* comp = nullptr);
        ~Compiler();

        ByteCode& compile(const vT& tokens);
};