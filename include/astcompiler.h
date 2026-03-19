#pragma once
#include "astnodes.h"
#include "bytecode.h"
#include "object.h"
#include "utils.h"
#include "vartable.h"
#include "vm.h"
#include <memory>
#include <stack>
#include <unordered_map>
#include <vector>
using namespace AST::Statement;
using namespace AST::Expression;

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
                CH_PRINT("COMPILATION ERROR MAXIMUM REACHED.\n");   \
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
        struct VarInfo
        {
            ui8* slot;
            ui8 scope; // Block scope depth.
            ui8 depth; // Function scope depth.
            bool access;
        };

        struct CellInfo
        {
            ui8 depth;
            ui8 slot;
            bool captured;
        };

        ByteCode code;
        ASTCompiler* scopeCompiler;

        ui8 previousReg;
        ui8 scope{0}; // Our current block scope depth.
        ui8 depth; // Our current function scope depth.

        std::stack<std::vector<std::string>> varScopes;
        ASTCompVarsWrapper* varsWrapper;
        ASTCompLoopLabels* labelsWrapper;

        std::vector<CellInfo> captures;
        std::unordered_map<std::string, ui8> captureNames;

        std::vector<ui64>* endJumps{nullptr};
        std::vector<ui64>* breakJumps{nullptr};
        std::vector<ui64>* continueJumps{nullptr};

        bool hitError{false};

        // Variables.

        inline void addVariableOp(bool type, const VarInfo& info, ui8 dest,
            ui8 src);
        inline void defVar(std::string name, ui8 reg, bool access);
        inline bool getAccess(ui8 reg);
        inline VarInfo getVarInfo(const Token& token);
        inline CellInfo getCell(const std::string& name, const VarInfo& info);
        // Returns true if a new capture has been made.
        inline bool captureVariable(const Token& token, const VarInfo& info);
        inline void popScope();

        // Registers.
        // Defined here for increased likelihood of inlining.

        inline void freeReg() { previousReg--; }
        inline void reserveReg() { previousReg++; }

        // Declarations.

        DECL(VarDecl);
        void funcBodyHelper(const vT& params, StmtUP& body, ui8 funcReg,
            const std::string& name);
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

        DECL(TupleExpr);
        // Helper.
        void compoundAssign(UP(AssignExpr)& node, const VarInfo& pos,
            bool cellUsed);
        DECL(AssignExpr);
        DECL(LogicExpr);
        DECL(CompareExpr);
        DECL(BitExpr);
        DECL(ShiftExpr);
        DECL(BinaryExpr);
        void _crementExpr(UP(UnaryExpr)& node); // Helper.
        DECL(UnaryExpr);
        DECL(CallExpr);
        DECL(IfExpr);
        DECL(LambdaExpr);
        DECL(ListExpr);
        DECL(VarExpr);
        DECL(LiteralExpr);

        void compileExpr(ExprUP& node);
        void compileStmt(StmtUP& node);

    public:
        int errorCount{0}; // So it can be modified directly.

        ASTCompiler(ASTCompiler* comp = nullptr);
        ~ASTCompiler();

        Function* compile(StmtVec& program);
};