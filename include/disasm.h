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

        ui8 restoreByte() const;
        ui16 restoreShort() const;
        ui32 restoreLong() const;

        void singleOper(ui8 byte);
        void doubleOper(ui8 byte);
        void loadOp();
        // `sign`: positive to jump forward, negative otherwise.
        void jumpOp(ui8 byte, int sign);
        void callOp(ui8 byte);
        void iterOp(ui8 byte);
        // Currently: lists and tuples.
        void collectionOp(ui8 byte);
        // For closure captures.
        void captureOp(ui8 byte);
        // For OP_MAKE_REF.
        void referenceOp();
        // For OP_FORMAT_STR.
        void formatOp();
    
    public:
        Disassembler(const ByteCode& code);
        void disassembleOp(ui8 byte);
        void disassembleCode();

        friend class VM;
};