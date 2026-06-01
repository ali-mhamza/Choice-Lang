#pragma once
#include "common.h"
#include "object.h"
#include "opcodes.h"
#include <vector>

#if defined(DEBUG)
    #define WATCH_EXEC  0
    #define WATCH_REG   0
#endif

#define COPY_INLINE 0

class Disassembler;
struct RuntimeError;
enum DiagCode : u8;

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

class VM
{
    private:
        Function* currentFunc{};
        Closure* currentClosure{};
        const u8* ip{};

        static constexpr size_t regSize{4096};
        Object* globalRegisters{new Object[regSize]};
        Object* registers{globalRegisters};
        const Object* pool{};

        std::vector<Object*> scopeStarts{};
        std::vector<CallFrame> frames{};
        std::vector<Cell*> activeCells{};

        bool inDeclaration{false};
        u8 clearIndex{}; // To communicate with compiler.

        #if WATCH_REG
        u8 regSlot{};
        #endif

        #if WATCH_EXEC
        Disassembler* dis{};
        #endif

        // Utilities.

        [[nodiscard]] u8 readByte();
        [[nodiscard]] u16 readShort();
        [[nodiscard]] u32 readLong();

        [[nodiscard]] bool isTruthy(const Object& obj);
        [[nodiscard]] Cell* captureValue(u8 slot);
        void closeCells(Object* limit);
        #if COPY_INLINE
            void copyObject(Object& dest, const Object& src);
        #endif

        [[nodiscard]] Object concatStrings(const Object& str1, const Object& str2);
        [[nodiscard]] Object makeRange(const Object& start, const Object& stop);
        [[nodiscard]] Object makeReference();
        // `willAssign`: True if the returned object will be the
        // target of an assignment (necessitating a mutability check).
        [[nodiscard]] Object& deref(Object& ref, bool willAssign = false);

        [[nodiscard]] Object loadOper();
        [[nodiscard]] Object arithOper(Opcode op, u8 firstOper);
        [[nodiscard]] Object compareOper(Opcode op, u8 firstOper);
        [[nodiscard]] Object bitOper(Opcode op, u8 firstOper);
        [[nodiscard]] Object unaryOper(Opcode op, u8 oper);

        [[nodiscard]] Object getIndex(u8 objReg, u8 indexReg);
        void setIndex(u8 objReg, u8 indexReg, u8 valueReg);

        void pushCurrentStackFrame();
        void callFunc(const Object& callee, u8 start, u8 argCount);
        void callNative(const Object& callee, u8 start, u8 argCount);
        void callObj(const Object& callee, u8 start, u8 argCount);
        void restoreData();

        void startIter();
        void updateIter();

        #if WATCH_REG
        void printRegister();
        #endif

        void reportError(const RuntimeError& error);
        void reportWarning(DiagCode code, const std::string& label = "");
        void executeOp(Opcode op);

    public:
        VM();
        ~VM();

        void executeCode(Function* script);
};