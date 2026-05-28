#pragma once
#include "astnodes.h"
#include "bytecode.h"
#include "debug.h"
#include "diagnostic.h"
#include "vartable.h"
#include <personal/linearTable.h>
#include <memory>
#include <stack>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class Compiler
{
    #define DECL_STMT(type) \
        void compile##type(const AST::Statement::type* node)
    #define DECL_EXPR(type) \
        void compile##type(const AST::Expression::type* node)

    private:
        enum VarType : u8 { GLOBAL, CELL, LOCAL };
        struct VarInfo
        {
            // Whether or not the variable was found.
            bool found{};
            // The slot/cell index of the variable.
            u8 slot{0};

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
            u8 slot{0};
        };

        struct CellInfo
        {
            u8 slot{};
            bool inCell{};
        };

        struct DeclarationPair
        {
            std::string name{};
            u8 reg{};
        };

        ByteCode code{};
        Compiler* const scopeCompiler{};
        FileID id{};
        DebugMetadata metadata{};

        u8 nextReg{0};
        u8 scope{0};       // Our current block scope depth.
        u8 scopeStart{0};  // To mark the initial register for a new scope (to pop to on exit).
        const u8 depth{};  // Our current function scope depth.

        using varTable = linearTable<VarEntry, u8, VarHasher>;
        using accessTable = linearTable<u8, bool>;
        using labelTable = linearTable<std::string_view, std::vector<u64>>;

        std::stack<std::vector<std::string>> varScopes{};
        const std::unique_ptr<varTable> varLocations{new varTable};
        const std::unique_ptr<accessTable> varAccess{new accessTable};
        const std::unique_ptr<labelTable> breakLabels{new labelTable};
        const std::unique_ptr<labelTable> continueLabels{new labelTable};

        std::vector<CellInfo> captures{};
        linearTable<std::string, u8> captureNames{};

        std::vector<u64>* endJumps{};
        std::vector<u64>* breakJumps{};
        std::vector<u64>* continueJumps{};

        /* Variables. */

        // Emit an appropriate get or set instruction.
        void emitVariableOp(bool type, const VarInfo& info, u8 dest, u8 src);

        // Define a variable with a register location and mutability
        // state.
        void defVar(const std::string& name, u8 reg, bool access);

        // Undefine a variable originally declared in the current scope.
        // Used as a primitive "rollback" if we hit an error during a
        // declaration.
        // Must always be called (if it is called at all) *after* defVar.
        void removeVar(const std::string& name, u8 reg);

        // To clear declaredVars upon a compile-time or runtime error.
        void clearDeclarations();

        // Check if variable at register `reg` is mutable.
        [[nodiscard]] bool getAccess(u8 reg) const;

        // Check if variable is already defined in local scope
        // (for declaration compiling helpers).
        [[nodiscard]] LocalInfo getScopeLocal(const Token& token) const;

        // Properly resolve a variable, recursively capturing
        // cells from enclosing scopes if needed.
        [[nodiscard]] VarInfo resolveVariable(const Token& token);

        // Capture a variable from the enclosing scope.
        // Returns the cell index for the new capture, if a capture is made.
        // Otherwise returns the variable's already-used cell index, or its
        // register slot if it should not be captured.
        [[nodiscard]]
        u8 captureVariable(const Token& token, const VarInfo& info);

        /* Variable scoping. */

        void pushScope();
        void popScope();

        /* Variable declarations. */

        void startDeclaration();
        void endDeclaration();

        /* Registers. */

        inline void freeReg()      { nextReg--; }
        inline void reserveReg()   { nextReg++; }

        /* General helpers. */

        // `patchBreaks` - True if we are to patch 'break' jumps.
        //                 False otherwise.
        void patchLoopLabelJumps(const Token& label, bool patchBreaks);

        // `offset` - Number of characters to subtract from the size.
        [[nodiscard]] std::string parseStringToken(
            const Token& token,
            size_t start,
            size_t offset
        );

        void reportError(
            DiagCode code,
            const Token& token,
            std::string_view message = ""
        );

        // General helper to report a specific part in the source code.
        void reportPart(
            bool isError,
            DiagCode code,
            u64 offset,
            u64 length,
            std::string_view message = ""
        );

        // For specific cases were we want to report a specific
        // part of a token for an error.
        void reportPartError(
            DiagCode code,
            const Token& token,
            u64 offset, // With respect to start of token.
            u64 length,
            std::string_view message = ""
        );

        /* Declarations. */

        DECL_STMT(VarDecl);
        void funcBodyHelper(
            const vT& params,
            const StmtUP& body,
            const u8 funcReg,
            const std::string& name
        );
        DECL_STMT(FuncDecl);
        DECL_STMT(ClassDecl);

        /* Statements. */

        DECL_STMT(IfStmt);
        DECL_STMT(WhileStmt);
        void forLoopHelper(
            const AST::Statement::ForStmt* node,
            const u8 varReg,
            const u8 iterReg
        );
        DECL_STMT(ForStmt);
        void matchCaseHelper(
            const AST::Statement::MatchStmt::MatchCase& checkCase,
            const u8 matchReg,
            u64& fallJump,
            u64& emptyJump
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

        // Returns the mutability and variable information for
        // a variable.
        // Returns true if `expr` is a variable expression with
        // an existing, mutable variable, or if it isn't a variable
        // expression at all, and reports errors accordingly.
        template<typename NodeT>
        [[nodiscard]] std::pair<bool, VarInfo> checkMutability(
            const NodeT* node,
            const ExprUP& expr
        );

        Opcode getCompoundAssignOpcode(
            const AST::Expression::AssignExpr* node
        );

        void assignToVar(
            const AST::Expression::AssignExpr* node
        );
        void compoundAssignToVar(
            const AST::Expression::AssignExpr* node,
            const VarInfo& info
        );

        void assignToElement(
            const AST::Expression::AssignExpr* node
        );
        void compoundAssignToElement(
            const AST::Expression::AssignExpr* node,
            u8 objReg,
            u8 indexReg
        );

        DECL_EXPR(MutExpr);
        DECL_EXPR(AssignExpr);
        DECL_EXPR(LogicExpr);
        DECL_EXPR(CompareExpr);
        DECL_EXPR(BitExpr);
        DECL_EXPR(ShiftExpr);
        DECL_EXPR(BinaryExpr);
        void _crementVar(
            const AST::Expression::UnaryExpr* node
        );
        void _crementElement(
            const AST::Expression::UnaryExpr* node
        );
        DECL_EXPR(UnaryExpr);
        DECL_EXPR(IndexExpr);
        DECL_EXPR(CallExpr);
        DECL_EXPR(IfExpr);
        DECL_EXPR(LambdaExpr);
        DECL_EXPR(ComprehensionExpr);
        DECL_EXPR(ListExpr);
        DECL_EXPR(TableExpr);
        DECL_EXPR(ReferenceExpr);
        DECL_EXPR(VarExpr);
        DECL_EXPR(StringPartExpr);
        DECL_EXPR(FormatExpr);
        DECL_EXPR(LiteralExpr);

        /* Primary compilation functions. */

        void compileExpr(const ExprUP& node);
        void compileStmt(const StmtUP& node);

        // Finalize and return ByteCode object.
        ByteCode& getCode();

    public:
        // So they can be modified directly.
        bool hitError{false};

        static std::vector<DeclarationPair> declaredVars;
        static bool clearDeclaredVars;
        static u8 clearIndex;

        Compiler(Compiler* comp = nullptr);
        ~Compiler();

        [[nodiscard]] Function* compile(FileID id, const StmtVec& program);
};