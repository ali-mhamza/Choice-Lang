#pragma once
#include "astnodes.h"
#include "bytecode.h"
#include "main_utils.h"
#include "vartable.h"
#include "vm.h"
#include <memory>
#include <stack>
#include <vector>
using namespace AST::Statement;
using namespace AST::Expression;

#ifndef ALLOW_REDEFS
    #define ALLOW_REDEFS 0
#endif

// Using PIMPL idiom.
class ASTCompVarsWrapper;
class ASTCompLoopLabels;

class ASTCompiler
{
    #define DECL(type)  void compile##type(UP(type) node)
    #define DEF(type)   void ASTCompiler::compile##type(UP(type) node)
    #define COMPILE(type)                                   \
        do {                                                \
            type* ptr = static_cast<type*>(node.release()); \
            compile##type(UP(type)(ptr));                   \
        } while (false)

    #undef REPORT_ERROR
    #define REPORT_ERROR(...)                                           \
        do {                                                            \
            hitError = true;                                            \
            if (errorCount > COMPILE_ERROR_MAX) return;                 \
            else if (errorCount == COMPILE_ERROR_MAX)                   \
            {                                                           \
                FORMAT_PRINT("COMPILATION ERROR MAXIMUM REACHED.\n");   \
                errorCount++;                                           \
                return;                                                 \
            }                                                           \
            CompileError(__VA_ARGS__).report();                         \
            errorCount++;                                               \
            return;                                                     \
        } while (false)

    #define GET_STR(tok)                                \
        normalizeNewlines(                              \
            (tok).text.substr(1, (tok).text.size() - 2) \
        )

    private:
        ByteCode code;
        ui8 previousReg;
        ui8 scope; // Our current lexical scope depth.
        std::stack<std::vector<std::string>> varScopes;
        ASTCompVarsWrapper* varsWrapper;
        ASTCompLoopLabels* labelsWrapper;
        std::vector<ui64> *endJumps, *breakJumps, *continueJumps;
        bool hitError;

        // Variables.

        inline void defVar(std::string name, ui8 reg, bool access);
        inline ui8* getVarSlot(const Token& token);
        inline ui8* getVarSlot(ExprUP& node);
        inline bool getAccess(ui8 reg);
        inline void popScope();

        // Registers.
        // Defined here for increased likelihood of inlining.

        inline void freeReg() { previousReg--; }
        inline void reserveReg() { previousReg++; }

        // Declarations.

        DECL(VarDecl);
        DECL(FuncDecl);
        DECL(ClassDecl);

        // Statements.

        DECL(IfStmt);
        DECL(WhileStmt);
        void forLoopHelper(UP(ForStmt)& node, ui8 varReg, ui8 iterReg);
        DECL(ForStmt);
        void matchCaseHelper(MatchStmt::matchCase& checkCase, const ui8 matchReg,
            ui64& fallJump, ui64& emptyJump);
        DECL(MatchStmt);
        DECL(RepeatStmt);
        DECL(ReturnStmt);
        DECL(BreakStmt);
        DECL(ContinueStmt);
        DECL(EndStmt);
        DECL(ExprStmt);
        DECL(BlockStmt);

        // Expressions.

        void compoundAssign(UP(AssignExpr)& node, ui8 slot); // Helper.
        DECL(AssignExpr);
        DECL(LogicExpr);
        DECL(CompareExpr);
        DECL(BitExpr);
        DECL(ShiftExpr);
        DECL(BinaryExpr);
        void _crementExpr(UP(UnaryExpr)& node);
        DECL(UnaryExpr);
        DECL(CallExpr);
        DECL(IfExpr);
        DECL(VarExpr);
        DECL(LiteralExpr);

        void compileExpr(ExprUP& node);
        void compileStmt(StmtUP& node);

    public:
        int errorCount; // So it can be modified directly.

        ASTCompiler();
        ~ASTCompiler();

        ByteCode& compile(StmtVec& program);
};