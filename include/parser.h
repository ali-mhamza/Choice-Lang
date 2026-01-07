#pragma once
#include "astnodes.h"

class Parser
{
    private:
        StmtVec program;
        Token previousTok;
        Token currentTok;
        vT::const_iterator it;
        bool inMatch, fall, end; // For match-is structures.

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
        TokenType matchType(std::string_view message = "");

        // Recursive descent parsing functions.

        // Declarations.

        StmtUP declaration();
        StmtUP varDecl();
        StmtUP funDecl();
        StmtUP classDecl();

        // Statements.

        StmtUP statement();
        StmtUP ifStmt();
        StmtUP whileStmt();
        StmtUP matchStmt();
        StmtUP repeatStmt();
        StmtUP returnStmt();
        StmtUP blockStmt();
        StmtUP exprStmt();

        // Expressions.

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
        ExprUP ifExpr();
        ExprUP primary();
    
    public:
        Parser();
        StmtVec& parseToAST(const vT& tokens);
};