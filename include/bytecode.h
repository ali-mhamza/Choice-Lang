#pragma once
#include "common.h"
#include "object.h"
#include <fstream>
#include <vector>

class VM;
class Disassembler;

class ByteCode
{
    private:
        vByte block;
        vObj pool;

        void addByte(ui8 byte);
        template<typename... Bytes>
        void addBytes(Bytes... bytes);
        // Using big endian.
        void addShort(ui16 bytes);
        void addLong(ui32 bytes);

    public:
        ByteCode();
        ByteCode(const vByte& block);
        ByteCode(const vByte& block, const vObj& pool);

        void loadReg(ui8 reg, ui8 op);
        void loadRegConst(Object& constant, ui8 reg);
        template<typename Op, typename... Bytes>
        void addOp(Op op, Bytes... opers);

        void cacheStream(std::ofstream& os) const;
        void clearCode();
        void clearPool();
        void clear();

        friend class VM;
        friend class Disassembler;
};

template<typename... Bytes>
void ByteCode::addBytes(Bytes... bytes)
{
    for (ui8 byte : {bytes...})
        addByte(byte);
}

template<typename Op, typename... Bytes>
void ByteCode::addOp(Op op, Bytes... opers)
{
    addByte(static_cast<ui8>(op));
    for (ui8 operand : {opers...})
        addByte(operand);
}