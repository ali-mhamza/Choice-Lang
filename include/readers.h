#pragma once
#include "bytecode.h"
#include "common.h"
#include <climits>
#include <fstream>

static_assert(CHAR_BIT == 8, "Incompatible ISA for interpreter.");

namespace Readers
{
    class CodeReader
    {
        private:
            std::ifstream& cacheFile;
            vBit it{};
            vBit end{};

            /* General helpers. */

            void readMagic();
            void readVersionNum();
            void handleFileLength(size_t expected);
            void eofError();
            template<typename T>
            T cacheRead(T* mem, size_t memSize = sizeof(T));

            /* Object reconstructors. */

            template<typename Size>
            [[nodiscard]] Size reconstructBytes();
            [[nodiscard]] ByteCode reconstructByteCode();
            [[nodiscard]] Object reconstructFunc();
            [[nodiscard]] Object reconstructString();

            /* Constant pool reconstructor. */

            [[nodiscard]] vObj reconstructPool(const vByte& poolBytes);

        public:
            CodeReader(std::ifstream& fileIn);
            CodeReader(const CodeReader&) = delete;
            CodeReader& operator=(const CodeReader&) = delete;
            [[nodiscard]] ByteCode readCache();
    };
}