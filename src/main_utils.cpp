#include "../include/main_utils.h"
#include "../include/bytecode.h"
#include "../include/common.h"
#include "../include/config.h"
#include "../include/disasm.h"
#include "../include/linear_alloc.h"
#include "../include/object.h"
#include "../include/readers.h"
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

static constexpr std::string_view CH_FILE_EXT{".ch"};
static constexpr std::string_view CH_BYTECODE_EXT{".chbc"};

std::string readFile(const char* fileName, bool binary)
{
	std::ifstream file;

	if (binary)
	    file.open(fileName, std::ios::binary);
	else
        file.open(fileName);

	if (file.fail())
	{
		CH_PRINT(stderr, "Failed to open file.\n");
		exit(66);
	}

	if (file.is_open())
	{
		std::stringstream buffer{};
		buffer << file.rdbuf();
		std::string fileString{buffer.str()};
		file.close();
		return fileString;
	}

	CH_PRINT(stderr, "File is closed.\n");
	exit(66);
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

void optionShowTokens(SourceManager* manager, FileID id, const vT& tokens)
{
	TokenPrinter{manager, id, tokens}.printTokens();
}

void optionShowBytes(const ByteCode& chunk)
{
	Disassembler{chunk}.disassembleCode();
}

void optionCacheBytes(const ByteCode& chunk, const char* fileName)
{
	if (chunk.codeSize() == 0)
	{
		CH_PRINT("No code generated -> no cache file generated.\n");
		return;
	}

	std::filesystem::path filePath{fileName};
	filePath.replace_extension(CH_BYTECODE_EXT);
	std::ofstream cacheFile{filePath.filename().c_str(), std::ios::binary};
	chunk.cacheStream(cacheFile);
}

void optionLoad(const char* fileName)
{
	if (!ends_with(fileName, CH_BYTECODE_EXT))
	{
		CH_PRINT(stderr, "Invalid bytecode file.\n");
		exit(65);
	}

	external = true;

	std::ifstream program{fileName, std::ios::binary};
	if (program.fail())
	{
		CH_PRINT(stderr, "Failed to open file.\n");
		exit(66);
	}

	Readers::CodeReader codeReader{program};
	ByteCode chunk{codeReader.readCache()};
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

	external = true;

	std::ifstream program{fileName, std::ios::binary};
	if (program.fail())
	{
		CH_PRINT(stderr, "Failed to open file.\n");
		exit(66);
	}

	Readers::CodeReader codeReader{program};
	ByteCode chunk{codeReader.readCache()};
	Disassembler{chunk}.disassembleCode();
}

bool fileNameCheck(const std::string_view fileName)
{
	return (ends_with(fileName, CH_FILE_EXT) || ends_with(fileName, CH_BYTECODE_EXT));
}