#pragma once
#include "astnodes.h"
#include "bytecode.h"
#include "common.h"
#include "object.h"
#include "opcodes.h"
#include "vartable.h"
#include <memory>
#include <stack>
#include <variant>

#define WATCH_EXEC  0
#define WATCH_REG   0

class Disassembler;

class VM
{
    private:
        struct FuncContext
        {
            struct Args
            {
                Object* regStart;
                const ui8* ip;
                const ui8* end;
                const Object* pool;

                #if WATCH_EXEC
                Disassembler* dis;
                #endif
            };
            
            Object* regStart;
            const ui8* ip;
            const ui8* end;
            const Object* pool;

            #if WATCH_EXEC
            Disassembler* dis;
            #endif

            FuncContext() = default;
            FuncContext(const Args& args);
        };

        const ui8* ip;
        const ui8* end;
        static constexpr int regSize = 256;
        Object* registers;
        std::stack<FuncContext> contexts;
        const Object* pool;

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

        inline Object loadOper();
        inline Object concatStrings(const Object& str1,
            const Object& str2);
        Object arithOper(Opcode oper);
        inline Object compareOper(Opcode oper);
        Object bitOper(Opcode oper);
        Object unaryOper(Opcode oper);
        void callFunc(ui8 callee, ui8 start, ui8 argCount);
        void handleIter(Opcode oper);

        #if WATCH_REG
        void printRegister();
        #endif

        void executeOp(Opcode op);
    
    public:
        VM();
        ~VM();

        void executeCode(const ByteCode& code);
};