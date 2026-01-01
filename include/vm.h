#pragma once
#include "astnodes.h"
#include "bytecode.h"
#include "common.h"
#include "object.h"
#include "opcodes.h"
#include "vartable.h"
#include <memory>
#include <variant>
#include <vector>
using AST::Expression::VarExpr;
class ASTCompiler;

class VM
{   
    private:
        vByte::const_iterator ip;
        static constexpr int regSize = 256;
        std::unique_ptr<Object[]> registers;
        ui8 regSlot;

        // Utilities.

        inline ui8 readByte();
        inline ui16 readShort();
        inline ui32 readLong();
        bool isTruthy(const Object& obj);
        void loadOper(const vObj& pool);
        Object arithOper(Opcode oper);
        inline Object compareOper(Opcode oper);
        inline Object bitOper(Opcode oper);
        inline Object unaryOper(Opcode oper);

        // Variables.

        void executeOp(Opcode op, const vObj& pool);
    
    public:
        VM();
        void executeCode(const ByteCode& code);
        friend class ASTCompiler;
};