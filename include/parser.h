#pragma once
#include "astnodes.h"
#include "common.h"     // For vT.
#include <string_view>

class Parser
{
    private:
        StmtVec program;
        Token previousTok;
        Token currentTok;
        vT::const_iterator it;
        bool inMatch, inFunc, fall; // For structures.
        bool syntaxError, semanticError; // We are currently in an error state.
        bool hitError;

        // Utilities.

        void nextTok();
        bool checkTok(TokenType type);
        bool consumeTok(TokenType type);
        template<typename... Type>
        bool consumeToks(Type... toks);
        bool matchError(TokenType type, std::string_view message);
        bool consumeType();
        void matchType(std::string_view message = "");
        // Bring the compiler back to a proper state.
        void reset();

        // Recursive descent parsing functions.

        // Declarations.

        StmtUP declaration();
        StmtUP varDecl();
        // skipParams: Since || is scanned as a single token, we use
        // this to indicate that the parser should assume no parameters.
        StmtUP funcBodyHelper(
            bool lambda,
            vT& params,
            bool skipParams = false
        );
        StmtUP funDecl();
        StmtUP classDecl();

        // Statements.

        StmtUP statement();
        StmtUP ifStmt();
        StmtUP whileStmt();
        StmtUP forStmt();
        StmtUP matchStmt();
        StmtUP repeatStmt();
        StmtUP returnStmt();
        StmtUP breakStmt();
        StmtUP continueStmt();
        StmtUP blockStmt();
        StmtUP exprStmt();

        // Expressions.

        ExprUP tuple();
        ExprUP expression();
        ExprUP assignment();
        ExprUP logicOr();
        ExprUP logicAnd();
        ExprUP equality();
        ExprUP comparison();
        ExprUP bitOr();
        ExprUP bitXor();
        ExprUP bitAnd();
        ExprUP shift();
        ExprUP sum();
        ExprUP product();
        ExprUP unary();
        ExprUP exponent();
        ExprUP call();
        ExprUP post(); // Post-increment/decrement.
        ExprUP ifExpr();
        ExprUP lambda(bool skipParams);
        ExprUP comprehension();
        ExprUP list();
        ExprUP primary();
    
    public:
        int errorCount; // So it can be modified directly.

        Parser();
        StmtVec& parseToAST(const vT& tokens);
};