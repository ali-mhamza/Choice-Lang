#pragma once
#include "bytecode.h"
#include "common.h"     // For vBit, fixed-size integer types.
#include "object.h"
#include <string_view>

class VM;

class Disassembler
{
    private:
        const ByteCode& code{};
        vBit ip{};
        const vBit start{};
        // Whether or not we are disassembling the top-level script.
        bool topLevel{true};
        // Whether we are disassembling code directly or during
        // VM execution.
        bool inVM{true};

        void printOpcode(std::string_view opName) const;
        void printOperValue(const Object& oper) const;
        void disFunction(const Function& func) const;

        [[nodiscard]] u8 restoreByte() const;
        [[nodiscard]] u16 restoreShort() const;
        [[nodiscard]] u32 restoreLong() const;

        void singleOper(u8 byte);
        void doubleOper(u8 byte);
        void loadOp();
        // `sign`: positive to jump forward, negative otherwise.
        void jumpOp(u8 byte, int sign);
        void callOp(u8 byte);
        void iterOp(u8 byte);
        // Currently: lists and tuples.
        void collectionOp(u8 byte);
        // For closure captures.
        void captureOp(u8 byte);
        // For OP_MAKE_REF.
        void referenceOp();
        // For OP_FORMAT_STR.
        void formatOp();

    public:
        Disassembler(const ByteCode& code);
        void disassembleOp(u8 byte);
        void disassembleCode();

        friend class VM;
};