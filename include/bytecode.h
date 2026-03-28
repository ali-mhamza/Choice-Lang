#pragma once
#include "common.h"
#include "opcodes.h"
#include <fstream>
#include <vector>

class Disassembler;
struct Function;
class Object;
class VM;

class ByteCode
{
    private:
        vByte block;
        vObj pool;

        void addByte(ui8 byte);
        template<typename... Bytes>
        inline void addBytes(Bytes... bytes);
        // Using big endian.
        void addShort(ui16 bytes);
        void addLong(ui32 bytes);

        ui64 countPool() const;

        void clearCode();
        void clearPool();

    public:
        ByteCode();
        ByteCode(const vByte& block);
        ByteCode(const vByte& block, const vObj& pool);

        void addOp(Opcode op);
        template<typename Op, typename... Bytes>
        inline void addOp(Op op, Bytes... opers);

        // i16 to allow negative values while still
        // fitting all register values.
        ui64 addJump(Opcode op, i16 reg = -1);
        void patchJump(ui64 offset);

        inline ui64 getLoopStart() const { return codeSize(); }
        void addLoop(ui64 start);

        void loadReg(ui8 reg, ui8 op);
        void loadRegConst(Object& constant, ui8 reg);

        inline ui64 codeSize() const { return static_cast<ui64>(block.size()); }
        void cacheStream(std::ofstream& os) const;
        void clear();

        friend class Disassembler;
        friend struct Function;
        friend class VM;
};

template<typename... Bytes>
inline void ByteCode::addBytes(Bytes... bytes)
{
    for (ui8 byte : {bytes...})
        addByte(byte);
}

template<typename Op, typename... Bytes>
inline void ByteCode::addOp(Op op, Bytes... opers)
{
    addByte(static_cast<ui8>(op));
    for (ui8 operand : {opers...})
        addByte(operand);
}