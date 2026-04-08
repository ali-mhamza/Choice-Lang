#pragma once
#include "common.h"
#include "opcodes.h"
#include <fstream>      // For cacheStream() method.
#include <vector>

class Disassembler;
struct Function;
class Object;
class VM;

class ByteCode
{
    private:
        vByte block{};
        vObj pool{};

        void addByte(ui8 byte);
        template<typename... Bytes>
        void addBytes(Bytes... bytes);
        // Using big endian.
        void addShort(ui16 bytes);
        void addLong(ui32 bytes);

        // Return the size of the constant pool once serialized.
        ui64 countPool() const;
        void clearCode();
        void clearPool();

    public:
        ByteCode() = default;
        ByteCode(const vByte& block);
        ByteCode(const vByte& block, const vObj& pool);

        void addOp(Opcode op);
        template<typename... Bytes>
        void addOp(Opcode op, Bytes... opers);

        // Add a jump instruction with an optional condition
        // register
        // i16 to allow -1 while still fitting all register values.
        ui64 addJump(Opcode op, i16 reg = -1);
        // Fill in the two-byte operand for a jump instruction.
        void patchJump(ui64 offset);

        ui64 getLoopStart() const { return codeSize(); }
        void addLoop(ui64 start);

        // Load a register with an immediate, opcode-based value.
        void loadReg(ui8 reg, ui8 op);
        // Store a constant in the constant pool, and emit a load
        // instruction to store it in a register.
        void loadRegConst(Object& constant, ui8 reg);

        ui64 codeSize() const { return static_cast<ui64>(block.size()); }
        // Serialize a ByteCode object into a file.
        void cacheStream(std::ofstream& os) const;
        // Clear code and constant pool.
        void clear();

        friend class Disassembler;
        friend struct Function;
        friend class VM;
};

template<typename... Bytes>
void ByteCode::addBytes(Bytes... bytes)
{
    for (ui8 byte : {bytes...})
        addByte(byte);
}

template<typename... Bytes>
void ByteCode::addOp(Opcode op, Bytes... opers)
{
    addByte(static_cast<ui8>(op));
    addBytes(opers...);
}