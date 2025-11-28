#include "bytecode.h"
#include "common.h"

class AltDisassembler
{
    private:
        const ByteCode& code;
        vByte::const_iterator ip;
        vByte::const_iterator start;

        void printOperValue(const BaseUP& oper);

        uint8_t restoreByte();
        uint16_t restoreShort();
        uint32_t restoreLong();

        void singleOper(std::string_view opName);
        void doubleOper(std::string_view opName);
        void tripleOper(std::string_view opName);
        void loadOper(std::string_view opName);

        void disassembleOp(uint8_t byte);
    
    public:
        AltDisassembler(const ByteCode& code);
        void disassembleCode();
};