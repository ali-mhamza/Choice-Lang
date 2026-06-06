#pragma once
#include "astnodes.h"
#include "common.h"
#include "diagnostic.h"
#include <string_view>

class Parser
{
    private:
        struct DepthCounter
        {
            static u64 depthCount;
            bool hitError{false};
            bool incremented{false};

            DepthCounter(FileID id, const Token& token);
            ~DepthCounter();
        };

        StmtVec program{};
        FileID id{};

        Token previousTok{}, currentTok{};

        vT::const_iterator it{};
        bool inMatch{false}, inFunc{false}, fall{false};    // For functions and control-flow.
        bool syntaxError{false}, semanticError{false};      // We are currently in an error state.

        // Utilities.

        void nextTok();
        [[nodiscard]] bool checkTok(TokenType type) const;
        bool consumeTok(TokenType type);
        template<typename... Type>
        [[nodiscard]] bool consumeToks(Type... toks);
        [[nodiscard]] bool matchError(TokenType type, std::string_view message);
        bool consumeTypename();
        void consumeType();

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

        void setStmtLocation(StmtUP& stmt, u64 start);
        void setExprLocation(ExprUP& expr, u64 start);

        // Recursive descent parsing functions.

        // Declarations.

        [[nodiscard]] StmtUP declaration();
        [[nodiscard]] StmtUP varDecl();
        [[nodiscard]] StmtUP funcBodyHelper(vT& params);
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
        [[nodiscard]] ExprUP mutation();
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
        [[nodiscard]] ExprUP reference();
        [[nodiscard]] ExprUP call(ExprUP&& expr, u64 start);
        // All post-fix operators.
        [[nodiscard]] ExprUP post();
        [[nodiscard]] ExprUP ifExpr();
        [[nodiscard]] StmtUP lambdaBodyHelper(
            vT& params,
            bool skipParams = false
        );
        [[nodiscard]] ExprUP lambda(bool skipParams);
        [[nodiscard]] ExprUP comprehension();
        [[nodiscard]] ExprUP list();
        [[nodiscard]] ExprUP table();
        [[nodiscard]] ExprUP formatString();
        [[nodiscard]] ExprUP primary();

    public:
        // So it can be modified directly.
        bool hitError{false};

        Parser() = default;
        [[nodiscard]] StmtVec& parseToAST(FileID id, const vT& tokens);
};