#include "bytecode.h"
#include "common.h"
#include "object.h"

class Disassembler
{
    private:
        const ByteCode& code;
        vByte::const_iterator ip;
        vByte::const_iterator start;

        void printOperValue(const Object& oper);

        ui8 restoreByte();
        ui16 restoreShort();
        ui32 restoreLong();

        void singleOper(std::string_view opName);
        void doubleOper(std::string_view opName);
        void tripleOper(std::string_view opName);
        void loadOper(std::string_view opName);

        void disassembleOp(ui8 byte);
    
    public:
        Disassembler(const ByteCode& code);
        void disassembleCode();
};