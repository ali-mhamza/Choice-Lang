#pragma once
#include "bytecode.h"
#include "common.h"
#include "object.h"
#include <string_view>

class Disassembler
{
    private:
        const ByteCode& code;
        vByte::const_iterator ip;
        vByte::const_iterator start;

        void printOperValue(const BaseUP& oper);

        void noOper(std::string_view opName);
        void byteOper(std::string_view opName);
        void shortOper(std::string_view opName);
        void longOper(std::string_view opName);
        void constOper(std::string_view opName);

        void disassembleOp(uint8_t byte);
    
    public:
        Disassembler(const ByteCode& code);
        void disassembleCode();
};