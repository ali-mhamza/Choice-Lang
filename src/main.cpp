#ifdef COMP_AST
	#include "../include/astcompiler.h"
	#include "../include/parser.h"
#else
	#include "../include/compiler.h"
#endif

#include "../include/disasm.h"
#include "../include/vm.h"
#include "../include/bytecode.h"
#include "../include/common.h"
#include "../include/lexer.h"
#include "../include/main_utils.h"
#include "../include/tokprinter.h"
#include "../include/utils.h"
#include <chrono>
#include <cstdio> // For stderr.
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

#define TIME_TOTAL
// #define TIME_RUN

std::string file = "";
bool external = false;

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
	{"-token", EMIT_TOKENS},
	{"-t", EMIT_TOKENS},

	{"-bytecode", EMIT_BYTECODE},
	{"-b", EMIT_BYTECODE},

	{"-cache", CACHE_BYTECODE},
	{"-c", CACHE_BYTECODE},

	{"-load", LOAD_PROGRAM},
	{"-l", LOAD_PROGRAM},

	{"-dis", DIS_PROGRAM},
	{"-d", DIS_PROGRAM}
};

// Optimization to run a cached bytecode
// file if it is recent enough rather than
// re-compiling.
// Should be updated if we get to multi-file
// compilation.
static bool cacheOptimize(ArgvOption option)
{
	using namespace std::filesystem;
	if (exists(file))
	{
		std::string cached = 
			file.substr(0, file.size() - 3) + ".bch";
		if (exists(cached) && 
			(last_write_time(cached) >= last_write_time(file)))
		{
			if (option == CACHE_BYTECODE)
				return true; // Nothing to do.
			
			std::ifstream code(cached);
			ByteCode chunk = readCache(code);

			if (option == EMIT_BYTECODE)
			{
				Disassembler dis(chunk);
				dis.disassembleCode();
				return true;

			}

			if (option == EXECUTE)
			{
				VM vm;
				vm.executeCode(chunk);
				return true;
			}
		}
	}
	else
	{
		FORMAT_PRINT(stderr, "Failed to open file.\n");
		exit(66);
	}

	return false;
}

static void runFile(const char* fileName, ArgvOption option = EXECUTE)
{
	if (!fileNameCheck(fileName))
	{
		FORMAT_PRINT(stderr, "Invalid Choice file.\n");
		exit(65);
	}
	
	file = std::string(fileName);
	if (option == LOAD_PROGRAM)
	{
		optionLoad(fileName);
		return;
	}

	if (option == DIS_PROGRAM)
	{
		optionDis(fileName);
		return;
	}

	if (cacheOptimize(option))
		return;

	using namespace std::chrono;
	#if defined(TIME_TOTAL) && !defined(TIME_RUN)
		auto begin = steady_clock::now();
	#endif

	std::string code = readFile(fileName);
	#ifdef COMP_AST
		ASTCompiler compiler;
	#else
		Compiler compiler;
	#endif
	VM vm; // Must persist for the entire execution.

	// Performing tokenization outside.
	Lexer lexer;
	vT& tokens = lexer.tokenize(code);
	if (option == EMIT_TOKENS)
	{
		TokenPrinter printer(tokens);
		printer.printTokens();
		return;
	}

	// Perform compilation outside.
	#ifdef COMP_AST
		Parser parser;
		StmtVec& program = parser.parseToAST(tokens);

		#ifdef TYPE
			// Perform type-checking here.
		#endif

		#ifdef OPT
			// Optimize here.
		#endif

		ByteCode& chunk = compiler.compile(program);
	#else
		ByteCode& chunk = compiler.compile(tokens);
	#endif

	if (option == EMIT_BYTECODE)
	{
		Disassembler dis(chunk);
		dis.disassembleCode();
		return;
	}

	if (option == CACHE_BYTECODE)
	{
		optionCacheBytes(chunk, fileName);
		return;
	}

	#if defined(TIME_RUN) && !defined(TIME_TOTAL)
		auto begin = steady_clock::now();
	#endif

	// Execution logic.
	vm.executeCode(chunk);

	#if defined(TIME_RUN) || defined(TIME_TOTAL)
		auto end = steady_clock::now();
		auto time = duration_cast<microseconds>(end - begin);
		FORMAT_PRINT("Time: {:.6f}\n",
			static_cast<long double>(time.count()) / 1000000);
	#endif
}

static void repl(ArgvOption option = EXECUTE)
{
	// All invalid options for REPL mode.
	if (option == CACHE_BYTECODE || option == LOAD_PROGRAM ||
		option == DIS_PROGRAM)
	{
		FORMAT_PRINT(stderr, "Invalid command-line option for REPL mode.\n");
		exit(64);
	}
	
	file = "";

	std::string line;
	#ifdef COMP_AST
		ASTCompiler compiler;
	#else
		Compiler compiler;
	#endif
	VM vm; // Must persist for the entire execution.
	while (true)
	{
		FORMAT_PRINT(">>> ");
		std::getline(std::cin, line);

		while (ends_with(line, "\\"))
		{
			line[line.size() - 1] = '\n';
			std::string temp;
			FORMAT_PRINT("... ");
			std::getline(std::cin, temp);
			line += temp;
		}

		if (!line.empty())
		{
			Lexer lexer;
			vT& tokens = lexer.tokenize(line);
			if (option == EMIT_TOKENS)
			{
				TokenPrinter printer(tokens);
				printer.printTokens();
				continue;
			}

			// Perform compilation outside.
			#ifdef COMP_AST
				Parser parser;
				StmtVec& program = parser.parseToAST(tokens);

				#ifdef TYPE
					// Perform type-checking here.
				#endif

				#ifdef OPT
					// Optimize here.
				#endif

				ByteCode& chunk = compiler.compile(program);
			#else
				ByteCode& chunk = compiler.compile(tokens);
			#endif

			if (option == EMIT_BYTECODE)
			{
				Disassembler dis(chunk);
				dis.disassembleCode();
				continue;
			}

			vm.executeCode(chunk);
		}
		else
			break;
	}
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
			FORMAT_PRINT(stderr, "Invalid command-line option.\n");
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