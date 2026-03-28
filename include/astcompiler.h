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
    #define DECL(type)  void compile##type(std::unique_ptr<type> node)
    #define DEF(type)   void ASTCompiler::compile##type(std::unique_ptr<type> node)
    #define COMPILE(type)                                   \
        do {                                                \
            type* ptr = static_cast<type*>(node.release()); \
            compile##type(std::unique_ptr<type>(ptr));      \
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
        enum VarType : ui8
        {
            GLOBAL,
            CELL,
            LOCAL
        };

        struct VarInfo
        {
            // Whether or not the variable was found.
            bool found;
            // The slot/cell index of the variable.
            ui8 slot{0};

            // Variable location type.
            VarType type{};

            // Whether or not variable is mutable.
            bool access{false};
            // Whether or not variable is contained in a cell in the enclosing function.
            // False by default for non-captured (global/local) variables.
            bool inCell{false};
        };

        struct LocalInfo
        {
            bool found;
            ui8 slot{0};
        };

        struct CellInfo
        {
            ui8 slot;
            bool inCell;
        };

        ByteCode code;
        ASTCompiler* scopeCompiler;

        ui8 previousReg{0};
        ui8 scope{0}; // Our current block scope depth.
        ui8 scopeStart; // To mark the initial register for a new scope (to pop to on exit).
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
        // Check if variable is already defined in local scope
        // (for declaration compiling helpers).
        inline LocalInfo getScopeLocal(const Token& token);
        // Properly resolve a variable, recursively capturing
        // cells from enclosing scopes if needed.
        inline VarInfo resolveVariable(const Token& token);
        inline ui8 captureVariable(const Token& token, const VarInfo& info);

        inline void pushScope();
        inline void popScope();

        // Registers.
        // Defined here for increased likelihood of inlining.

        inline void freeReg() { previousReg--; }
        inline void reserveReg() { previousReg++; }

        // General helpers.

        // patchBreaks: True if we are to patch 'break' jumps.
        // False otherwise.
        void patchLoopLabelJumps(const Token& label, bool patchBreaks);

        // Declarations.

        DECL(VarDecl);
        void funcBodyHelper(const vT& params, StmtUP& body, ui8 funcReg,
            const std::string& name);
        DECL(FuncDecl);
        DECL(ClassDecl);

        // Statements.

        DECL(IfStmt);
        DECL(WhileStmt);
        void forLoopHelper(std::unique_ptr<ForStmt>& node, ui8 varReg,
            ui8 iterReg);
        DECL(ForStmt);
        void matchCaseHelper(MatchStmt::MatchCase& checkCase, const ui8 matchReg,
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
        void compoundAssign(std::unique_ptr<AssignExpr>& node,
            const VarInfo& info);
        DECL(AssignExpr);
        DECL(LogicExpr);
        DECL(CompareExpr);
        DECL(BitExpr);
        DECL(ShiftExpr);
        DECL(BinaryExpr);
        void _crementExpr(std::unique_ptr<UnaryExpr>& node); // Helper.
        DECL(UnaryExpr);
        DECL(CallExpr);
        DECL(IfExpr);
        DECL(LambdaExpr);
        DECL(ComprehensionExpr);
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