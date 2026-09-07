#pragma once
#include "astnodes.h"
#include "utils.h"
#include <string_view>
#include <vector>

class ASTPrinter
{
    #define DECL_STMT(type) void print##type(const AST::Statement::type* node)
    #define DECL_EXPR(type) void print##type(const AST::Expression::type* node)

    private:
        std::vector<bool> lastChildren{};

        void enterNode(bool last)   { lastChildren.push_back(last); }
        void exitNode()             { lastChildren.pop_back(); }

        void beginChild(
            std::string_view name,
            bool last,
            bool newline
        );
        void endChild();

        template<typename T, typename Fn>
        void forEachChild(const std::vector<T>& array, Fn&& fn)
        {
            for (u64 i{0}; i < array.size(); i++)
                fn(array[i], (i == array.size() - 1));
        }

        void printDepth();
        void printName(std::string_view name, bool newline = true);

        // `last` - The last variable.
        void printVar(const AST::Var& print, bool last);
        // `last` - The last parameter.
        void printParam(const AST::Param& print, bool last);
        // Never the last printed entry in any AST node.
        void printLoopHeader(const AST::LoopHeader& header);

        // Does the same thing as printBlockStmt, but without the
        // extra 'Block' header and level of indentation.
        // Also works for non-block statement bodies, like in loops.
        void printBody(
            const StmtUP& body,
            bool last,
            std::string_view name = "Body"
        );

        // Never the last printed entry in any AST node.
        void printDecl(const AST::Decl& decl, bool last);
        void printUnpackState(const AST::UnpackState& state, bool last);

        void printSimpleType(const AST::Types::TypeHint& hint);
        void printSignatureType(const AST::Types::TypeHint& hint);
        void printOptionType(const AST::Types::TypeHint& hint);
        void printGroupType(const AST::Types::TypeHint& hint);
        void printGenericType(const AST::Types::TypeHint& hint);
        void printRefType(const AST::Types::TypeHint& hint);
        void printNullableType(const AST::Types::TypeHint& hint);
        void printTypeHint(const AST::Types::TypeUP& type);
        void printTypeHint(const AST::Types::TypeHint& hint);

        /* Statement printers. */

        DECL_STMT(VarDecl);
        DECL_STMT(FuncDecl);
        DECL_STMT(TypeDecl);
        DECL_STMT(AliasDecl);
        DECL_STMT(UseStmt);
        DECL_STMT(IfStmt);
        DECL_STMT(WhileStmt);
        DECL_STMT(ForStmt);
        DECL_STMT(MatchStmt);
        DECL_STMT(RepeatStmt);
        DECL_STMT(ReturnStmt);
        DECL_STMT(BreakStmt);
        DECL_STMT(ContinueStmt);
        DECL_STMT(EndStmt);
        DECL_STMT(ExprStmt);
        DECL_STMT(BlockStmt);

        /* Expression printers. */

        DECL_EXPR(MutExpr);
        DECL_EXPR(AssignExpr);
        DECL_EXPR(LogicExpr);
        DECL_EXPR(CompareExpr);
        DECL_EXPR(BitExpr);
        DECL_EXPR(ShiftExpr);
        DECL_EXPR(BinaryExpr);
        DECL_EXPR(UnaryExpr);
        DECL_EXPR(IndexExpr);
        DECL_EXPR(CallExpr);
        DECL_EXPR(FieldExpr);
        DECL_EXPR(ScopeExpr);
        DECL_EXPR(IfExpr);
        DECL_EXPR(LambdaExpr);
        DECL_EXPR(ListExpr);
        DECL_EXPR(TableExpr);
        DECL_EXPR(InstanceExpr);
        DECL_EXPR(ListCompExpr);
        DECL_EXPR(TableCompExpr);
        DECL_EXPR(RefExpr);
        DECL_EXPR(VarExpr);
        DECL_EXPR(StringPartExpr);
        DECL_EXPR(FormatExpr);
        DECL_EXPR(LiteralExpr);

        /* General driver functions. */

        void printExpr(const ExprUP& node, bool last);
        void printStmt(const StmtUP& node, bool last);

    public:
        ASTPrinter() = default;

        void printAST(const StmtVec& program);

    #undef DECL_STMT
    #undef DECL_EXPR
};