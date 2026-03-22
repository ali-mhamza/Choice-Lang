#ifdef COMP_AST
	#include "../include/astcompiler.h"
	#include "../include/parser.h"
#else
	#include "../include/compiler.h"
#endif

#include "../include/bytecode.h"
#include "../include/common.h"
#include "../include/gen_alloc.h"
#include "../include/lexer.h"
#include "../include/main_utils.h"
#include "../include/object.h"
#include "../include/utils.h"
#include "../include/vm.h"

// Use replxx library instead of standard
// std::getline.
#define EXTERNAL_REPL 1

#if EXTERNAL_REPL
	#include "replxx.hxx"
#endif

#include <chrono>
#include <cstdio> // For stderr.
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

#if defined(DEBUG)
	#define TIME_TOTAL	1
	#define TIME_RUN	0
#endif

#if EXTERNAL_REPL
	#define TRACK_REPL_HISTORY	1
	#define SAVE_REPL_HISTORY	1
	#define LOAD_REPL_HISTORY	1
	#define CLEAR_REPL_HISTORY	0
#endif

std::string file = "";
bool external = false;
bool inRepl = false;

#if CH_USE_ALLOC && defined(CH_LINEAR_ALLOC)
	#include "../include/linear_alloc.h"
	LinearAlloc allocator(CH_ALLOC_SIZE);
#endif

enum ArgvOption
{
	// Show the tokens for the given 
	// script or REPL input.
	EMIT_TOKENS,

	// Compile and show the bytecode for the
	// given script or REPL input.
	EMIT_BYTECODE,

	// Compile and store the bytecode for the
	// given script in a file.
	CACHE_BYTECODE,

	// Load a bytecode file/program and run it.
	LOAD_PROGRAM,

	// Load a bytecode file/program and display
	// the bytecode held in it.
	DIS_PROGRAM,

	// Entire execution pipeline.
	// Scan, compile, and execute given program
	// or REPL input.
	EXECUTE
};

static std::unordered_map<std::string_view, ArgvOption> options = {
	{"-token", EMIT_TOKENS},		{"-t", EMIT_TOKENS},
	{"-bytecode", EMIT_BYTECODE},	{"-b", EMIT_BYTECODE},
	{"-cache", CACHE_BYTECODE},		{"-c", CACHE_BYTECODE},
	{"-load", LOAD_PROGRAM},		{"-l", LOAD_PROGRAM},
	{"-dis", DIS_PROGRAM},			{"-d", DIS_PROGRAM}
};

static inline vT& runLexer(std::string_view source)
{
	static Lexer lexer;
	return lexer.tokenize(source);
}

static Function* runCompiler(const vT& tokens)
{
	#ifdef COMP_AST
		static Parser parser;
		static ASTCompiler compiler;
		StmtVec& program = parser.parseToAST(tokens);

		#ifdef TYPE
			// Perform type-checking here.
		#endif

		#ifdef OPT
			// Optimize here.
		#endif

		// To not report too many errors when using an
		// AST.
		compiler.errorCount = parser.errorCount;
		return compiler.compile(program);
	#else
		// Fix return value.
		static Compiler compiler;
		return compiler.compile(tokens);
	#endif
}

// Optimization to run a cached bytecode
// file if it is recent enough rather than
// re-compiling.
// Should be updated if we get to multi-file
// compilation.
static bool cacheOptimize(ArgvOption option)
{
	using std::filesystem::exists;
	using std::filesystem::last_write_time;

	std::filesystem::path cache(file);
	cache.replace_extension(".bch");

	if (exists(file))
	{
		if (exists(cache) &&
			(last_write_time(cache) >= last_write_time(file)))
		{
			if (option == CACHE_BYTECODE)
				return true; // Nothing to do.
			
			std::ifstream code(cache);
			ByteCode chunk = readCache(code);

			if (option == EMIT_BYTECODE)
			{
				optionShowBytes(chunk);
				return true;
			}

			if (option == EXECUTE)
			{
				VM vm;
				Function* script = CH_ALLOC(Function, chunk, 0);
				vm.executeCode(script);

				#if !CH_USE_ALLOC
					delete script;
				#endif

				return true;
			}
		}
	}
	else
	{
		CH_PRINT(stderr, "Failed to open file.\n");
		exit(66);
	}

	return false;
}

