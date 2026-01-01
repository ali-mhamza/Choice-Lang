#include "../include/main_utils.h"
#include "../include/bytecode.h"
#include "../include/common.h"
#include "../include/disasm.h"
#include "../include/object.h"
#include "../include/utils.h"
#include "../include/vm.h"
#include <iostream>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

std::string readFile(const char* fileName)
{
	std::ifstream file(fileName);
	if (file.fail())
	{
		std::cerr << "Failed to open file.\n";
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

	std::cerr << "File is closed.\n";
	exit(66);
}

template<typename Size>
static Size reconstructBytes(vByte::const_iterator& it)
{
	Size value = 0;
	ui8 bytes[sizeof(Size)];
	for (size_t i = 0; i < sizeof(Size); i++)
		bytes[i] = *(it++);
	it--;
	std::memcpy(&value, &bytes[0], sizeof(Size));
	return value;
}

static Object reconstructInt(vByte::const_iterator& it)
{
	return Object(reconstructBytes<i64>(it));
}

static Object reconstructDec(vByte::const_iterator& it)
{
	return Object(reconstructBytes<double>(it));
}

static Object reconstructString(vByte::const_iterator& it)
{
	std::string str;
	while (static_cast<char>(*it) != '\0')
	{
		str.push_back(static_cast<char>(*it));
		it++;
	}

	HeapObj* obj = new String(str);
	return Object(obj);
}

static Object reconstructHeapObj(vByte::const_iterator& it)
{
	ObjType type = static_cast<ObjType>(*it);
	it++;
	switch (type)
	{
		case HEAP_STRING:	return reconstructString(it);
		default:			return Object(); // Temporarily.
	}
}

vObj reconstructPool(const vByte& poolBytes)
{
	vObj pool;

	for (auto it = poolBytes.begin(); it != poolBytes.end(); it++)
	{
		ObjType type = static_cast<ObjType>(*it);
		switch (type)
		{
			case OBJ_INT:
				pool.push_back(reconstructInt(++it));
				break;
			case OBJ_DEC:
				pool.push_back(reconstructDec(++it));
				break;
			case OBJ_HEAP:
				pool.push_back(reconstructHeapObj(++it));
				break;
			default:
			{
				if ((type != OBJ_BOOL) && (type != OBJ_NULL))
					std::cout << "Error: byte is "
						<< static_cast<int>(*it) << ".\n";
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
		{
			std::cerr << "Reached end of file prematurely.\n";
			exit(65);
		}
		else if (fileIn.fail())
		{
			std::cerr << "Encountered internal I/O error.\n";
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
		std::cerr << "Improper magic flag for bytecode file.\n";
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

		nameLength = static_cast<ui8>(fileIn.get()); // Check for EOF.
		fileName.resize(nameLength);

		fileIn.read(reinterpret_cast<char*>(&codeLength), sizeof(ui64));
		codeBytes.resize(codeLength);

		fileIn.read(reinterpret_cast<char*>(&poolLength), sizeof(ui64));
		poolBytes.resize(poolLength);

		fileIn.read(reinterpret_cast<char*>(fileName.data()), nameLength);
		file = fileName;
		fileIn.read(reinterpret_cast<char*>(codeBytes.data()), codeLength);
		fileIn.read(reinterpret_cast<char*>(poolBytes.data()), poolLength);

		fileIn.close();
		return ByteCode(codeBytes, reconstructPool(poolBytes));
	}

	std::cerr << "File is closed.\n";
	exit(66);
}

void optionLoad(const char* fileName)
{	
	if (!ends_with(fileName, ".bch"))
	{
		std::cerr << "Invalid bytecode file.\n";
		exit(65);
	}

	external = true;

	std::ifstream program(fileName, std::ios::binary);
	if (program.fail())
	{
		std::cerr << "Failed to open file.\n";
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
		std::cerr << "Invalid bytecode file.\n";
		exit(65);
	}

	external = true;

	std::ifstream program(fileName, std::ios::binary);
	if (program.fail())
	{
		std::cerr << "Failed to open file.\n";
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