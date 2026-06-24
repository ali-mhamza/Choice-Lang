#pragma once
#include "bytecode.h"
#include "common.h"
#include "debug.h"
#include <climits>
#include <cstring>
#include <fstream>
#include <string>

static_assert(CHAR_BIT == 8, "Incompatible ISA for interpreter.");

namespace Bytes
{
    template<typename T>
    void encodeMemValue(u8* mem, const T value)
    {
        static_assert(sizeof(T) <= sizeof(u64), "Encoded value too large.");

        if (mem == nullptr) return;

        constexpr auto size{sizeof(T)};
        u64 asBytes{};
        memcpy(&asBytes, &value, size);
        for (size_t i{0}; i < size; i++)
            mem[i] = (asBytes >> ((size - 1 - i) * CHAR_BIT)) & 0xff;
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
            // Debug info is combined with the bytecode, so both
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
            [[nodiscard]] Object reconstructType();
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

    class BinaryInspector
    {
        private:
            using sv = std::string_view;

            vByte cacheBytes{};
            vBit start{};
            vBit it{};
            vBit end{};
            DebugInfoState state{};

            /* Byte reading. */

            void readBytes(void* mem, size_t memSize);
            template<typename T>
            [[nodiscard]] T readValue();

            [[nodiscard]] u64 getCurrentPosition() const { return it - start; }
            static void printStartEnd(u64 start, u64 end, bool indent);
            static void printEntryTitle(sv title, u64 titleLength);
            static void printStringWithTruncation(
                std::string& str,
                u64 displayLen,
                sv truncMsg,
                bool center
            );

            void inspectHeaders();
            void inspectFileName();
            void inspectLineMarkers();
            void inspectByteCode();

            void inspectConstantPool(u64 poolSize);

            // Functions to display compact views of objects
            // in a neat table.

            void inspectBriefObject(u64& position);
            void inspectBriefInt(u64 start);
            void inspectBriefDec(u64 start);
            void inspectBriefString(u64 start);

            void skipByteCode();
            void inspectBriefType(u64 start);

            void skipFuncData();
            void inspectBriefFunc(u64 start);

            // Functions to display detailed views of objects
            // and their components.

            void inspectDetailObject(u64& position);
            void inspectDetailInt(u64 start);
            void inspectDetailDec(u64 start);
            void inspectDetailString(u64 start);

            void inspectDetailTypeFields();
            void inspectDetailType(u64 start);

            // Function name.
            void inspectDetailFuncName();
            // Arity and bytecode.
            void inspectDetailFuncComponents(u8& arityMin, u8& arityMax);
            // Metadata and default arguments.
            void inspectDetailFuncExtras(u8 arityMin, u8 arityMax);
            void skipFuncDefaultArgs(u8 defaultArgs);
            void inspectDetailFunc(u64 start);

            void inspectMetadata();

        public:
            BinaryInspector(std::ifstream& cacheFile);
            void inspect();
    };
}