static bool prelimChecks(const char* fileName, ArgvOption option)
{
	if (!fileNameCheck(fileName))
	{
		CH_PRINT(stderr, "Invalid Choice file.\n");
		exit(65);
	}

	file = std::string(fileName);
	if (option == LOAD_PROGRAM)
	{
		optionLoad(fileName);
		return true;
	}

	if (option == DIS_PROGRAM)
	{
		optionDis(fileName);
		return true;
	}

	if (cacheOptimize(option))
		return true;

	return false;
}

static void runFile(const char* fileName, ArgvOption option = EXECUTE)
{
	inRepl = false;

	if (prelimChecks(fileName, option))
		return;

	using namespace std::chrono;
	#if TIME_TOTAL && !TIME_RUN
		auto begin = steady_clock::now();
	#endif

	std::string code = readFile(fileName);
	vT& tokens = runLexer(code);
	if (option == EMIT_TOKENS)
	{
		optionShowTokens(tokens);
		return;
	}

	Function* script = runCompiler(tokens);

	if (option == EMIT_BYTECODE)
	{
		optionShowBytes(script->code);
		return;
	}
	if (option == CACHE_BYTECODE)
	{
		optionCacheBytes(script->code, fileName);
		return;
	}

	#if TIME_RUN && !TIME_TOTAL
		auto begin = steady_clock::now();
	#endif

	VM vm; // Must persist for the entire execution.
	vm.executeCode(script);

	#if TIME_RUN || TIME_TOTAL
		auto end = steady_clock::now();
		auto time = duration_cast<microseconds>(end - begin);
		CH_PRINT("Time: {:.6f}\n",
			static_cast<long double>(time.count()) / 1000000);
	#endif

	#if !CH_USE_ALLOC
		delete script;
	#endif

}

static void printReplIntro()
{
	#ifndef CH_COMMIT_TIME_STAMP
		#define CH_COMMIT_TIME_STAMP "last modification time not available"
	#endif

	CH_PRINT("Choice {}.{}.{} ", CH_VERSION_MAJOR, CH_VERSION_MINOR,
		CH_VERSION_PATCH);
	CH_PRINT("({}).\n", CH_COMMIT_TIME_STAMP);
	CH_PRINT("Built on: [{}][{}].\n", CH_COMPILER, CH_LOCAL_OS);
}

static std::string& buildLine(std::string& line)
{
	while (ends_with(line, "\\"))
	{
		line[line.size() - 1] = '\n';
		std::string temp;
		CH_PRINT("... ");
		std::getline(std::cin, temp);
		line += temp;
	}

	return line;
}

static void repl(ArgvOption option = EXECUTE)
{	
	inRepl = true;

	// All invalid options for REPL mode.
	if (option == CACHE_BYTECODE || option == LOAD_PROGRAM ||
		option == DIS_PROGRAM)
	{
		CH_PRINT(stderr, "Invalid command-line option for REPL mode.\n");
		exit(64);
	}
	
	file = "";
	std::string line;

	#if EXTERNAL_REPL
		replxx::Replxx rx;
	#endif

	#if LOAD_REPL_HISTORY
		rx.history_load("history.txt");
	#elif CLEAR_REPL_HISTORY
		std::ofstream history("history.txt", std::ios::trunc);
		if (history.is_open()) history.close();
	#endif

	VM vm; // Must persist for the entire execution.

	printReplIntro();
	while (true)
	{
		#if EXTERNAL_REPL
			line = rx.input(">>> ");
		#else
			CH_PRINT(">>> ");
			std::getline(std::cin, line);
		#endif
		buildLine(line);

		if (!line.empty())
		{
			#if TRACK_REPL_HISTORY
				rx.history_add(line);
			#endif

			vT& tokens = runLexer(line);
			if (option == EMIT_TOKENS)
				optionShowTokens(tokens);
			else
			{
				Function* script = runCompiler(tokens);
				if (option == EMIT_BYTECODE)
					optionShowBytes(script->code);
				else
					vm.executeCode(script);

				#if !CH_USE_ALLOC
					delete script;
				#endif

			}
		}
		else
			break;
	}

	#if SAVE_REPL_HISTORY
		rx.history_save("history.txt");
	#endif
}

int main(int argc, const char* argv[])
{
	if (argc == 3)
	{
		auto it = options.find(argv[1]);
		if (it != options.end())
			runFile(argv[2], it->second);
		else
		{
			CH_PRINT(stderr, "Invalid command-line option.\n");
			exit(64);
		}
	}
	else if (argc == 2)
	{
		auto it = options.find(argv[1]);
		if (it != options.end())
			repl(it->second);
		else
			runFile(argv[1]);
	}
	else
		repl();

	return 0;
}