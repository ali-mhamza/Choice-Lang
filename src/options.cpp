/*
 * All option handlers for interpreter options, with additional
 * helpers for common functionalities.
 */

#include "../include/options.h"
#include "../include/astnodes.h"
#include "../include/bytecode.h"
#include "../include/bytes.h"
#include "../include/common.h"
#include "../include/compiler.h"
#include "../include/debug.h"
#include "../include/disasm.h"
#include "../include/tokprinter.h"
#include "../include/lexer.h"
#include "../include/linear_alloc.h"
#include "../include/object.h"
#include "../include/optimizer.h"
#include "../include/parser.h"
#include "../include/silencer.h"
#include "../include/utils.h"
#include "../include/vm.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <vector>

#define REPORT_ERRORS()					\
	do {								\
		if (diagEngine.hasReports())	\
			diagEngine.emitReports();	\
	} while (false)

[[nodiscard]]
static inline vT& runLexer(FileID id, const std::string_view source)
{
	static Lexer lexer{};
	return lexer.tokenize(id, source);
}

[[nodiscard]]
static Function* runCompiler(FileID id, const vT& tokens)
{
	static Parser parser{};
	static Compiler compiler{};
	StmtVec& program{parser.parseToAST(id, tokens)};

	#ifdef TYPE
		// Perform type-checking here.
	#endif

	#if CH_OPTIMIZATIONS_ON
		Optimizer::optimize(program);
	#endif

	// To stop after compilation if either hit an error.
	compiler.hitError = parser.hitError;
	return compiler.compile(id, program);
}

static void runVM(Function* script)
{
    static VM vm{};
    vm.execute(script);
}

void optionExecute(FileID id, std::string_view input)
{
	vT& tokens{runLexer(id, input)};
	Function* script{runCompiler(id, tokens)};
	REPORT_ERRORS();
	runVM(script);
	CH_DEALLOC(script);
}

void optionEmitTokens(FileID id, std::string_view input)
{
    vT& tokens{runLexer(id, input)};
    REPORT_ERRORS();
    TokenPrinter{id, tokens}.printTokens();
}

void optionEmitBytecode(FileID id, std::string_view input)
{
    vT& tokens{runLexer(id, input)};
    Function* script{runCompiler(id, tokens)};
    REPORT_ERRORS();
    Disassembler{script}.disassembleCode();
    CH_DEALLOC(script);
}

void optionCacheBytecode(FileID id, std::string_view input)
{
	vT& tokens{runLexer(id, input)};
    Function* script{runCompiler(id, tokens)};

    if (script->code.codeSize() == 0)
	{
		auto stream{diagEngine.hasReports() ? stdout : stderr};
		REPORT_ERRORS();
		CH_PRINT(stream, "No code generated -> no cache file generated.\n");
	}
	else
	{
		std::filesystem::path filePath{sourceManager.getFile(id)};
		std::ofstream cacheFile{filePath.stem().concat(CH_BYTECODE_EXT),
			std::ios::binary};

		std::ofstream debugFile{};
		if (debugInfoState == DebugInfoState::Separate)
			debugFile.open(filePath.stem().concat(CH_DEBUG_EXT), std::ios::binary);

		std::ofstream& debugDestination{
			(debugInfoState == DebugInfoState::Combined) ? cacheFile : debugFile
		};
		const auto& lineMarkers{sourceManager.getLineMarkers(id)};

		script->code.encodeHeaders(cacheFile);
		if (debugInfoState != DebugInfoState::Stripped)
		{
			Bytes::encodeValue(debugDestination, static_cast<u64>(lineMarkers.size()));
			for (const auto& marker : lineMarkers)
				Bytes::encodeValue(debugDestination, marker);
		}

		script->code.encodeData(cacheFile);
		if (debugInfoState != DebugInfoState::Stripped)
			script->code.encodeMetadata(debugDestination);
	}

    CH_DEALLOC(script);
}

static bool loadRecentSourceFile(
	FileID id,
	const std::filesystem::path& debugInfoLocation,
	const std::filesystem::path& originalFile
)
{
	using std::filesystem::exists;

	if (fileMoreRecent(debugInfoLocation, originalFile))
	{
		std::string fileContent{readFile(originalFile)};
		normalizeInput(fileContent);
		sourceManager.setContent(id, fileContent);
		return true;
	}

	if (exists(originalFile))
	{
		CH_PRINT_WARNING(
			"Warning: original source file more recent than bytecode file.\n"
			"Diagnostic reporting may be limited accordingly.\n\n"
		);
	}
	else
	{
		CH_PRINT_WARNING(
			"Warning: original source file not found.\n"
			"Diagnostic reporting may be limited accordingly.\n\n"
		);
	}

	return false;
}

