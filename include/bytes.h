#pragma once
#include "bytecode.h"
#include "common.h"
#include "debug.h"
#include <climits>
#include <fstream>
#include <string>

static_assert(CHAR_BIT == 8, "Incompatible ISA for interpreter.");

namespace Bytes
{
    template<typename T>
    void encodeMemValue(u8* mem, const T value)
    {
        if (mem == nullptr) return;

        constexpr auto size{sizeof(T)};
        const u64* asBytes{reinterpret_cast<const u64*>(&value)};
        for (size_t i{0}; i < size; i++)
            mem[i] = (*asBytes >> ((size - 1 - i) * CHAR_BIT)) & 0xff;
    }

    template<typename T>
    void encodeValue(std::ofstream& os, const T value)
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
            // Debug info exists.
            bool debugInfoExists{};
            // Debug info  is combined with the bytecode, so both
            // must be read together.
            bool debugInfoCombined{};
            FileID id{};
            DebugMetadata* data{};
            u64 dataIndex{0};

            vBit it{};
            vBit end{};
            vByte cacheBytes{};

            /* Byte reading. */

            void readBytes(void* mem, size_t memSize);
            template<typename T>
            [[nodiscard]] T readValue();

            /* General helpers. */

            void readMagic();
            void readVersionNum();
            void readDebugMetadata(ByteCode& code);
            void matchDebugMetadata(ByteCode& code);

            /* Object reconstructors. */

            [[nodiscard]] ByteCode reconstructByteCode();
            [[nodiscard]] Object reconstructFunc();
            [[nodiscard]] Object reconstructString();

            /* Constant pool reconstructor. */

            [[nodiscard]] vObj reconstructPool(u64 poolByteSize);

        public:
            CodeReader(std::ifstream& cacheFile);

            CodeReader(const CodeReader&) = delete;
            CodeReader& operator=(const CodeReader&) = delete;

            void readHeaders();
            DebugInfoState readDebugState();
            std::string readFileName();
            std::vector<u64> readLineMarkers();
            void setFileID(FileID id) { this->id = id; }
            [[nodiscard]] ByteCode readCache();
            [[nodiscard]] ByteCode readCache(std::vector<DebugMetadata>& metadata);
    };

    class DebugReader
    {
        private:
            vByte debugBytes{};
            vBit it{};
            vBit end{};

            template<typename T>
            [[nodiscard]] T readValue();

        public:
            DebugReader(vBit& it, vBit& end);
            DebugReader(std::ifstream& debugFile);

            DebugReader(const DebugReader&) = delete;
            DebugReader& operator=(const DebugReader&) = delete;

            std::vector<u64> readLineMarkers();
            DebugMetadata readMetadataBlock();
            std::vector<DebugMetadata> readMetadata();
    };
}