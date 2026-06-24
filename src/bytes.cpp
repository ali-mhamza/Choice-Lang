#include "../include/bytes.h"
#include "../include/astnodes.h"
#include "../include/bytecode.h"
#include "../include/common.h"
#include "../include/config.h"
#include "../include/linear_alloc.h"
#include "../include/object.h"
#include "../include/utils.h"
#include <fmt/base.h>
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
	CH_PRINT_ERROR("Reached end of file prematurely.\n");
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
		CH_PRINT_ERROR("Improper magic flag for bytecode file.\n");
		exit(65);
	}
}

void CodeReader::readVersionNum()
{
	std::array<u8, 3> num{};
	readBytes(num.data(), 3);

	if (num[0] != CH_VERSION_MAJOR)
	{
		CH_PRINT_ERROR(
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

Object CodeReader::reconstructType()
{
   	u8 nameLen{readValue<u8>()};
	std::string name{};
   	name.resize(nameLen);
    readBytes(name.data(), nameLen);

    u8 fieldCount{readValue<u8>()};
    std::vector<std::string> fields(fieldCount);

    for (u8 i{0}; i < fieldCount; i++)
    {
        nameLen = readValue<u8>();
        fields[i].resize(nameLen);
        readBytes(fields[i].data(), nameLen);
    }

    return Object{CH_ALLOC(Type, name, fields)};
}

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

	u8 arityMin{readValue<u8>()};
	u8 arityMax{readValue<u8>()};
	bool variadic{readValue<bool>()};
	ByteCode code{reconstructByteCode()};

	Object func{};
	if (nameLen == 0) // Lambda.
		func = CH_ALLOC(Function, code, arityMin, arityMax);
	else
		func = CH_ALLOC(Function, name, code, arityMin, arityMax);

	u8 defaultCount{static_cast<u8>(arityMax - arityMin)};
	ByteCode* defaultArgs{new ByteCode[defaultCount]};
	for (u8 i{0}; i < defaultCount; i++)
	    defaultArgs[i] = reconstructByteCode();

	AS_USER_FUNC(func)->variadic = variadic;
	AS_USER_FUNC(func)->defaultArgs = defaultArgs;

	return func;
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
			case OBJ_USER_TYPE: pool.emplace_back(reconstructType());           break;
			case OBJ_USER_FUNC:
			case OBJ_LAMBDA:    pool.emplace_back(reconstructFunc());           break;
			case OBJ_STRING:    pool.emplace_back(reconstructString());         break;
			default:
			{
				if ((type != OBJ_BOOL) && (type != OBJ_NULL))
				{
					CH_PRINT_ERROR_ARGS("Error: byte {} is {}.\n", it - cacheBytes.begin(),
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


/* BinaryInspector class. */

using Bytes::BinaryInspector;

BinaryInspector::BinaryInspector(std::ifstream& cacheFile)
{
    std::string cached{readFile(cacheFile)};
    cacheBytes = std::vector<u8>(cached.begin(), cached.end());
	start = cacheBytes.begin();
    it = cacheBytes.begin();
    end = cacheBytes.end();
}

void BinaryInspector::readBytes(void* mem, size_t memSize)
{
    if (it + memSize > end)
		eofError();

    std::memcpy(mem, &it[0], memSize);
    it += memSize;
}

template<typename T>
T BinaryInspector::readValue()
{
    T ret{readMemValue<T>(&it[0], &end[0])};
    it += sizeof(T);
    return ret;
}

void BinaryInspector::printStartEnd(u64 start, u64 end, bool indent)
{
	if (indent)
		CH_PRINT("  (0x{:04X} - 0x{:04X}) ", start, end - 1);
	else
		CH_PRINT("(0x{:04X} - 0x{:04X}) ", start, end - 1);
}

void BinaryInspector::printEntryTitle(sv title, u64 titleLength)
{
	CH_PRINT("{:<{}}  ", title, titleLength);
}

void BinaryInspector::printStringWithTruncation(
	std::string& str,
	u64 displayLen,
	sv truncMsg,
	bool center
)
{
	constexpr u64 maxDisplayLen{30 - sizeof('\'') * 2};

	if (displayLen <= maxDisplayLen)
	{
	    if (center)
			CH_PRINT(" {:^30}\n", CH_QUOTED(str));
	    else
	        CH_PRINT(" {:<30}\n", CH_QUOTED(str));
	}
	else
	{
		constexpr u64 newlineSize{CH_NEWLINE_REPLACEMENT.size()};
		constexpr u64 newlineStart{25 - newlineSize};
		constexpr u64 newlineEnd{25 - 1};

		u64 pos{str.find(CH_NEWLINE_REPLACEMENT)};
		// Check if truncating to 25 characters (30 - the quote marks
		// and ellipsis) will cut into the newline replacement sequence.
		if ((pos != str.npos) && (pos >= newlineStart) && (pos <= newlineEnd))
		{
			str = str.substr(0, pos);
			if (center)
			    CH_PRINT(" {:^30}  ({})\n", "'" + str + "...'", truncMsg);
			else
			    CH_PRINT(" {:<30}  ({})\n", "'" + str + "...'", truncMsg);;
		}
		else
		{
			str = str.substr(0, 25);
			CH_PRINT(" {:.30}  ({})\n", "'" + str + "...'", truncMsg);
		}
	}
}

void BinaryInspector::inspectHeaders()
{
	// Not subtracting 1 to include the ':'.
	constexpr u64 titleLength{sizeof("Version number")};
	u64 start{}, end{};

	// Magic.

	start = getCurrentPosition();
	std::array<char, sizeof("choice") - 1> magic{};
	readBytes(magic.data(), magic.size());
	end = getCurrentPosition();

	printStartEnd(start, end, true);
	printEntryTitle("Magic:", titleLength);
	CH_PRINT("{}\n", sv{magic.data(), magic.size()});

	// Version number.

	start = getCurrentPosition();
	std::array<u8, 3> version{};
	readBytes(version.data(), version.size());
	end = getCurrentPosition();

	printStartEnd(start, end, true);
	printEntryTitle("Version number:", titleLength);
	CH_PRINT("{}.{}.{}\n", version[0], version[1], version[2]);

	// Debug info state.

	start = getCurrentPosition();
	DebugInfoState state{readValue<DebugInfoState>()};
	this->state = state;
	end = getCurrentPosition();

	printStartEnd(start, end, true);
	printEntryTitle("Debug info:", titleLength);
	switch (state)
	{
		case DEBUG_COMBINED:	CH_PRINT("Combined\n");	break;
		case DEBUG_SEPARATE:	CH_PRINT("Separate\n");	break;
		case DEBUG_STRIPPED:	CH_PRINT("Stripped\n");	break;
	}
}

void BinaryInspector::inspectFileName()
{
	constexpr u64 titleLength{sizeof("File name length")};
	u64 start{}, end{};

	// File name length.

	start = getCurrentPosition();
	u8 nameLength{readValue<u8>()};
	end = getCurrentPosition();

	printStartEnd(start, end, true);
	printEntryTitle("File name length:", titleLength);
	CH_PRINT("{}\n", nameLength);

	// File name string.

	std::string fileName{};
	fileName.resize(nameLength);
	start = getCurrentPosition();
	readBytes(fileName.data(), nameLength);
	end = getCurrentPosition();

	printStartEnd(start, end, true);
	printEntryTitle("File name:", titleLength);
	CH_PRINT("{}\n", fileName);
}

void BinaryInspector::inspectLineMarkers()
{
	constexpr u64 titleLength{sizeof("Number of line markers")};
	u64 start{}, end{};

	start = getCurrentPosition();
	u64 numMarkers{readValue<u64>()};
	end = getCurrentPosition();
	printStartEnd(start, end, true);
	printEntryTitle("Number of line markers:", titleLength);
	CH_PRINT("{}\n", numMarkers);

	start = getCurrentPosition();
	std::vector<u64> lineMarkers(numMarkers);
	for (u64 i{0}; i < numMarkers; i++)
		lineMarkers[i] = readValue<u64>();
	end = getCurrentPosition();

	printStartEnd(start, end, true);
	printEntryTitle("Line markers:", titleLength);

	if (numMarkers == 0)
		CH_PRINT("[]\n");
	else
	{
		CH_PRINT("[");

		if (numMarkers <= 5)
		{
			for (u64 i{0}; i < numMarkers; i++)
			{
				CH_PRINT("{}", lineMarkers[i]);
				if (i != numMarkers - 1)
					CH_PRINT(", ");
			}
		}
		else
		{
			for (u64 i{0}; i < 5; i++)
			{
				CH_PRINT("{}", lineMarkers[i]);
				CH_PRINT(", ");
			}
			CH_PRINT("..., ");
			CH_PRINT("{}, {}", lineMarkers[numMarkers - 2], lineMarkers[numMarkers - 1]);
		}

		CH_PRINT("]\n");
	}
}

void BinaryInspector::inspectByteCode()
{
	constexpr u64 titleLength{sizeof("Constant pool size")};
	u64 start{}, end{};

	start = getCurrentPosition();
	u64 codeSize{readValue<u64>()};
	end = getCurrentPosition();
	printStartEnd(start, end, true);
	printEntryTitle("Code segment size:", titleLength);
	CH_PRINT("{}\n", codeSize);

	start = getCurrentPosition();
	u64 poolSize{readValue<u64>()};
	end = getCurrentPosition();
	printStartEnd(start, end, true);
	printEntryTitle("Constant pool size:", titleLength);
	CH_PRINT("{}\n", poolSize);

	vByte code(codeSize);
	start = getCurrentPosition();
	readBytes(code.data(), codeSize);
	end = getCurrentPosition();
	printStartEnd(start, end, true);
	printEntryTitle("Code bytes:", titleLength);
	CH_PRINT("[{}, {}, {}, {}, {}, ...]\n", code[0], code[1], code[2],
		code[3], code[4]);

	CH_PRINT("\nConstant pool:\n");
	inspectConstantPool(poolSize);
}

void BinaryInspector::inspectConstantPool(u64 poolSize)
{
	// Constant table (brief).

	CH_PRINT("  {:^7} ", "Index");
	// Offset pair in () + a space on either side + positioning.
	CH_PRINT("{:^19} ", "Offset");
	// Longest typename + a space on either side.
	CH_PRINT("{:^10} ", "Type");
	// Object size + a space on either side + positioning.
	CH_PRINT("{:^7} ", "Bytes");
	// Enough size to fit (at least a truncated form of) the value.
	CH_PRINT("{:^30}\n", "Value");

	CH_PRINT("  {:-^7} ", "");
	// Offset pair in () + a space on either side + positioning.
	CH_PRINT("{:-^19} ", "");
	// Longest typename + a space on either side.
	CH_PRINT("{:-^10} ", "");
	// Object size + a space on either side + positioning.
	CH_PRINT("{:-^7} ", "");
	// Enough size to fit (at least a truncated form of) the value.
	CH_PRINT("{:-^30}\n", "");

	vBit startIter{it};
	for (u64 i{0}; i < poolSize;)
		inspectBriefObject(i);

	// Constants (individual in detail).

	it = startIter;
	for (u64 i{0}; i < poolSize;)
		inspectDetailObject(i);
}

void BinaryInspector::inspectBriefObject(u64& position)
{
	static u64 index{0};
	u64 start{};

	CH_PRINT("  {:^7} ", CH_STR("[{}]", index++));
	start = getCurrentPosition();
	ObjType type{readValue<ObjType>()};

	switch (type)
	{
		case OBJ_INT:		inspectBriefInt(start);		break;
		case OBJ_DEC:		inspectBriefDec(start);		break;
		case OBJ_STRING:	inspectBriefString(start);	break;
		case OBJ_USER_TYPE: inspectBriefType(start);    break;
		case OBJ_USER_FUNC:
		case OBJ_LAMBDA:	inspectBriefFunc(start);	break;
		default: ;
	}

	position += getCurrentPosition() - start;
}

void BinaryInspector::inspectBriefInt(u64 start)
{
	CH_PRINT(" ");
	i64 value{readValue<i64>()};
	printStartEnd(start, getCurrentPosition(), false);
	CH_PRINT(" {:^10}", "Int");
	CH_PRINT(" {:^7}", getCurrentPosition() - start);
	CH_PRINT(" {:^30}\n", value);
}

void BinaryInspector::inspectBriefDec(u64 start)
{
	CH_PRINT(" ");
	double value{readValue<double>()};
	printStartEnd(start, getCurrentPosition(), false);
	CH_PRINT(" {:^10}", "Dec");
	CH_PRINT(" {:^7}", getCurrentPosition() - start);
	CH_PRINT(" {:^30}\n", value);
}

void BinaryInspector::inspectBriefString(u64 start)
{
	CH_PRINT(" ");
	u64 nameLen{readValue<u64>()};
	std::string str{};

	if (nameLen != 0)
	{
		str.resize(nameLen);
		readBytes(str.data(), nameLen);
	}

	printStartEnd(start, getCurrentPosition(), false);
	CH_PRINT(" {:^10}", "String");
	CH_PRINT(" {:^7}", getCurrentPosition() - start);
	str = formatMultiLineString(str);
	printStringWithTruncation(str, str.size(),
		CH_STR("truncated; length={}", nameLen), true);
}

void BinaryInspector::skipTypeFields(u8 fieldCount)
{
    for (u8 i{0}; i < fieldCount; i++)
        it += readValue<u8>();
}

void BinaryInspector::inspectBriefType(u64 start)
{
    constexpr u64 maxNameDisplayLength{30 - sizeof("name=''") + 1};
	CH_PRINT(" ");

	u8 nameLen{readValue<u8>()};
	std::string name{};
	name.resize(nameLen);
	readBytes(name.data(), nameLen);

	u8 fieldCount{readValue<u8>()};
	for (u8 i{0}; i < fieldCount; i++)
        it += readValue<u8>();

	printStartEnd(start, getCurrentPosition(), false);
	CH_PRINT(" {:^10}", "Type");
	CH_PRINT(" {:^7}", getCurrentPosition() - start);

	if (nameLen <= maxNameDisplayLength)
		CH_PRINT(" {:^30}\n", "name='" + name + "'");
	else
	{
		CH_PRINT(" name='{:.20}...'  ", name);
		CH_PRINT("(truncated; length={})\n", nameLen);
	}
}

void BinaryInspector::skipFuncData()
{
	// Skip arity data.
	u8 arityMin{readValue<u8>()};
	u8 arityMax{readValue<u8>()};

	// Skip variadic flag.
	it++;

	auto skipCode = [this] {
		// Skip code and constant pool.
		u64 codeSize{readValue<u64>()};
		u64 poolSize{readValue<u64>()};
		it += codeSize + poolSize;
		if (it > end) eofError();

		// Skip metadata.
		if (state == DEBUG_COMBINED)
		{
			u64 metadataBlockCount{readValue<u64>()};
			it += metadataBlockCount * sizeof(DebugRange);
			if (it > end) eofError();
		}
	};

	skipCode();
	u8 defaultArgs{static_cast<u8>(arityMax - arityMin)};
	for (u8 i{0}; i < defaultArgs; i++)
		skipCode();
}

void BinaryInspector::inspectBriefFunc(u64 start)
{
	constexpr u64 maxNameDisplayLength{30 - sizeof("name=''") + 1};
	CH_PRINT(" ");
	u8 nameLen{readValue<u8>()};
	std::string name{};

	if (nameLen != 0)
	{
		name.resize(nameLen);
		readBytes(name.data(), nameLen);
	}

	skipFuncData();
	printStartEnd(start, getCurrentPosition(), false);
	CH_PRINT(" {:^10}", (nameLen == 0) ? "Lambda": "Function");
	CH_PRINT(" {:^7}", getCurrentPosition() - start);

	if (nameLen <= maxNameDisplayLength)
		CH_PRINT(" {:^30}\n", "name='" + name + "'");
	else
	{
		CH_PRINT(" name='{:.20}...'  ", name);
		CH_PRINT("(truncated; length={})\n", nameLen);
	}
}

void BinaryInspector::inspectDetailObject(u64& position)
{
	static u64 index{0};
	u64 start{};

	CH_PRINT("\n  Constant [{}]:\n", index++);
	start = getCurrentPosition();
	ObjType type{readValue<ObjType>()};

	switch (type)
	{
		case OBJ_INT:		inspectDetailInt(start);	break;
		case OBJ_DEC:		inspectDetailDec(start);	break;
		case OBJ_STRING:	inspectDetailString(start);	break;
		case OBJ_USER_TYPE: inspectDetailType(start);   break;
		case OBJ_USER_FUNC:
		case OBJ_LAMBDA:	inspectDetailFunc(start);	break;
		default: ;
	}

	position += getCurrentPosition() - start;
}

#define PRINT_ENTRY_RANGE() \
	CH_PRINT("  "); printStartEnd(start, getCurrentPosition(), true);

void BinaryInspector::inspectDetailInt(u64 start)
{
	PRINT_ENTRY_RANGE();
	CH_PRINT("Type: Int\n");

	start = getCurrentPosition();
	i64 value{readValue<i64>()};
	PRINT_ENTRY_RANGE();
	CH_PRINT("Integer value: {}\n", value);
}

void BinaryInspector::inspectDetailDec(u64 start)
{
	PRINT_ENTRY_RANGE();
	CH_PRINT("Type: Dec\n");

	start = getCurrentPosition();
	double value{readValue<double>()};
	PRINT_ENTRY_RANGE();
	CH_PRINT("Floating-point value: {}\n", value);
}

void BinaryInspector::inspectDetailString(u64 start)
{
	PRINT_ENTRY_RANGE();
	CH_PRINT("Type: String\n");

	start = getCurrentPosition();
	u64 nameLen{readValue<u64>()};
	PRINT_ENTRY_RANGE();
	CH_PRINT("String length: {}\n", nameLen);

	start = getCurrentPosition();
	std::string str{};
	if (nameLen != 0)
	{
		str.resize(nameLen);
		readBytes(str.data(), nameLen);
	}

	str = formatMultiLineString(str);
	PRINT_ENTRY_RANGE();
	CH_PRINT("String value:");
	printStringWithTruncation(str, str.size(), "truncated", false);
}

void BinaryInspector::inspectDetailType(u64 start)
{
   	PRINT_ENTRY_RANGE();
	CH_PRINT("Type: Compound Type\n");

	constexpr u64 maxNameDisplayLength{30 - sizeof('\'') * 2};
	start = getCurrentPosition();
	u8 nameLen{readValue<u8>()};
	PRINT_ENTRY_RANGE();
	CH_PRINT("Name length: {}\n", nameLen);

	start = getCurrentPosition();
	std::string name{};
	name.resize(nameLen);
	readBytes(name.data(), nameLen);

	if (nameLen != 0)
	{
		PRINT_ENTRY_RANGE();
		if (nameLen <= maxNameDisplayLength)
		    CH_PRINT("Type name: {:<30}\n", "'" + name + "'");
		else
			CH_PRINT("Type name: '{:.25}...'  (truncated)\n", name);
	}

	start = getCurrentPosition();
	u8 fieldCount{readValue<u8>()};
	PRINT_ENTRY_RANGE();
	CH_PRINT("Field count: {}\n", fieldCount);

	start = getCurrentPosition();
	for (u8 i{0}; i < fieldCount; i++)
        it += readValue<u8>();
	PRINT_ENTRY_RANGE();
	CH_PRINT("Field names: [...]\n");
}

void BinaryInspector::inspectDetailFuncName()
{
	constexpr u64 maxNameDisplayLength{30 - sizeof('\'') * 2};
	u64 start{};

	start = getCurrentPosition();
	u8 nameLen{readValue<u8>()};
	PRINT_ENTRY_RANGE();
	CH_PRINT("Function name length: {}\n", nameLen);

	start = getCurrentPosition();
	std::string name{};
	if (nameLen != 0)
	{
		name.resize(nameLen);
		readBytes(name.data(), nameLen);
	}

	if (nameLen != 0)
	{
		PRINT_ENTRY_RANGE();
		if (nameLen <= maxNameDisplayLength)
		    CH_PRINT("Function name: {:<30}\n", "'" + name + "'");
		else
			CH_PRINT("Function name: '{:.25}...'  (truncated)\n", name);
	}
}

void BinaryInspector::inspectDetailFuncComponents(u8& arityMin, u8& arityMax)
{
	u64 start{};

	start = getCurrentPosition();
	arityMin = readValue<u8>();
	arityMax = readValue<u8>();
	bool variadic{readValue<bool>()};
	PRINT_ENTRY_RANGE();
	CH_PRINT("Arity: min={}, max={}\n", arityMin, (variadic ? CODE_MAX : arityMax));

	start = getCurrentPosition();
	u64 codeSize{readValue<u64>()};
	PRINT_ENTRY_RANGE();
	CH_PRINT("Code size: {}\n", codeSize);

	start = getCurrentPosition();
	u64 poolSize{readValue<u64>()};
	PRINT_ENTRY_RANGE();
	CH_PRINT("Constant pool size: {}\n", poolSize);

	start = getCurrentPosition();
	if ((it += codeSize) > end) eofError();
	PRINT_ENTRY_RANGE();
	CH_PRINT("Code bytes: [...]\n");

	if (poolSize != 0)
	{
		start = getCurrentPosition();
		if ((it += poolSize) > end) eofError();
		PRINT_ENTRY_RANGE();
		CH_PRINT("Constant pool bytes: [...]\n");
	}
}

void BinaryInspector::skipFuncDefaultArgs(u8 defaultArgs)
{
	for (u8 i{0}; i < defaultArgs; i++)
	{
		u64 codeSize{readValue<u64>()};
		u64 poolSize{readValue<u64>()};
		it += codeSize + poolSize;

		if (state == DEBUG_COMBINED)
		{
			u64 metadataBlocks{readValue<u64>()};
			it += metadataBlocks * sizeof(DebugRange);
		}
	}
}

void BinaryInspector::inspectDetailFuncExtras(u8 arityMin, u8 arityMax)
{
	if (state == DEBUG_COMBINED)
	{
		if (it > end) eofError();
		CH_PRINT("  ");
		inspectMetadata();
	}

	u64 start{getCurrentPosition()};
	u8 defaultArgs{static_cast<u8>(arityMax - arityMin)};
	if (defaultArgs == 0) return;

	skipFuncDefaultArgs(defaultArgs);
	PRINT_ENTRY_RANGE();
	CH_PRINT("Default arguments: {} args ({} bytes)\n", defaultArgs,
		getCurrentPosition() - start);
}

void BinaryInspector::inspectDetailFunc(u64 start)
{
	PRINT_ENTRY_RANGE();
	if (it == end) eofError();
	CH_PRINT("Type: {}\n", (*it == 0) ? "Lambda" : "Function");

	inspectDetailFuncName();

	u8 arityMin{}, arityMax{};
	inspectDetailFuncComponents(arityMin, arityMax);
	inspectDetailFuncExtras(arityMin, arityMax);
}

#undef PRINT_ENTRY_RANGE

void BinaryInspector::inspectMetadata()
{
	u64 start{getCurrentPosition()};
	u64 metadataBlocks{readValue<u64>()};
	u64 metadataBytes{metadataBlocks * sizeof(DebugRange)};
	it += metadataBytes;
	printStartEnd(start, getCurrentPosition(), true);
	CH_PRINT("Metadata: {} block{}, {} bytes total ", metadataBlocks,
		(metadataBlocks == 1 ?  "" : "s"), metadataBytes + sizeof(u64));
	CH_PRINT("({} data bytes + 8 size bytes)\n", metadataBytes);
}

void BinaryInspector::inspect()
{
	CH_PRINT("\nChoice Headers:\n");
	inspectHeaders();
	CH_PRINT("\nFile name:\n");
	inspectFileName();

	if (state == DEBUG_COMBINED)
	{
		CH_PRINT("\nDebug line markers:\n");
		inspectLineMarkers();
	}

	CH_PRINT("\nCompiled code:\n");
	inspectByteCode();

	if (state == DEBUG_COMBINED)
	{
		CH_PRINT("\nScript debug metadata:\n");
		inspectMetadata();
	}

	CH_PRINT("\n");
}