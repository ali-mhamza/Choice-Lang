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
                const ui8* ip{};

                #if WATCH_EXEC
                Disassembler* dis;
                #endif
            };

            Function* function{};
            Closure* closure{};
            Object* regStart{};
            const ui8* ip{};

            #if WATCH_EXEC
            Disassembler* dis;
            #endif

            CallFrame() = default;
            CallFrame(const Args& args);
        };

        Function* currentFunc{};
        Closure* currentClosure{};
        const ui8* ip{};
        const ui8* end{};

        static constexpr size_t regSize = 4096;
        Object* globalRegisters{new Object[regSize]};
        Object* registers{globalRegisters};
        const Object* pool{};

        std::vector<Object*> scopeStarts{};
        std::vector<CallFrame> frames{};
        std::vector<Cell*> activeCells{};

        #if WATCH_REG
        ui8 regSlot{};
        #endif

        #if WATCH_EXEC
        Disassembler* dis{};
        #endif

        // Utilities.

        ui8 readByte();
        ui16 readShort();
        ui32 readLong();

        bool isTruthy(const Object& obj);
        Cell* captureValue(ui8 slot);
        void closeCells(Object* limit);
        #if COPY_INLINE
            void copyObject(Object& dest, const Object& src);
        #endif

        Object loadOper();
        Object concatStrings(const Object& str1, const Object& str2);
        Object makeRange(const Object& start, const Object& stop);
        Object arithOper(Opcode op, ui8 firstOper);
        Object compareOper(Opcode op, ui8 firstOper); // No variables get modified, so no offset.
        Object bitOper(Opcode op, ui8 firstOper);
        Object unaryOper(Opcode op, ui8 oper);

        void callFunc(const Object& callee, ui8 start, ui8 argCount);
        void callNative(const Object& callee, ui8 start, ui8 argCount);
        void callObj(const Object& callee, ui8 start, ui8 argCount);
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