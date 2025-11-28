#pragma once
#include "common.h"
#include "object.h"
#include <fstream>
#include <vector>
using namespace Object;

class VM;
class Disassembler;
// For testing.
class AltDisassembler;

struct ByteCode
{
    private:
        vByte block;
        vObj pool;

    public:
        ByteCode();
        ByteCode(const vByte& block);
        ByteCode(const vByte& block, vObj pool);

        void addByte(uint8_t byte);
        template<typename... Bytes>
        void addBytes(Bytes... bytes);
        // Using big endian.
        void addShort(uint16_t bytes);
        void addLong(uint32_t bytes);
        void addConst(BaseUP constant);

        void cacheStream(std::ofstream& os) const;

        // For testing.
        void loadReg(uint8_t reg, uint8_t op);
        void loadRegConst(BaseUP constant, uint8_t reg);
        template<typename Op, typename... Bytes>
        void addOp(Op op, Bytes... opers);

        friend class VM;
        friend class Disassembler;
        // For testing.
        friend class AltDisassembler;
};

template<typename... Bytes>
void ByteCode::addBytes(Bytes... bytes)
{
    for (uint8_t byte : {bytes...})
        addByte(byte);
}

// For testing.

template<typename Op, typename... Bytes>
void ByteCode::addOp(Op op, Bytes... opers)
{
    addByte(static_cast<uint8_t>(op));
    for (uint8_t operand : {opers...})
        addByte(operand);
}