#include "bytecode.h"
#include "common.h"
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
        void loadOper();
        void jumpOper(ui8 byte, int sign);
        void callOper(ui8 byte);
        void iterOper(ui8 byte);
        void collectionOper(ui8 byte);
        void captureOper(ui8 byte);
    
    public:
        Disassembler(const ByteCode& code);
        void disassembleOp(ui8 byte);
        void disassembleCode();

        friend class VM;
};