#pragma once
#include "astnodes.h"
#include "bytecode.h"
#include "vartable.h"
#include "vm.h"
#include <memory>
using namespace AST::Statement;
using namespace AST::Expression;

// Using PIMPL idiom.
class ASTCompVarsWrapper;

class ASTCompiler
{
    #define DECL(type)  void compile##type(UP(type) node)
    #define DEF(type)   void ASTCompiler::compile##type(UP(type) node)
    #define COMPILE(type, node) \
        do { \
            type* ptr = static_cast<type*>(node.release()); \
            compile##type(UP(type)(ptr)); \
        } while (false)

    #define GET_STR(tok) \
        std::string((tok).text.substr(1, (tok).text.size() - 2))

    private:
        ByteCode code;
        ui8 previousReg;
        ui8 currentReg;
        // The last register that contains a variable (initially 0).
        ui8 lastVarReg;
        ui8 scope; // Our current lexical scope depth.
        ASTCompVarsWrapper* varsWrapper;
        std::vector<std::vector<std::string>> varScopes;

        // Variables.

        void defVar(std::string name, ui8 reg, ui8 scope);
        ui8* getVarSlot(const Token& token, ui8 scope);
        ui8* getVarSlot(ExprUP& node, ui8 scope);
        void popScope(ui8 scope);

        // Registers.

        void freeReg();
        void reserveReg();

        // Declarations.

        DECL(VarDecl);
        DECL(FunDecl);
        DECL(ClassDecl);

        // Statements.

        DECL(ReturnStmt);
        DECL(LoopStmt);
        DECL(ExprStmt);
        DECL(BlockStmt);

        // Expressions.

        DECL(AssignExpr);
        DECL(LogicExpr);
        DECL(CompareExpr);
        DECL(BitExpr);
        DECL(ShiftExpr);
        DECL(BinaryExpr);
        DECL(UnaryExpr);
        DECL(CallExpr);
        DECL(VarExpr);
        DECL(LiteralExpr);

        void compileExpr(ExprUP& node);
        void compileStmt(StmtUP& node);

    public:
        ASTCompiler();
        ~ASTCompiler();

        ByteCode& compile(StmtVec& program);
};