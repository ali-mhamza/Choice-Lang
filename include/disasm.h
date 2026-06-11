#pragma once
#include "bytecode.h"
#include "common.h"
#include "object.h"
#include <string_view>

class VM;

class Disassembler
{
    private:
        const Function* func{};
        vBit ip{};
        const vBit start{};
        // Whether or not we are disassembling the top-level script.
        bool topLevel{true};
        // Whether we are disassembling code directly or during
        // VM execution.
        bool inVM{true};

        void printOpcode(std::string_view opName);
        // No padding or space after the opcode name.
        void printBareOpcode(std::string_view opName);
        void printOperValue(const Object& oper) const;
        void disFunction(const Function* func) const;

        [[nodiscard]] u8 readByte();
        [[nodiscard]] u16 readShort();
        [[nodiscard]] u32 readLong();

        void singleOper(u8 byte);
        void doubleOper(u8 byte);
        void loadOp();
        // `sign`: positive to jump forward, negative otherwise.
        void jumpOp(u8 byte, int sign);
        void callOp(u8 byte);
        void iterOp(u8 byte);
        void indexOp(u8 byte);
        // Currently: lists and tables.
        void collectionOp(u8 byte);
        // For closure captures.
        void captureOp(u8 byte);
        // For OP_MAKE_REF.
        void referenceOp();
        // For OP_FORMAT_STR.
        void formatOp();
        // For OP_DEF_START.
        void declOp();

    public:
        Disassembler(const Function* function);
        void disassembleOp(u8 byte);
        void disassembleCode();

        friend class VM;
};