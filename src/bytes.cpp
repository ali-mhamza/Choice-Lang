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

	T result{};
	std::memcpy(&result, &value, sizeof(T));
	return result;
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

	ByteCode code{bytes, reconstructPool(poolSize)};
	if (debugInfoCombined)
		readDebugMetadata(code);
	else if (debugInfoExists)
		matchDebugMetadata(code);

	return code;
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

vObj CodeReader::reconstructPool(u64 poolByteSize)
{
	vObj pool{};
	auto stop{it + poolByteSize};

	while (it < stop)
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
					CH_PRINT(stderr, "Error: byte {} is {}.\n", it - cacheBytes.begin(),
						static_cast<u8>(type));
					exit(65);
				}

				it++;
			}
		}
	}

	return pool;
}

void CodeReader::readHeaders()
{
	readMagic();
	readVersionNum();
}

DebugInfoState CodeReader::readDebugState()
{
	return static_cast<DebugInfoState>(readValue<u8>());
}

std::string CodeReader::readFileName()
{
	std::string fileName{};
	u8 nameLength{readValue<u8>()};
	fileName.resize(nameLength);
	readBytes(fileName.data(), nameLength);
	return fileName;
}

std::vector<u64> CodeReader::readLineMarkers()
{
	u64 numMarkers{readValue<u64>()};
	std::vector<u64> lineMarkers(numMarkers);

	for (u64 i{0}; i < numMarkers; i++)
		lineMarkers[i] = readValue<u64>();

	return lineMarkers;
}

void CodeReader::readDebugMetadata(ByteCode& code)
{
	DebugReader reader{it, end};
	DebugMetadata metadata{reader.readMetadataBlock()};
	code.setDebugData(id, metadata);

	it += (metadata.size() * sizeof(DebugRange)) + sizeof(u64);
}

void CodeReader::matchDebugMetadata(ByteCode& code)
{
	code.setDebugData(id, data[dataIndex++]);
}

ByteCode CodeReader::readCache()
{
	debugInfoExists = false;
	debugInfoCombined = false;
	vByte codeBytes{};

	u64 codeSize{readValue<u64>()};
	codeBytes.resize(codeSize);
	u64 poolSize{readValue<u64>()};
	readBytes(codeBytes.data(), codeSize);

	return ByteCode{codeBytes, reconstructPool(poolSize)};
}

ByteCode CodeReader::readCache(std::vector<DebugMetadata>& metadata)
{
	debugInfoExists = true;
	debugInfoCombined = metadata.empty();
	data = metadata.data();

	vByte codeBytes{};

	u64 codeSize{readValue<u64>()};
	codeBytes.resize(codeSize);
	u64 poolSize{readValue<u64>()};
	readBytes(codeBytes.data(), codeSize);

	ByteCode code{codeBytes, reconstructPool(poolSize)};
	if (debugInfoCombined)
		readDebugMetadata(code);
	else if (debugInfoExists)
		matchDebugMetadata(code);

	return code;
}


/* DebugReader class. */

using Bytes::DebugReader;

DebugReader::DebugReader(vBit& it, vBit& end) :
	it{it}, end{end} {}

DebugReader::DebugReader(std::ifstream& debugFile)
{
    std::string debugContent{readFile(debugFile)};
    debugBytes = std::vector<u8>(debugContent.begin(), debugContent.end());
	it = debugBytes.begin();
	end = debugBytes.end();
}

template<typename T>
T DebugReader::readValue()
{
    T ret{readMemValue<T>(&it[0], &end[0])};

	it += sizeof(T);
	return ret;
}

std::vector<u64> DebugReader::readLineMarkers()
{
	u64 numMarkers{readValue<u64>()};
	std::vector<u64> lineMarkers(numMarkers);

	for (u64 i{0}; i < numMarkers; i++)
		lineMarkers[i] = readValue<u64>();

	return lineMarkers;
}

DebugMetadata DebugReader::readMetadataBlock()
{
	using AST::Statement::StmtType;
    using AST::Expression::ExprType;

	DebugMetadata metadata;
	u64 size{readValue<u64>()};

	for (u64 i{0}; i < size; i++)
	{
		u64 byteStart{readValue<u64>()};
		u64 byteEnd{readValue<u64>()};
		u64 sourceStart{readValue<u64>()};
		u64 sourceEnd{readValue<u64>()};

		metadata.push_back(DebugRange{
			byteStart, byteEnd, sourceStart, sourceEnd
		});
	}

	return metadata;
}

std::vector<DebugMetadata> DebugReader::readMetadata()
{
    std::vector<DebugMetadata> metadata{};

    while (it < end)
        metadata.push_back(readMetadataBlock());

    return metadata;
}