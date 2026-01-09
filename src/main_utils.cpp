#include "../include/main_utils.h"
#include "../include/bytecode.h"
#include "../include/common.h"
#include "../include/disasm.h"
#include "../include/object.h"
#include "../include/utils.h"
#include "../include/vm.h"
#include <cstdio> // For stderr.
#include <cstring>
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
		FORMAT_PRINT(stderr, "Failed to open file.\n");
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

	FORMAT_PRINT(stderr, "File is closed.\n");
	exit(66);
}

static inline void eofError()
{
	FORMAT_PRINT(stderr, "Reached end of file prematurely.\n");
	exit(65);
}

template<typename Size>
static Size reconstructBytes(vBit& it, const vBit& end)
{
	Size value = 0;
	ui8 bytes[sizeof(Size)];
	for (size_t i = 0; i < sizeof(Size); i++)
	{
		if (it == end)
			eofError();
		bytes[i] = *(it++);
	}
	it--;
	std::memcpy(&value, &bytes[0], sizeof(Size));
	return value;
}

static Object reconstructString(vBit& it, const vBit& end)
{
	std::string str;
	if (it == end)
		eofError();
	while (static_cast<char>(*it) != '\0')
	{
		str.push_back(static_cast<char>(*it));
		it++;
		if (it == end)
			eofError();
	}

	HeapObj* obj = new String(str);
	return Object(obj);
}

static Object reconstructHeapObj(vBit& it, const vBit& end)
{
	if (it == end)
		eofError();
	ObjType type = static_cast<ObjType>(*it);
	it++;
	switch (type)
	{
		case OBJ_STRING:	return reconstructString(it, end);
		default:			UNREACHABLE();
	}
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
			default:
			{
				if (IS_HEAP_TYPE(type))
				{
					pool.push_back(reconstructHeapObj(it, poolBytes.end()));
					break;
				}
				else if ((type != OBJ_BOOL) && (type != OBJ_NULL))
				{
					FORMAT_PRINT(stderr, "Error: byte is {}.\n",
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
	if (fileIn.gcount() < expected)
	{
		if (fileIn.eof())
			eofError();
		else if (fileIn.fail())
		{
			FORMAT_PRINT(stderr, "Encountered internal I/O error.\n");
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
		FORMAT_PRINT(stderr, "Improper magic flag for bytecode file.\n");
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
		nameLength = static_cast<ui8>(ch); // Check for EOF.
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

	FORMAT_PRINT(stderr, "File is closed.\n");
	exit(66);
}

void optionLoad(const char* fileName)
{	
	if (!ends_with(fileName, ".bch"))
	{
		FORMAT_PRINT(stderr, "Invalid bytecode file.\n");
		exit(65);
	}

	external = true;

	std::ifstream program(fileName, std::ios::binary);
	if (program.fail())
	{
		FORMAT_PRINT(stderr, "Failed to open file.\n");
		exit(66);
	}

	ByteCode chunk = readCache(program);
	VM vm;
	vm.executeCode(chunk);
}

void optionDis(const char* fileName)
{
	if (!ends_with(fileName, ".bch"))
	{
		FORMAT_PRINT(stderr, "Invalid bytecode file.\n");
		exit(65);
	}

	external = true;

	std::ifstream program(fileName, std::ios::binary);
	if (program.fail())
	{
		FORMAT_PRINT(stderr, "Failed to open file.\n");
		exit(66);
	}

	ByteCode chunk = readCache(program);
	Disassembler dis(chunk);
	dis.disassembleCode();
}

void optionCacheBytes(const ByteCode& chunk, const char* fileName)
{
	std::string outFile(fileName);
	// Cut off extension.
	outFile = outFile.substr(0, outFile.size() - 3);
	// Add new extension.
	outFile += ".bch";
	std::ofstream cacheFile(outFile, std::ios::binary);
	chunk.cacheStream(cacheFile);
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