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
                Object* regStart;
                const ui8* ip;
                const ui8* end;
                const Object* pool;
                ui8 offset;

                #if WATCH_EXEC
                Disassembler* dis;
                #endif
            };

            Object* regStart;
            const ui8* ip;
            const ui8* end;
            const Object* pool;
            ui8 offset;

            #if WATCH_EXEC
            Disassembler* dis;
            #endif

            CallFrame() = default;
            CallFrame(const Args& args);
        };

        struct ScopeUndo
        {
            ui8 offset;
            Object* window;
        };

        const ui8* ip;
        const ui8* end;
        static constexpr size_t regSize = 256;
        Object* registers;
        std::vector<CallFrame> frames;
        Object* depthWindows[MAX_SCOPE_DEPTH];
        std::vector<ScopeUndo> scopeUndo;
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
        #if COPY_INLINE
            inline void copyObject(Object& dest, const Object& src);
        #endif

        inline void pushScope(ui8 depth, Object* window);
        inline void popScope();
        // keepGlobal: Do not clear the global scope
        // (when an error is hit, unwind to the global scope).
        inline void clearScopes(bool keepGlobal);

        inline Object loadOper();
        inline Object concatStrings(const Object& str1,
            const Object& str2);
        Object arithOper(Opcode op, ui8 offset, ui8 firstOper);
        Object compareOper(Opcode op, ui8 firstOper); // No variables get modified, so no offset.
        Object bitOper(Opcode op, ui8 offset, ui8 firstOper);
        Object unaryOper(Opcode op, ui8 offset, ui8 oper);
        void callFunc(const Object& callee, ui8 offset, ui8 start, ui8 argCount);
        void callNative(const Object& callee, ui8 start, ui8 argCount);
        void callObj(const Object& callee, ui8 offset, ui8 start, ui8 argCount);
        inline void restoreData();
        void handleIter(Opcode op);

        #if WATCH_REG
        void printRegister();
        #endif

        void executeOp(Opcode op);
    
    public:
        VM();
        ~VM();

        void executeCode(const ByteCode& code);
};