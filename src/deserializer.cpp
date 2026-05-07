#include "../include/deserializer.h"
#include "../include/bytecode.h"
#include "../include/common.h"
#include "../include/linear_alloc.h"
#include "../include/object.h"
#include <array>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#if defined(DEBUG)
	#include <ios>
	#include <limits>
#endif

#if defined(DEBUG)
    #define CHECK_EOF()     \
        do {                \
            if (it == end)  \
			    eofError(); \
        } while (false)
#else
    #define CHECK_EOF()
#endif

Deserializer::Deserializer(std::ifstream& fileIn) :
    cacheFile{fileIn} {}

void Deserializer::readMagic()
{
	std::array<char, 6> magic{};
	cacheFile.read(magic.data(), sizeof(magic));
	handleFileLength(sizeof(magic));
	if (strncmp(magic.data(), "choice", 6) != 0)
	{
		CH_PRINT(stderr, "Improper magic flag for bytecode file.\n");
		exit(65);
	}
}

void Deserializer::readVersionNum()
{
	std::array<char, 3> num{};
	cacheFile.read(num.data(), sizeof(num));
	handleFileLength(sizeof(num));
}

void Deserializer::handleFileLength(size_t expected)
{
	if (static_cast<size_t>(cacheFile.gcount()) < expected)
	{
		if (cacheFile.eof())
			eofError();
		else if (cacheFile.fail())
		{
			CH_PRINT(stderr, "Encountered internal I/O error.\n");
			exit(74);
		}
	}
}

void Deserializer::eofError()
{
	CH_PRINT(stderr, "Reached end of file prematurely.\n");
	exit(65);
}

template<typename T>
void Deserializer::cacheRead(T* mem, size_t memSize)
{
    cacheFile.read(reinterpret_cast<char*>(mem), memSize);
    handleFileLength(memSize);
}

template<typename Size>
Size Deserializer::reconstructBytes()
{
	u64 value{0};
	constexpr size_t size{sizeof(Size)};
	for (size_t i{0}; i < size; i++)
	{
		CHECK_EOF();
		value = (value << CHAR_BIT) | *(it++);
	}

	it--;
	Size* temp{reinterpret_cast<Size*>(&value)};
	return *temp;
}

ByteCode Deserializer::reconstructByteCode()
{
	u64 codeSize{reconstructBytes<u64>()};
	it++;
	u64 poolSize{reconstructBytes<u64>()};
	it++;

	vByte bytes(codeSize);
	for (u64 i{0}; i < codeSize; i++)
	{
		CHECK_EOF();
		bytes[i] = *(it++);
	}

	vByte pool(poolSize);
	for (u64 i{0}; i < poolSize; i++)
	{
		CHECK_EOF();
		pool[i] = *(it++);
	}

	it--;
	return ByteCode{bytes, reconstructPool(pool)};
}

[[nodiscard]]
Object Deserializer::reconstructFunc()
{
	CHECK_EOF();
	u8 nameLen{*(it++)};
	std::string name{};

	if (nameLen != 0)
	{
    	name.resize(nameLen);

    	#if defined(DEBUG)
       	if (it + nameLen > end)
       	    eofError();
    	#endif

    	for (u8 i{0}; i < nameLen; i++)
    	    name[i] = static_cast<char>(*(it++));
	}

	CHECK_EOF();
	u8 argCount{*(it++)};

	CHECK_EOF();
	bool lambda{static_cast<bool>(*(it++))};

	if (lambda)
		return Object{CH_ALLOC(Function, reconstructByteCode(), argCount)};
	else
		return Object{CH_ALLOC(Function, name, reconstructByteCode(), argCount)};
}

Object Deserializer::reconstructString()
{
	CHECK_EOF();
	std::string str{};
	while (static_cast<char>(*it) != '\0')
	{
		str.push_back(static_cast<char>(*it));
		it++;
		CHECK_EOF();
	}

	return Object{CH_ALLOC(String, str)};
}

vObj Deserializer::reconstructPool(const vByte& poolBytes)
{
	vObj pool{};
	// Since we reuse this method recursively in reconstructFunc.
	vBit currentIt{this->it}, currentEnd{this->end};

	this->it = poolBytes.begin();
	this->end = poolBytes.end();

	while (it < end)
	{
		ObjType type{static_cast<ObjType>(*(it++))};
		switch (type)
		{
			case OBJ_INT:       pool.emplace_back(reconstructBytes<i64>());     break;
			case OBJ_DEC:       pool.emplace_back(reconstructBytes<double>());  break;
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
			}
		}

		it++;
	}

	this->it = currentIt;
	this->end = currentEnd;
	return pool;
}

ByteCode Deserializer::readCache()
{
	if (cacheFile.is_open())
	{
		std::string fileName{};	u8 nameLength{};
		vByte codeBytes{};		u64 codeLength{};
		vByte poolBytes{};		u64 poolLength{};

		readMagic();
		readVersionNum();

		int ch{cacheFile.get()};
		if (ch == -1) // EOF.
			eofError();
		nameLength = static_cast<u8>(ch);
		fileName.resize(nameLength);

		cacheRead(&codeLength);
		codeBytes.resize(codeLength);

		cacheRead(&poolLength);
		poolBytes.resize(poolLength);

		cacheRead(fileName.data(), nameLength);
		file = fileName;

		#if defined(DEBUG)
			constexpr auto maxSize{static_cast<u64>(
				std::numeric_limits<std::streamsize>::max()
			)};
			CH_ASSERT(
				(codeLength < maxSize) && (poolLength < maxSize),
				"File serialization did not bounds-check bytecode "
				"and constant pool sizes."
			);
		#endif

		cacheRead(codeBytes.data(), codeLength);
		cacheRead(poolBytes.data(), poolLength);

		cacheFile.close();
		return ByteCode{codeBytes, reconstructPool(poolBytes)};
	}

	CH_PRINT(stderr, "File is closed.\n");
	exit(66);
}