[[nodiscard]]
static ByteCode readByteCode(const std::filesystem::path& file)
{
	std::ifstream program{openFile(file, true)};
	std::filesystem::path debugFile{file.stem().concat(CH_DEBUG_EXT)};

	Bytes::CodeReader codeReader{program};
	codeReader.readHeaders();
	debugInfoState = codeReader.readDebugState();
	auto originalFile{codeReader.readFileName()};

	FileID id{sourceManager.addFile(originalFile)};
	codeReader.setFileID(id); // Before any code is read.

	if (debugInfoState != DebugInfoState::Stripped)
	{
		std::vector<u64> lineMarkers{};
		std::vector<DebugMetadata> metadataBlocks{};

		std::filesystem::path checkPath{};
		if (debugInfoState == DebugInfoState::Combined)
			checkPath = file;
		else
			checkPath = debugFile;

		if (debugInfoState == DebugInfoState::Separate)
		{
			std::ifstream debugStream{openFile(debugFile, true,
				"Failed to access debug information.")};
			Bytes::DebugReader debugReader{debugStream};
			lineMarkers = debugReader.readLineMarkers();
			metadataBlocks = debugReader.readMetadata();
		}
		else /* if (infoState == DEBUG_COMBINED) */
			lineMarkers = codeReader.readLineMarkers();

		// Failed to use existing source file (or its data).
		if (!loadRecentSourceFile(id, checkPath, originalFile))
			sourceManager.setLineMarkers(id, lineMarkers);
		return codeReader.readCache(metadataBlocks);
	}

	return codeReader.readCache();
}

void optionDisProgram(FileID id, std::string_view input)
{
	(void) id;

	ByteCode chunk{readByteCode(input)};
	Function* script{CH_ALLOC(Function, chunk)};
	Disassembler{script}.disassembleCode();
	CH_DEALLOC(script);
}

void optionLoadProgram(FileID id, std::string_view input)
{
	(void) id;

	ByteCode chunk{readByteCode(input)};
	Function* script{CH_ALLOC(Function, chunk)};
	runVM(script); // Reports errors through diagEngine directly.
	CH_DEALLOC(script);
}

void optionCheckProgram(FileID id, std::string_view input)
{
	vT& tokens{runLexer(id, input)};
	// We don't need the bytecode, but we must capture
	// the returned pointer to free its memory (if needed).
	Function* script{runCompiler(id, tokens)};
	(void) script; // In case it is not used again.

	if (diagEngine.hasReports())
		diagEngine.emitReports();
	else
	{
		const auto& sourceFile{sourceManager.getFile(id)};
		CH_PRINT_SUCCESS_ARGS("File '{}' compiles successfully.\n", sourceFile);
	}

	CH_DEALLOC(script);
}

void optionInspectBytecode(FileID id, std::string_view input)
{
	(void) id;

	std::ifstream cacheFile{openFile(input, true)};
	Bytes::BinaryInspector{cacheFile}.inspect();
}

static StmtUP generateFakeCall(const Token& func)
{
	using namespace AST::Statement;
	using namespace AST::Expression;

	ExprUP var{std::make_unique<VarExpr>(func)};
	ExprVec args{};
	ExprUP call{std::make_unique<CallExpr>(var, args, false)};
	return std::make_unique<ExprStmt>(call);
}

static void compileTests(
	FileID id,
	StmtVec& program,
	std::vector<std::pair<std::string_view, Function*>>& tests
)
{
	using namespace AST::Statement;

	for (auto& node : program)
	{
		if (node == nullptr) continue;
		if (node->type == StmtType::FuncDecl)
		{
			FuncDecl* decl{static_cast<FuncDecl*>(node.get())};
			if (!isTest(decl->attr)) continue;
			if (decl->params.size() != 0)
			{
				diagEngine.recordError(id, TEST_FUNC_HAS_PARAMS,
					decl->params[0].param, "");
				break;
			}

			Compiler compiler{};
			StmtVec temp{};
			temp.push_back(std::move(node));
			temp.push_back(generateFakeCall(decl->name));
			tests.push_back({ decl->name.text, compiler.compile(id, temp) });
		}
	}
}

static void runTests(
	FileID id,
	std::vector<std::pair<std::string_view, Function*>>& tests
)
{
	std::filesystem::path filePath{sourceManager.getFile(id)};
	std::string fileName{filePath.stem().string()};

	if (diagEngine.hasReports())
		diagEngine.emitReports();
	else
	{
		for (auto [name, test] : tests)
		{
			VM vm{};
			Silencer silencer{};
			silencer.silence();
			vm.execute(test);
			silencer.restore();

			CH_PRINT("test: {}::{} - ", fileName, name);
			if (diagEngine.hasReports())
				CH_PRINT("{}FAIL{}\n", RED, NORMAL);
			else
				CH_PRINT("{}PASS{}\n", GREEN, NORMAL);
			fflush(stdout);
			diagEngine.clearReports();
		}
	}
}

void optionRunTests(FileID id, std::string_view input)
{
	vT& tokens{runLexer(id, input)};
	Parser parser{};
	StmtVec& program{parser.parseToAST(id, tokens)};

	std::vector<std::pair<std::string_view, Function*>> tests{};
	compileTests(id, program, tests);

	if (diagEngine.hasReports())
		diagEngine.emitReports();
	else
		runTests(id, tests);

	for (auto [_, test] : tests)
	{
		(void) test;
		CH_DEALLOC(test);
	}
}

void optionExplainError(FileID id, std::string_view input)
{
    (void) id;
    diagEngine.explain(input);
}