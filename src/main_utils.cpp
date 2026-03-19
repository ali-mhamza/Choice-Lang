#include "../include/main_utils.h"
#include "../include/bytecode.h"
#include "../include/common.h"
#include "../include/disasm.h"
#include "../include/linear_alloc.h"
#include "../include/object.h"
#include "../include/tokprinter.h"
#include "../include/utils.h"
#include "../include/vm.h"
#include <array>
#include <climits> // For CHAR_BIT.
#include <cstdio> // For stderr.
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using vBit = vByte::const_iterator;

std::string readFile(const char* fileName)
{
	std::ifstream file(fileName);
	if (file.fail())
	{
		CH_PRINT(stderr, "Failed to open file.\n");
		exit(66);
	}
	
	if (file.is_open())
	{
		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string fileString = buffer.str();
		file.close();
		return fileString;
	}

	CH_PRINT(stderr, "File is closed.\n");
	exit(66);
}

static inline void eofError()
{
	CH_PRINT(stderr, "Reached end of file prematurely.\n");
	exit(65);
}

template<typename Size>
static Size reconstructBytes(vBit& it, const vBit& end)
{
	ui64 value = 0;
	size_t size = sizeof(Size);
	for (size_t i = 0; i < size; i++)
	{
		CHECK_EOF();
		value = (value << CHAR_BIT) | *(it++);
	}
	it--;
	Size* temp = reinterpret_cast<Size*>(&value);
	return *temp;
}

static ByteCode reconstructByteCode(vBit& it, const vBit& end)
{
	ui64 codeSize = reconstructBytes<ui64>(it, end);
	it++;
	ui64 poolSize = reconstructBytes<ui64>(it, end);
	it++;

	vByte bytes(codeSize);
	for (ui64 i = 0; i < codeSize; i++)
	{
		CHECK_EOF();
		bytes[i] = *(it++);
	}

	vByte pool(poolSize);
	for (ui64 i = 0; i < poolSize; i++)
	{
		CHECK_EOF();
		pool[i] = *(it++);
	}
	it--;

	return ByteCode(bytes, reconstructPool(pool));
}

static Object reconstructFunc(vBit& it, const vBit& end)
{
	CHECK_EOF();
	std::string name;
	while (static_cast<char>(*it) != '\0')
	{
		name.push_back(static_cast<char>(*it));
		it++;
		CHECK_EOF();
	}

	++it;
	CHECK_EOF();
	ui8 argCount = *it;

	++it;
	CHECK_EOF();
	bool lambda = static_cast<bool>(*it);

	if (lambda)
	{
		return Object(CH_ALLOC(Function, reconstructByteCode(++it, end),
			argCount)
		);
	}
	else
	{
		return Object(CH_ALLOC(Function, name,
			reconstructByteCode(++it, end),
			argCount)
		);
	}
}

static Object reconstructString(vBit& it, const vBit& end)
{
	CHECK_EOF();
	std::string str;
	while (static_cast<char>(*it) != '\0')
	{
		str.push_back(static_cast<char>(*it));
		it++;
		CHECK_EOF();
	}

	return Object(CH_ALLOC(String, str));
}

static Object reconstructRange(vBit& it, const vBit& end)
{
	std::array<i64, 3> array;
	for (int i = 0; i < 3; i++)
	{
		CHECK_EOF();
		array[i] = reconstructBytes<i64>(it, end);
		// reconstructBytes decrements the iterator once done
		// to account for the extra increment for the last loop
		// iteration.
		// This undoes that to actually move the iterator forward.
		it++;
	}

	it--;
	return Object(CH_ALLOC(Range, array));
}

vObj reconstructPool(const vByte& poolBytes)
{
	vObj pool;

	for (auto it = poolBytes.begin(); it < poolBytes.end(); it++)
	{
		ObjType type = static_cast<ObjType>(*it);
		switch (type)
		{
			case OBJ_INT:
				pool.push_back(reconstructBytes<i64>(++it, poolBytes.end()));
				break;
			case OBJ_DEC:
				pool.push_back(reconstructBytes<double>(++it, poolBytes.end()));
				break;
			case OBJ_FUNC:
				pool.push_back(reconstructFunc(++it, poolBytes.end()));
				break;
			case OBJ_STRING:
				pool.push_back(reconstructString(++it, poolBytes.end()));
				break;
			case OBJ_RANGE:
				pool.push_back(reconstructRange(++it, poolBytes.end()));
				break;
			default:
			{
				if ((type != OBJ_BOOL) && (type != OBJ_NULL))
				{
					CH_PRINT(stderr, "Error: byte is {}.\n",
						static_cast<int>(type));
					exit(65);
				}
			}
		}
	}

	return pool;
}

