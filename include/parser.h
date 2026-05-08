#pragma once
#include "astnodes.h"
#include "common.h"
#include "diagnostic.h"
#include <string_view>

class Parser
{
    private:
        StmtVec program{};
        FileID id{};
        DiagnosticEngine* engine{};

        Token previousTok{}, currentTok{};

        vT::const_iterator it{};
        bool inMatch{false}, inFunc{false}, fall{false};    // For structures.
        bool syntaxError{false}, semanticError{false};      // We are currently in an error state.

        // Utilities.

        void nextTok();
        [[nodiscard]] bool checkTok(TokenType type) const;
        bool consumeTok(TokenType type);
        template<typename... Type>
        [[nodiscard]] bool consumeToks(Type... toks);
        [[nodiscard]] bool matchError(TokenType type, std::string_view message);
        [[nodiscard]] bool consumeType();
        void matchType(std::string_view message = "");
        // Bring the compiler back to a proper state.
        void reset();
        void reportSyntax(
            DiagCode code,
            const Token& token,
            std::string_view message = ""
        );
        void reportSemantic(
            DiagCode code,
            const Token& token,
            std::string_view message = ""
        );

        // Recursive descent parsing functions.

        // Declarations.

        [[nodiscard]] StmtUP declaration();
        [[nodiscard]] StmtUP varDecl();
        // skipParams: Since || is scanned as a single token, we use
        // this to indicate that the parser should assume no parameters.
        [[nodiscard]] StmtUP funcBodyHelper(
            bool lambda,
            vT& params,
            bool skipParams = false
        );
        [[nodiscard]] StmtUP funDecl();
        [[nodiscard]] StmtUP classDecl();

        // Statements.

        [[nodiscard]] StmtUP statement();
        [[nodiscard]] StmtUP ifStmt();
        [[nodiscard]] StmtUP whileStmt();
        [[nodiscard]] StmtUP forStmt();
        [[nodiscard]] StmtUP matchStmt();
        [[nodiscard]] StmtUP repeatStmt();
        [[nodiscard]] StmtUP returnStmt();
        [[nodiscard]] StmtUP breakStmt();
        [[nodiscard]] StmtUP continueStmt();
        [[nodiscard]] StmtUP blockStmt();
        [[nodiscard]] StmtUP exprStmt();

        // Expressions.

        [[nodiscard]] ExprUP returnExpr();
        [[nodiscard]] ExprUP expression();
        [[nodiscard]] ExprUP assignment();
        [[nodiscard]] ExprUP range();
        [[nodiscard]] ExprUP logicOr();
        [[nodiscard]] ExprUP logicAnd();
        [[nodiscard]] ExprUP equality();
        [[nodiscard]] ExprUP comparison();
        [[nodiscard]] ExprUP bitOr();
        [[nodiscard]] ExprUP bitXor();
        [[nodiscard]] ExprUP bitAnd();
        [[nodiscard]] ExprUP shift();
        [[nodiscard]] ExprUP sum();
        [[nodiscard]] ExprUP product();
        [[nodiscard]] ExprUP unary();
        [[nodiscard]] ExprUP exponent();
        [[nodiscard]] ExprUP call();
        [[nodiscard]] ExprUP post(); // Post-increment/decrement.
        [[nodiscard]] ExprUP ifExpr();
        [[nodiscard]] ExprUP lambda(bool skipParams);
        [[nodiscard]] ExprUP comprehension();
        [[nodiscard]] ExprUP list();
        [[nodiscard]] ExprUP formatString();
        [[nodiscard]] ExprUP primary();

    public:
        bool hitError{false};
        int errorCount{0}; // So it can be modified directly.

        Parser(DiagnosticEngine* engine);
        [[nodiscard]] StmtVec& parseToAST(FileID id, const vT& tokens);
};