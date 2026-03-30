#pragma once
#include "common.h"     // For fixed-size integer types, size_t.
#include "object.h"
#include "opcodes.h"
#include <vector>

#if defined(DEBUG)
    #define WATCH_EXEC  0
    #define WATCH_REG   0
#endif

#define COPY_INLINE 1

class Disassembler;

class VM
{
    private:
        struct CallFrame
        {
            struct Args
            {
                Function* function;
                Closure* closure;
                Object* regStart;
                const ui8* ip;

                #if WATCH_EXEC
                Disassembler* dis;
                #endif
            };

            Function* function;
            Closure* closure;
            Object* regStart;
            const ui8* ip;

            #if WATCH_EXEC
            Disassembler* dis;
            #endif

            CallFrame() = default;
            CallFrame(const Args& args);
        };

        Function* currentFunc{nullptr};
        Closure* currentClosure{nullptr};
        const ui8* ip{nullptr};
        const ui8* end{nullptr};
        static constexpr size_t regSize = 4096;
        Object* globalRegisters;
        Object* registers;
        const Object* pool{nullptr};

        std::vector<Object*> scopeStarts;
        std::vector<CallFrame> frames;
        std::vector<Cell*> activeCells;

        #if WATCH_REG
        ui8 regSlot;
        #endif

        #if WATCH_EXEC
        Disassembler* dis{nullptr};
        #endif

        // Utilities.

        inline ui8 readByte();
        inline ui16 readShort();
        inline ui32 readLong();

        inline bool isTruthy(const Object& obj);
        inline Cell* captureValue(ui8 slot);
        inline void closeCells(Object* limit);
        #if COPY_INLINE
            inline void copyObject(Object& dest, const Object& src);
        #endif

        // keepGlobal: Do not clear the global scope
        // (when an error is hit, unwind to the global scope).
        inline void clearScopes(bool keepGlobal);

        inline Object loadOper();
        inline Object concatStrings(const Object& str1, const Object& str2);
        Object arithOper(Opcode op, ui8 firstOper);
        Object compareOper(Opcode op, ui8 firstOper); // No variables get modified, so no offset.
        Object bitOper(Opcode op, ui8 firstOper);
        Object unaryOper(Opcode op, ui8 oper);

        void callFunc(const Object& callee, ui8 start, ui8 argCount);
        void callNative(const Object& callee, ui8 start, ui8 argCount);
        void callObj(const Object& callee, ui8 start, ui8 argCount);
        inline void restoreData();

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