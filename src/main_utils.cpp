#include "../include/main_utils.h"
#include "../include/bytecode.h"
#include "../include/common.h"
#include "../include/config.h"
#include "../include/diagnostic.h"
#include "../include/disasm.h"
#include "../include/linear_alloc.h"
#include "../include/object.h"
#include "../include/bytes.h"
#include "../include/tokprinter.h"
#include "../include/utils.h"
#include "../include/vm.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

/* General helpers. */

static std::ifstream openFile(
	const std::filesystem::path& filePath,
	bool binary = false,
	const std::string_view message = "Failed to open file."
)
{
	std::ifstream fileStream{};

	if (binary)
	    fileStream.open(filePath, std::ios::binary);
	else
        fileStream.open(filePath);

	if (fileStream.fail() || !fileStream.is_open())
	{
		CH_PRINT(stderr, "{}\n", message);
		exit(66);
	}

	return fileStream;
}

std::string readFile(std::ifstream& stream)
{
    std::stringstream buffer{};
	buffer << stream.rdbuf();
	std::string fileString{buffer.str()};
	stream.close();
	return fileString;
}

std::string readFile(const std::filesystem::path& filePath, bool binary)
{
	std::ifstream fileStream{openFile(filePath, binary)};
	return readFile(fileStream);
}

bool fileMoreRecent(
    const std::filesystem::path& a,
    const std::filesystem::path& b
)
{
	using std::filesystem::exists;
	using std::filesystem::last_write_time;

	return (exists(a) && exists(b)
			&& (last_write_time(a) >= last_write_time(b)));
}

void normalizeInput(std::string& input)
{
	input.erase(std::remove_if(input.begin(), input.end(),
	[](char c) -> bool {
		return (isspace(c) && (c != ' ')
				&& (c != '\n') && (c != '\t'));
    }), input.end());

    auto it{input.find('\t')};
    while (it != input.npos)
    {
		// Normalize all tabs with spaces.
        input.replace(it, 1, std::string(TAB_SIZE, ' '));
        it = input.find('\t', it + 1);
    }
}

/* Special option handlers. */

void optionShowTokens(FileID id, const vT& tokens)
{
	TokenPrinter{id, tokens}.printTokens();
}

void optionShowBytes(const ByteCode& chunk)
{
	Disassembler{chunk}.disassembleCode();
}

void optionCacheBytes(FileID id, const ByteCode& chunk)
{
	if (chunk.codeSize() == 0)
	{
		CH_PRINT("No code generated -> no cache file generated.\n");
		return;
	}

	std::filesystem::path filePath{sourceManager.getFile(id)};
	filePath.replace_extension(CH_BYTECODE_EXT);
	std::ofstream cacheFile{filePath.filename().c_str(), std::ios::binary};

	const auto& lineMarkers{sourceManager.getLineMarkers(id)};

	chunk.encodeHeaders(cacheFile);
	Bytes::encodeValue(cacheFile, static_cast<u64>(lineMarkers.size()));
	for (const auto& marker : lineMarkers)
		Bytes::encodeValue(cacheFile, marker);
	chunk.encodeData(cacheFile);
	chunk.encodeMetadata(cacheFile);
}

// Read headers and file name.
// Read debug info state field.
// If bytecode information is stripped, ignore following steps.

// Check if bytecode and debug info are combined.
// - If they are, read only the line markers and let
// the code reader parse the rest of the debug info.
// - If not, read all the debug data first, organize it,
// then pass it to the code reader to use when parsing
// the bytecode objects.

// For both, if the source file is available, read it
// then store it in the source manager (let it compute
// the line markers manually).
// If it isn't available, store the file name and line markers
// in the source manager directly (keep file content empty).

// Only read/load the source file if it is not more recent
// than the bytecode file. Otherwise, report an error/warning(?)
// and ignore it.

[[nodiscard]]
static std::pair<ByteCode, FileID> readByteCode(const char* fileName)
{
	std::ifstream program{openFile(fileName, true)};
	std::filesystem::path debugFile{fileName};
	debugFile.replace_extension(CH_DEBUG_EXT);

	Bytes::CodeReader codeReader{program};
	codeReader.readHeaders();
	auto infoState{codeReader.readDebugState()};
	auto originalFile{codeReader.readFileName()};

	FileID id{sourceManager.addFile(originalFile)};
	codeReader.setFileID(id); // Before any code is read.

	if (infoState != DEBUG_STRIPPED)
	{
		std::vector<u64> lineMarkers{};
		std::vector<DebugMetadata> metadataBlocks{};
		bool usingSource{false};

		std::filesystem::path checkPath{};
		if (infoState == DEBUG_COMBINED)
			checkPath = fileName;
		else
			checkPath = debugFile;

		if (fileMoreRecent(checkPath, originalFile))
		{
			std::string fileContent{readFile(originalFile)};
			normalizeInput(fileContent);
			sourceManager.setContent(id, fileContent);
			usingSource = true;
		}

		if (infoState == DEBUG_SEPARATE)
		{
			std::ifstream debugStream{openFile(debugFile, true,
				"Failed to access debug information.")};
			Bytes::DebugReader debugReader{debugStream};
			lineMarkers = debugReader.readLineMarkers();
			metadataBlocks = debugReader.readMetadata();
		}
		else /* if (infoState == DEBUG_COMBINED) */
			lineMarkers = codeReader.readLineMarkers();

		if (!usingSource)
			sourceManager.setLineMarkers(id, lineMarkers);
		return std::make_pair(codeReader.readCache(metadataBlocks), id);
	}

	return std::make_pair(codeReader.readCache(), id);
}

void optionLoad(const char* fileName)
{
	if (!ends_with(fileName, CH_BYTECODE_EXT))
	{
		CH_PRINT(stderr, "Invalid bytecode file.\n");
		exit(65);
	}

	auto [chunk, id] = readByteCode(fileName);
	(void) id;
	Function* script{CH_ALLOC(Function, chunk, 0)};
	VM{}.executeCode(script);

	#if !CH_USE_ALLOC
		delete script;
	#endif
}

void optionDis(const char* fileName)
{
	if (!ends_with(fileName, CH_BYTECODE_EXT))
	{
		CH_PRINT(stderr, "Invalid bytecode file.\n");
		exit(65);
	}

	auto [chunk, id] = readByteCode(fileName);
	(void) id;
	Disassembler{chunk}.disassembleCode();
}

/* File extension check. */

bool fileNameCheck(const std::string_view fileName)
{
	return (ends_with(fileName, CH_FILE_EXT) || ends_with(fileName, CH_BYTECODE_EXT));
}