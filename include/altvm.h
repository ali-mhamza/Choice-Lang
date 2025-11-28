#pragma once
#include "bytecode.h"
#include "common.h"
#include "object.h"
#include <variant>
#include <vector>

using RegVar = std::variant<BaseUP, int, bool, Null>;

class AltVM
{
    private:
        vByte::const_iterator ip;
        static constexpr int regSize = 256;
        RegVar registers[regSize];
        uint8_t regSlot;

        // Utilities.

        bool checkNumOper(uint8_t slot);
        bool checkNumOpers(uint8_t slot1, uint8_t slot2);
        void arithOper(Opcode oper);

        void executeOp(uint8_t op);
    
    public:
        AltVM();
        void executeCode(const ByteCode& code);
};