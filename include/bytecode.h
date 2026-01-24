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
        ui64 countPool() const;

    public:
        ByteCode();
        ByteCode(const vByte& block);
        ByteCode(const vByte& block, const vObj& pool);

        template<typename Op, typename... Bytes>
        void addOp(Op op, Bytes... opers);

        void loadReg(ui8 reg, ui8 op);
        void loadRegConst(Object& constant, ui8 reg);
        // i16 to allow negative values while still
        // fitting all register values.
        ui64 addJump(Opcode op, i16 reg = -1);
        void patchJump(ui64 offset);
        inline ui64 codeSize() { return static_cast<ui64>(block.size()); }
        inline ui64 getLoopStart() { return codeSize(); }
        void addLoop(ui64 start);

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