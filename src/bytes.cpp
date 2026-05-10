#include "../include/bytes.h"
#include "../include/astnodes.h"
#include "../include/bytecode.h"
#include "../include/common.h"
#include "../include/linear_alloc.h"
#include "../include/main_utils.h"
#include "../include/object.h"
#include "base.h"
#include <array>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

static void eofError()
{
	CH_PRINT(stderr, "Reached end of file prematurely.\n");
	exit(65);
}

template<typename T>
T Bytes::readMemValue(const u8* mem, const u8* end)
{
    static_assert(sizeof(T) <= sizeof(u64),
        "Invalid size template type for 'readValue' function.");

    if (mem + sizeof(T) > end)
        eofError();

    u64 value{0};
	for (size_t i{0}; i < sizeof(T); i++)
		value = (value << CHAR_BIT) | mem[i];

	T* temp{reinterpret_cast<T*>(&value)};
	return *temp;
}

/* CodeReader class. */

using Bytes::CodeReader;

CodeReader::CodeReader(std::ifstream& cacheFile)
{
    std::string cached{readFile(cacheFile)};
    cacheBytes = std::vector<u8>(cached.begin(), cached.end());
    it = cacheBytes.begin();
    end = cacheBytes.end();
}

void CodeReader::readBytes(void* mem, size_t memSize)
{
    if (it + memSize > end)
		eofError();

    std::memcpy(mem, &it[0], memSize);
    it += memSize;
}

template<typename T>
T CodeReader::readValue()
{
    T ret{readMemValue<T>(&it[0], &end[0])};
    it += sizeof(T);
    return ret;
}

void CodeReader::readMagic()
{
	std::array<char, 6> magic{};
	readBytes(magic.data(), 6);
	if (strncmp(magic.data(), "choice", 6) != 0)
	{
		CH_PRINT(stderr, "Improper magic flag for bytecode file.\n");
		exit(65);
	}
}

void CodeReader::readVersionNum()
{
	std::array<u8, 3> num{};
	readBytes(num.data(), 3);

	if (num[0] != CH_VERSION_MAJOR)
	{
		CH_PRINT(stderr,
			"File version is incompatible with current language implementation.\n" \
			"Please update to a newer, compatible version."
		);
		exit(EXIT_FAILURE);
	}
}

ByteCode CodeReader::reconstructByteCode()
{
	u64 codeSize{readValue<u64>()};
	u64 poolSize{readValue<u64>()};

	vByte bytes(codeSize);
	readBytes(bytes.data(), codeSize);

	vByte pool{};
	// The pool for a function (unlike the code block)
	// may be empty.
	if (poolSize != 0)
	{
		pool.resize(poolSize);
		readBytes(pool.data(), poolSize);
	}

	return ByteCode{bytes, reconstructPool(pool)};
}

[[nodiscard]]
Object CodeReader::reconstructFunc()
{
	u8 nameLen{readValue<u8>()};
	std::string name{};

	// Name is not encoded for a lambda.
	if (nameLen != 0)
	{
    	name.resize(nameLen);
        readBytes(name.data(), nameLen);
	}

	u8 argCount{readValue<u8>()};
	bool lambda{readValue<bool>()};

	if (lambda)
		return Object{CH_ALLOC(Function, reconstructByteCode(), argCount)};
	else
		return Object{CH_ALLOC(Function, name, reconstructByteCode(), argCount)};
}

Object CodeReader::reconstructString()
{
    u64 length{readValue<u64>()};
	std::string str{};

	if (length != 0)
	{
		str.resize(length);
		readBytes(str.data(), length);
	}

	return Object{CH_ALLOC(String, str)};
}

vObj CodeReader::reconstructPool(const vByte& poolBytes)
{
	vObj pool{};
	// Since we reuse this method recursively in reconstructFunc.
	vBit currentIt{this->it}, currentEnd{this->end};

	this->it = poolBytes.begin();
	this->end = poolBytes.end();

	while (it < end)
	{
		ObjType type{readValue<ObjType>()};
		switch (type)
		{
			case OBJ_INT:       pool.emplace_back(readValue<i64>());            break;
			case OBJ_DEC:       pool.emplace_back(readValue<double>());         break;
			case OBJ_FUNC:
			case OBJ_LAMBDA:    pool.emplace_back(reconstructFunc());           break;
			case OBJ_STRING:    pool.emplace_back(reconstructString());         break;
			default:
			{
				if ((type != OBJ_BOOL) && (type != OBJ_NULL))
				{
					CH_PRINT(stderr, "Error: byte is {}.\n", static_cast<u8>(type));
					exit(65);
				}

				it++;
			}
		}
	}

	this->it = currentIt;
	this->end = currentEnd;
	return pool;
}

ByteCode CodeReader::readCache()
{
	readMagic();
	readVersionNum();

	std::string fileName{};
	u8 nameLength{readValue<u8>()};
	fileName.resize(nameLength);

	vByte codeBytes{}, poolBytes{};

	u64 codeSize{readValue<u64>()};
	codeBytes.resize(codeSize);
	u64 poolSize{readValue<u64>()};

	readBytes(fileName.data(), nameLength);
	file = fileName;
	readBytes(codeBytes.data(), codeSize);

	if (poolSize != 0)
	{
		poolBytes.resize(poolSize);
		readBytes(poolBytes.data(), poolSize);
	}

	return ByteCode{codeBytes, reconstructPool(poolBytes)};
}


/* DebugReader class. */

using Bytes::DebugReader;

DebugReader::DebugReader(std::ifstream& debugFile)
{
    std::string debugContent{readFile(debugFile)};
    debugBytes = std::vector<u8>(debugContent.begin(), debugContent.end());
}

template<typename T>
T DebugReader::readValue()
{
    return readMemValue<T>(debugBytes.data() + index,
        debugBytes.data() + debugBytes.size());
}

std::vector<DbgBlock> DebugReader::decodeBlocks()
{
    using AST::Statement::StmtType;
    using AST::Expression::ExprType;

    std::vector<DbgBlock> blocks{};

    while (index < debugBytes.size())
    {
        blocks.emplace_back();
        u64 size{readValue<u64>()};

        for (u64 i{0}; i < size; i++)
        {
            u64 byteStart{readValue<u64>()};
            u64 byteEnd{readValue<u64>()};
            u64 offset{readValue<u64>()};
            u64 lineNo{readValue<u64>()};
            (void) lineNo;

            u8 typeByte{readValue<u8>()};
            bool isStmt{(typeByte & 0x80) == 0}; // Check if first bit is set/reset.

            blocks.back().push_back(DbgRange{
                isStmt,
                static_cast<StmtType>(typeByte),
                static_cast<ExprType>(typeByte & 0x7f), // Reset first bit.
                byteStart, byteEnd, offset
            });
        }
    }

    return blocks;
}