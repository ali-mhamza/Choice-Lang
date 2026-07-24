#pragma once
#include "astnodes.h"
#include "attributes.h"
#include "common.h"
#include "diagnostic.h"
#include <string_view>
#include <utility>

class Parser
{
    private:
        struct DepthCounter
        {
            static u64 exprDepthCount;
            static u64 blockDepthCount;
            bool hitError{false};
            bool incrementedExpr{false}, incrementedBlock{false};

            DepthCounter(FileID id, const Token& token);
            ~DepthCounter();

            void checkExprDepth(FileID id, const Token& token);
            void checkBlockDepth(FileID id, const Token& token);
            static void reset();
        };

        StmtVec program{};
        FileID id{};

        Token previousTok{}, currentTok{};

        vT::const_iterator it{};
        // For functions and control-flow.
        bool inFunc{false}, inConstructor{false};
        bool inLoop{false}, inComprehension{false};
        bool inMatch{false}, fallthrough{false};
        // We are currently in an error state.
        bool syntaxError{false}, semanticError{false};
        // Currently parsing lambda parameter list (don't consume
        // the closing '|' except if inside grouping expression).
        bool inLambdaParams{false};

        // Utilities.

        void nextTok();
        [[nodiscard]] bool checkTok(TokenType type) const;
        bool consumeTok(TokenType type);
        template<typename... Type>
        [[nodiscard]] bool consumeToks(Type... toks);
        [[nodiscard]] bool matchError(TokenType type, std::string_view message);
        bool consumeTypename();
        void consumeType();
        // To skip erroneous 'elif' or 'else' blocks.
        void skipOrphanedConditionalBranch();

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

        [[nodiscard]] std::pair<VarAttr, vT> consumeAttributes();
        void parseVariableList(
            vT& vars,
            AST::UnpackState& unpack,
            std::string_view errorMsg
        );

        [[nodiscard]] StmtUP declaration();
        [[nodiscard]] StmtUP varDecl();
        [[nodiscard]] bool parseParams(std::vector<AST::Param>& params);
        [[nodiscard]] StmtUP funcBodyHelper(std::vector<AST::Param>& params);
        [[nodiscard]] StmtUP funcDecl();
        [[nodiscard]] StmtUP typeDecl();

        // Statements.

        [[nodiscard]] StmtUP statement();
        [[nodiscard]] AST::Statement::UseStmt::Entry parseModuleEntry();
        [[nodiscard]] StmtUP useStmt();
        [[nodiscard]] StmtUP ifStmt();
        [[nodiscard]] StmtUP whileStmt();
        [[nodiscard]] AST::LoopHeader parseLoopHeader();
        [[nodiscard]] StmtUP forStmt();
        [[nodiscard]] StmtUP matchStmt();
        [[nodiscard]] StmtUP repeatStmt();
        [[nodiscard]] StmtUP returnStmt();
        [[nodiscard]] StmtUP breakStmt();
        [[nodiscard]] StmtUP continueStmt();
        [[nodiscard]] StmtUP blockStmt();
        [[nodiscard]] StmtUP exprStmt();

        // Expressions.

        [[nodiscard]] ExprUP multiAssignment();
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
        [[nodiscard]] ExprUP post(); // All post-fix operators.
        [[nodiscard]] ExprUP ifExpr();
        [[nodiscard]] StmtUP lambdaBodyHelper(
            std::vector<AST::Param>& params,
            bool skipParams = false
        );
        [[nodiscard]] ExprUP lambda(bool skipParams);
        [[nodiscard]] ExprUP list();
        [[nodiscard]] ExprUP table();
        [[nodiscard]] ExprUP instance();
        [[nodiscard]] ExprUP listComprehension();
        [[nodiscard]] ExprUP tableComprehension();
        [[nodiscard]] ExprUP formatString();
        [[nodiscard]] ExprUP primary();

    public:
        // So it can be modified directly.
        bool hitError{false};

        Parser() = default;
        [[nodiscard]] StmtVec& parseToAST(FileID id, const vT& tokens);
};