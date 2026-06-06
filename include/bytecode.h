#pragma once
#include "common.h"
#include "debug.h"
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
        vByte block{};
        vObj pool{};
        FileID id{};
        DebugMetadata metadata{};

        void addByte(u8 byte);
        template<typename... Bytes>
        void addBytes(Bytes... bytes);
        // Using big endian.
        void addShort(u16 bytes);
        void addLong(u32 bytes);

        // Return the size of the constant pool once serialized.
        [[nodiscard]] u64 countPool() const;
        void sortMetadata();

    public:
        ByteCode() = default;
        ByteCode(const vByte& block);
        ByteCode(const vByte& block, const vObj& pool);

        void addOp(Opcode op);
        template<typename... Bytes>
        void addOp(Opcode op, Bytes... opers);

        void setDebugData(FileID id, const DebugMetadata& metadata);

        // Add a jump instruction with an optional condition
        // register.
        // i16 to allow -1 while still fitting all register values.
        [[nodiscard]] u64 addJump(Opcode op, i16 reg = -1);
        // Fill in the two-byte operand for a jump instruction.
        void patchJump(u64 offset);

        [[nodiscard]] u64 getLoopStart() const { return codeSize(); }
        void addLoop(u64 start);

        // Load a register with an immediate, opcode-based value.
        void loadReg(u8 reg, u8 op);
        // Store a constant in the constant pool, and emit a load
        // instruction to store it in a register.
        void loadRegConst(Object& constant, u8 reg);

        [[nodiscard]]
        u64 codeSize() const { return static_cast<u64>(block.size()); }
        void clearCode();
        void clearPool();
        // Clear code and constant pool.
        void clear();

        // Serialize data headers into a file.
        void encodeHeaders(std::ofstream& os) const;
        // Serialize code and pool data into a file.
        void encodeData(std::ofstream& os) const;
        // Serialize debug metadata into a file.
        void encodeMetadata(std::ofstream& os) const;

        friend class Disassembler;
        friend struct Function;
        friend class VM;
};

template<typename... Bytes>
void ByteCode::addBytes(Bytes... bytes)
{
    for (u8 byte : {bytes...})
        addByte(byte);
}

template<typename... Bytes>
void ByteCode::addOp(Opcode op, Bytes... opers)
{
    addByte(static_cast<u8>(op));
    addBytes(opers...);
}