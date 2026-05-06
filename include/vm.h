#pragma once
#include "common.h"     // For fixed-size integer types, size_t.
#include "object.h"
#include "opcodes.h"
#include <vector>

#if defined(DEBUG)
    #define WATCH_EXEC  0
    #define WATCH_REG   0
#endif

#define COPY_INLINE 0

class Disassembler;

class VM
{
    private:
        struct CallFrame
        {
            struct Args
            {
                Function* function{};
                Closure* closure{};
                Object* regStart{};
                const u8* ip{};

                #if WATCH_EXEC
                Disassembler* dis;
                #endif
            };

            Function* function{};
            Closure* closure{};
            Object* regStart{};
            const u8* ip{};

            #if WATCH_EXEC
            Disassembler* dis;
            #endif

            CallFrame() = default;
            CallFrame(const Args& args);
        };

        Function* currentFunc{};
        Closure* currentClosure{};
        const u8* ip{};
        const u8* end{};

        static constexpr size_t regSize{4096};
        Object* globalRegisters{new Object[regSize]};
        Object* registers{globalRegisters};
        const Object* pool{};

        std::vector<Object*> scopeStarts{};
        std::vector<CallFrame> frames{};
        std::vector<Cell*> activeCells{};

        #if WATCH_REG
        u8 regSlot{};
        #endif

        #if WATCH_EXEC
        Disassembler* dis{};
        #endif

        // Utilities.

        u8 readByte();
        u16 readShort();
        u32 readLong();

        bool isTruthy(const Object& obj);
        Cell* captureValue(u8 slot);
        void closeCells(Object* limit);
        #if COPY_INLINE
            void copyObject(Object& dest, const Object& src);
        #endif

        Object concatStrings(const Object& str1, const Object& str2);
        Object makeRange(const Object& start, const Object& stop);
        Object makeReference();

        Object loadOper();
        Object arithOper(Opcode op, u8 firstOper);
        Object compareOper(Opcode op, u8 firstOper);
        Object bitOper(Opcode op, u8 firstOper);
        Object unaryOper(Opcode op, u8 oper);

        void callFunc(const Object& callee, u8 start, u8 argCount);
        void callNative(const Object& callee, u8 start, u8 argCount);
        void callObj(const Object& callee, u8 start, u8 argCount);
        void restoreData();

        void startIter();
        void updateIter();

        #if WATCH_REG
        void printRegister();
        #endif

        void executeOp(Opcode op);

    public:
        VM();
        ~VM();

        void executeCode(Function* script);
};