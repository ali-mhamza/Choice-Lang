#pragma once
#include "bytecode.h"
#include "common.h"
#include "diagnostic.h"
#include <fstream>
#include <climits>
#include <fstream>
#include <string>

static_assert(CHAR_BIT == 8, "Incompatible ISA for interpreter.");

namespace Bytes
{
    template<typename T>
    void encodeMemValue(u8* mem, T value)
    {
        if (mem == nullptr) return;

        constexpr auto size{sizeof(T)};
        u64* asBytes{reinterpret_cast<u64*>(&value)};
        for (size_t i{0}; i < size; i++)
            mem[i] = (*asBytes >> ((size - 1 - i) * CHAR_BIT)) & 0xff;
    }

    template<typename T>
    void encodeValue(std::ofstream& os, T value)
    {
        std::array<u8, sizeof(T)> bytes{};
        encodeMemValue(bytes.data(), value);
        os.write(reinterpret_cast<const char*>(bytes.data()), sizeof(T));
    }

    template<typename T>
    [[nodiscard]] T readMemValue(const u8* mem, const u8* end);

    class CodeReader
    {
        private:
            vByte cacheBytes{};
            vBit it{};
            vBit end{};

            /* Byte reading. */

            void readBytes(void* mem, size_t memSize);
            template<typename T>
            [[nodiscard]] T readValue();

            /* General helpers. */

            void readMagic();
            void readVersionNum();

            /* Object reconstructors. */

            [[nodiscard]] ByteCode reconstructByteCode();
            [[nodiscard]] Object reconstructFunc();
            [[nodiscard]] Object reconstructString();

            /* Constant pool reconstructor. */

            [[nodiscard]] vObj reconstructPool(const vByte& poolBytes);

        public:
            CodeReader(std::ifstream& cacheFile);

            CodeReader(const CodeReader&) = delete;
            CodeReader& operator=(const CodeReader&) = delete;

            [[nodiscard]] ByteCode readCache();
    };

    class DebugReader
    {
        private:
            vByte debugBytes{};
            u64 index{0};

            template<typename T>
            [[nodiscard]] T readValue();

        public:
            DebugReader(std::ifstream& debugFile);

            DebugReader(const DebugReader&) = delete;
            DebugReader& operator=(const DebugReader&) = delete;

            std::vector<DbgBlock> decodeBlocks();
    };
}