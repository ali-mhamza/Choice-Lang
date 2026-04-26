#pragma once
#include "linearTable.h"

#include "astnodes.h"
#include "bytecode.h"
#include "vartable.h"
#include <memory>       // For std::unique_ptr.
#include <stack>
#include <string>
#include <string_view>
#include <vector>

class ASTCompiler
{
    #define DECL_STMT(type) \
        void compile##type(const AST::Statement::type* node)
    #define DECL_EXPR(type) \
        void compile##type(const AST::Expression::type* node)

    private:
        enum VarType : ui8 { GLOBAL, CELL, LOCAL };
        struct VarInfo
        {
            // Whether or not the variable was found.
            bool found{};
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
            bool found{};
            ui8 slot{0};
        };

        struct CellInfo
        {
            ui8 slot{};
            bool inCell{};
        };

        ByteCode code{};
        ASTCompiler* const scopeCompiler{};

        ui8 nextReg{0};
        ui8 scope{0};       // Our current block scope depth.
        ui8 scopeStart{0};  // To mark the initial register for a new scope (to pop to on exit).
        const ui8 depth{};  // Our current function scope depth.

        using varTable = linearTable<VarEntry, ui8, VarHasher>;
        using accessTable = linearTable<ui8, bool>;
        using labelTable = linearTable<std::string_view, std::vector<ui64>>;

        std::stack<std::vector<std::string>> varScopes{};
        const std::unique_ptr<varTable> varLocations{new varTable};
        const std::unique_ptr<accessTable> varAccess{new accessTable};
        const std::unique_ptr<labelTable> breakLabels{new labelTable};
        const std::unique_ptr<labelTable> continueLabels{new labelTable};

        std::vector<CellInfo> captures{};
        linearTable<std::string, ui8> captureNames{};

        std::vector<ui64>* endJumps{};
        std::vector<ui64>* breakJumps{};
        std::vector<ui64>* continueJumps{};

        /* Variables. */

        // Emit an appropriate get or set instruction.
        void addVariableOp(
            bool type,
            const VarInfo& info,
            ui8 dest,
            ui8 src
        );

        // Define a variable with a register location and mutability
        // state.
        void defVar(const std::string& name, ui8 reg, bool access);

        // Undefine a variable originally declared in the current scope.
        // Used as a primitive "rollback" if we hit an error during a
        // declaration.
        // Must always be called (if it is called at all) *after* defVar.
        void removeVar(const std::string& name, ui8 reg);

        // Check if variable at register `reg` is mutable.
        bool getAccess(ui8 reg) const;

        // Check if variable is already defined in local scope
        // (for declaration compiling helpers).
        LocalInfo getScopeLocal(const Token& token) const;

        // Properly resolve a variable, recursively capturing
        // cells from enclosing scopes if needed.
        VarInfo resolveVariable(const Token& token);

        // Capture a variable from the enclosing scope.
        // Returns the cell index for the new capture, if a capture is made.
        // Otherwise returns the variable's already-used cell index, or its
        // register slot if it should not be captured.
        ui8 captureVariable(const Token& token, const VarInfo& info);

        /* Variable scoping. */

        void pushScope();
        void popScope();

        /* Registers. */

        inline void freeReg()      { nextReg--; }
        inline void reserveReg()   { nextReg++; }

        /* General helpers. */

        // `patchBreaks`: True if we are to patch 'break' jumps.
        //                False otherwise.
        void patchLoopLabelJumps(const Token& label, bool patchBreaks);
        std::string parseStringToken(const Token& token);
        void reportError(const Token& token, std::string_view message);

        /* Declarations. */

        DECL_STMT(VarDecl);
        void funcBodyHelper(
            const vT& params,
            const StmtUP& body,
            const ui8 funcReg,
            const std::string& name
        );
        DECL_STMT(FuncDecl);
        DECL_STMT(ClassDecl);

        /* Statements. */

        DECL_STMT(IfStmt);
        DECL_STMT(WhileStmt);
        void forLoopHelper(
            const AST::Statement::ForStmt* node,
            const ui8 varReg,
            const ui8 iterReg
        );
        DECL_STMT(ForStmt);
        void matchCaseHelper(
            const AST::Statement::MatchStmt::MatchCase& checkCase,
            const ui8 matchReg,
            ui64& fallJump,
            ui64& emptyJump
        );
        DECL_STMT(MatchStmt);
        DECL_STMT(RepeatStmt);
        DECL_STMT(ReturnStmt);
        DECL_STMT(BreakStmt);
        DECL_STMT(ContinueStmt);
        DECL_STMT(EndStmt);
        DECL_STMT(ExprStmt);
        DECL_STMT(BlockStmt);

        /* Expressions. */

        DECL_EXPR(TupleExpr);
        void compoundAssign(
            const AST::Expression::AssignExpr* node,
            const VarInfo& info
        );
        DECL_EXPR(AssignExpr);
        DECL_EXPR(LogicExpr);
        DECL_EXPR(CompareExpr);
        DECL_EXPR(BitExpr);
        DECL_EXPR(ShiftExpr);
        DECL_EXPR(BinaryExpr);
        void _crementExpr(
            const AST::Expression::UnaryExpr* node
        );
        DECL_EXPR(UnaryExpr);
        DECL_EXPR(CallExpr);
        DECL_EXPR(IfExpr);
        DECL_EXPR(LambdaExpr);
        DECL_EXPR(ComprehensionExpr);
        DECL_EXPR(ListExpr);
        DECL_EXPR(ReferenceExpr);
        DECL_EXPR(VarExpr);
        DECL_EXPR(LiteralExpr);

        /* Primary compilation functions. */

        void compileExpr(const ExprUP& node);
        void compileStmt(const StmtUP& node);

    public:
        // So they can be modified directly.
        bool hitError{false};
        int errorCount{0};

        ASTCompiler(ASTCompiler* comp = nullptr);
        ~ASTCompiler();

        Function* compile(const StmtVec& program);
};