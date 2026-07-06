#pragma once
#include "bytecode.h"
#include "common.h"
#include "config.h"
#include "object.h"
#include "opcodes.h"
#include <string>
#include <unordered_set>
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
        const ByteCode* code{};
        Closure* closure{};
        Object* regStart{};
        const u8* ip{};

        #if WATCH_EXEC
        Disassembler* dis;
        #endif
    };

    const ByteCode* code{};
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
        const ByteCode* currentCode{};
        Closure* currentClosure{};
        const u8* ip{};

        Object* globalRegisters{new Object[NUM_REGS]};
        Object* registers{globalRegisters};
        const Object* pool{};

        std::vector<Object*> scopeStarts{};
        std::vector<CallFrame> frames{};
        std::vector<Cell*> activeCells{};

        // Set to 'true' if each function call in 'callFunc'
        // should run until termination.
        // If set to 'false', 'callFunc' initializes function object's
        // chunk and exits immediately.
        bool encapsulateCall{false};

        // Argument count for most recent call.
        u8 args{};

        // To communicate with compiler on variables to "undeclare"
        // upon initializer error.
        bool inDeclaration{false};
        u8 clearIndex{};

        #if WATCH_REG
        u8 regSlot{};
        #endif

        #if WATCH_EXEC
        Disassembler* dis{};
        #endif

        // Utilities.

        // Initialize global built-in constants or functions.
        void defineBuiltinGlobals();

        // Fix any built-in constants or functions which are
        // only determined upon receiving input.

        void amendFileName();

        [[nodiscard]] u8 readByte();
        [[nodiscard]] u16 readShort();
        [[nodiscard]] u32 readLong();

        [[nodiscard]] Cell* captureValue(u8 slot, bool local);
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
        void checkFuncArgs(const Function* func, u8 argCount);
        void prepFuncArgs(const Function* func, u8 argCount);
        void restoreData();

        void callFunc(const Object& callee, u8 start, u8 argCount);
        void callNative(const Object& callee, u8 start, u8 argCount);
        void callCtor(const Object& callee, u8 start, u8 argCount);
        void callClass(const Object& callee, u8 start, u8 argCount);
        void callMethod(const Object& callee, u8 start, u8 argCount);
        void callObj(const Object& callee, u8 start, u8 argCount);

        void getModule(Object& module, const Object& dir);

        void startIter();
        void updateIter();

        // Initializes any remaining uninitialized fields in an instance
        // object.
        // `start`: First available register after instance object.
        void finishFields(Instance& instance, u8 start);

        void unpackObject(u8 reg, u8 count);

        #if WATCH_REG
        void printRegister();
        #endif

        // To report errors without any call-stack display.
        // Currently only when maximum call depth is reached.
        void reportShortError(const RuntimeError& error);
        void reportError(const RuntimeError& error);
        void reportWarning(DiagCode code, const std::string& label = "");
        // Reset variable state upon error.
        void errorReset();

        void executeOp(Opcode op);
        void executeChunk(const ByteCode& chunk, Function* func = nullptr);
        void executeCode();

    public:
        VM();
        ~VM();

        void execute(Function* script);
        // Only to be used for modules.
        [[nodiscard]]
        const Object* getRegisters() const { return globalRegisters; }
};