static void handleFileLength(std::ifstream& fileIn, size_t expected)
{
	if (static_cast<size_t>(fileIn.gcount()) < expected)
	{
		if (fileIn.eof())
			eofError();
		else if (fileIn.fail())
		{
			CH_PRINT(stderr, "Encountered internal I/O error.\n");
			exit(74);
		}
	}
}

static void readMagic(std::ifstream& fileIn)
{
	char magic[6];
	fileIn.read(magic, sizeof(magic));
	handleFileLength(fileIn, sizeof(magic));
	if (strncmp(magic, "choice", 6) != 0)
	{
		CH_PRINT(stderr, "Improper magic flag for bytecode file.\n");
		exit(65);
	}
}

static void readVersionNum(std::ifstream& fileIn)
{
	char num[3];
	fileIn.read(num, sizeof(num));
	handleFileLength(fileIn, sizeof(num));
}

ByteCode readCache(std::ifstream& fileIn)
{
	if (fileIn.is_open())
	{
		std::string fileName;	ui8 nameLength;
		vByte codeBytes;		ui64 codeLength;
		vByte poolBytes;		ui64 poolLength;

		readMagic(fileIn);
		readVersionNum(fileIn);

		int ch = fileIn.get();
		if (ch == -1) // EOF.
			eofError();
		nameLength = static_cast<ui8>(ch);
		fileName.resize(nameLength);

		fileIn.read(reinterpret_cast<char*>(&codeLength), sizeof(ui64));
		handleFileLength(fileIn, sizeof(ui64));
		codeBytes.resize(codeLength);

		fileIn.read(reinterpret_cast<char*>(&poolLength), sizeof(ui64));
		handleFileLength(fileIn, sizeof(ui64));
		poolBytes.resize(poolLength);

		fileIn.read(reinterpret_cast<char*>(fileName.data()), nameLength);
		handleFileLength(fileIn, nameLength);
		file = fileName;

		fileIn.read(reinterpret_cast<char*>(codeBytes.data()), codeLength);
		handleFileLength(fileIn, codeLength);

		fileIn.read(reinterpret_cast<char*>(poolBytes.data()), poolLength);
		handleFileLength(fileIn, poolLength);

		fileIn.close();
		return ByteCode(codeBytes, reconstructPool(poolBytes));
	}

	CH_PRINT(stderr, "File is closed.\n");
	exit(66);
}

void optionShowTokens(const vT& tokens)
{
	TokenPrinter printer(tokens);
	printer.printTokens();
}

void optionShowBytes(const ByteCode& chunk)
{
	Disassembler dis(chunk);
	dis.disassembleCode();
}

void optionCacheBytes(const ByteCode& chunk, const char* fileName)
{
	std::filesystem::path filePath(fileName);
	filePath.replace_extension(".bch");
	std::ofstream cacheFile(filePath.filename().c_str(), std::ios::binary);
	chunk.cacheStream(cacheFile);
}

void optionLoad(const char* fileName)
{	
	if (!ends_with(fileName, ".bch"))
	{
		CH_PRINT(stderr, "Invalid bytecode file.\n");
		exit(65);
	}

	external = true;

	std::ifstream program(fileName, std::ios::binary);
	if (program.fail())
	{
		CH_PRINT(stderr, "Failed to open file.\n");
		exit(66);
	}

	ByteCode chunk = readCache(program);
	VM vm;
	vm.executeCode(CH_ALLOC(Function, chunk, 0));
}

void optionDis(const char* fileName)
{
	if (!ends_with(fileName, ".bch"))
	{
		CH_PRINT(stderr, "Invalid bytecode file.\n");
		exit(65);
	}

	external = true;

	std::ifstream program(fileName, std::ios::binary);
	if (program.fail())
	{
		CH_PRINT(stderr, "Failed to open file.\n");
		exit(66);
	}

	ByteCode chunk = readCache(program);
	Disassembler dis(chunk);
	dis.disassembleCode();
}

bool fileNameCheck(std::string_view fileName)
{
	return (ends_with(fileName, ".ch") || ends_with(fileName, ".bch"));
}

std::string normalizeNewlines(const std::string_view& sv)
{
	std::string str(sv);
	str.erase(std::remove_if(str.begin(), str.end(), [](char c){
        return (c == '\r');
    }), str.end());
	return str;
}