#pragma once
#include "bytecode.h"
#include "common.h"     // For vBit, fixed-size integer types.
#include "object.h"
#include <string_view>

class VM;

class Disassembler
{
    private:
        const ByteCode& code;
        vBit ip;
        vBit start;
        // Whether or not we are disassembling the top-level script.
        bool topLevel;
        // Whether we are disassembling code directly or during
        // VM execution.
        bool inVM;

        void printOpcode(std::string_view opName);
        void printOperValue(const Object& oper);
        void disFunction(const Function& func);

        ui8 restoreByte();
        ui16 restoreShort();
        ui32 restoreLong();

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
    
    public:
        Disassembler(const ByteCode& code);
        void disassembleOp(ui8 byte);
        void disassembleCode();

        friend class VM;
};