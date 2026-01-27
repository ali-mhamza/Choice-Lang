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

#define WATCH_EXEC  0
#define WATCH_REG   0

class Disassembler;

class VM
{   
    private:
        const ui8* ip;
        const ui8* end;
        static constexpr int regSize = 256;
        std::unique_ptr<Object[]> registers;

        #if WATCH_REG
        ui8 regSlot;
        #endif

        #if WATCH_EXEC
        Disassembler* dis;
        #endif

        // Utilities.

        inline ui8 readByte();
        inline ui16 readShort();
        inline ui32 readLong();
        inline bool isTruthy(const Object& obj);
        inline Object loadOper(const vObj& pool);
        inline Object concatStrings(const Object& str1,
            const Object& str2);
        Object arithOper(Opcode oper);
        inline Object compareOper(Opcode oper);
        Object bitOper(Opcode oper);
        Object unaryOper(Opcode oper);
        void handleIter(Opcode oper);

        #if WATCH_REG
        void printRegister();
        #endif

        void executeOp(Opcode op, const vObj& pool);
    
    public:
        VM();
        void executeCode(const ByteCode& code);
};