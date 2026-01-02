#include "bytecode.h"
#include "common.h"
#include "object.h"
#include <optional>

class Disassembler
{
    private:
        using vBit = vByte::const_iterator;

        const ByteCode& code;
        vBit ip;
        vBit start;

        void printOperValue(const Object& oper);

        ui8 restoreByte();
        ui16 restoreShort();
        ui32 restoreLong();

        void singleOper(std::string_view opName);
        void doubleOper(std::string_view opName);
        void tripleOper(std::string_view opName);
        void loadOper(std::string_view opName);
    
    public:
        Disassembler(const ByteCode& code);
        void disassembleOp(ui8 byte);
        void disassembleCode();
};