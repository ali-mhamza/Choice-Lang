/*
 * Execution starting point for REPL, file execution, as well
 * as other options.
 */

#include "../include/args.h"
#include "../include/common.h"
#include "../include/debug.h"
#include "../include/diagnostic.h"
#include "../include/gen_alloc.h"
#include "../include/options.h"
#include "../include/utils.h"

// Use replxx library instead of standard
// std::getline.
#define EXTERNAL_REPL 1

#if EXTERNAL_REPL
	#include <replxx/include/replxx.hxx>
#endif

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#if EXTERNAL_REPL
	#define TRACK_REPL_HISTORY	1
	#define SAVE_REPL_HISTORY	1
	#define LOAD_REPL_HISTORY	1
	#define CLEAR_REPL_HISTORY	0
#endif

/* Global variables. */

SourceManager sourceManager{};
DiagnosticEngine diagEngine{};
DebugInfoState debugInfoState{DEBUG_COMBINED};
bool inRepl{false};

#if CH_USE_ALLOC && defined(CH_LINEAR_ALLOC)
	#include "../include/linear_alloc.h"
	LinearAlloc allocator{static_cast<size_t>(CH_ALLOC_SIZE)};
#endif

[[nodiscard]]
static DebugInfoState readCacheFileState(const std::filesystem::path& path)
{
    // Byte offset of the debug info state byte into the cache file.
    // Does not change regardless of whether or not debug info is
    // combined, separate, or removed.
    constexpr u64 debugInfoBytePosition{9};
    std::ifstream cacheFile{openFile(path, true)};

    cacheFile.seekg(debugInfoBytePosition);
    u8 state;
    cacheFile >> state;
    cacheFile.close();
    return static_cast<DebugInfoState>(state);
}

// Optimization to use a cached bytecode file if it
// is recent enough rather than re-compiling.
// Should be updated if we get to multi-file compilation.
[[nodiscard]] static bool cacheOptimize(Args::Config& config)
{
	std::filesystem::path cache{config.arg};
	cache.replace_extension(CH_BYTECODE_EXT);

	if (fileMoreRecent(cache, config.arg))
	{
	    DebugInfoState fileState{readCacheFileState(cache)};
		if ((config.option == Args::CACHE_BYTECODE)
		    && (debugInfoState == fileState))
		{
		    // We only return true here so that `config.run()`
			// is not executed back in readFile (we don't do anything).
		    return true;
		}

		// We don't want the debug restructuring function(s)
		// to go searching for a (possibly missing) debug info file.
		if ((config.option == Args::EMIT_BYTECODE)
		    && (fileState != DEBUG_SEPARATE))
		{
		    config.option = Args::DIS_PROGRAM;
			config.handler = optionDisProgram;
		    config.arg = cache.string();
		}

		// Only combined debug info can guarantee the same level
		// of detail in error reporting as the source file itself.
		if ((config.option == Args::EXECUTE)
		    && (fileState == DEBUG_COMBINED))
		{
		    config.option = Args::LOAD_PROGRAM;
			config.handler = optionLoadProgram;
			config.arg = cache.string();
		}
	}

	return false;
}

static void runFile(Args::Config& config)
{
	inRepl = false;

	if (!cacheOptimize(config))
	    config.run();

	#if defined(DEBUG) && CH_USE_ALLOC
		CH_PRINT("Total memory from allocator: {} bytes\n",
			allocator.allocatedMemory());
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

static void buildLine(std::string& line)
{
	while (ends_with(line, "\\"))
	{
		line.back() = '\n';
		std::string temp{};
		CH_PRINT("... ");
		std::getline(std::cin, temp);
		line += temp;
	}
}

#if EXTERNAL_REPL
	static void handleReplHistory(replxx::Replxx& rx)
	{
		#if LOAD_REPL_HISTORY
			rx.history_load("history.txt");
		#elif CLEAR_REPL_HISTORY
			(void) rx; // Since we don't use it in this case.

			std::ofstream history{"history.txt", std::ios::trunc};
			if (history.is_open()) history.close();
		#endif
	}
#endif

static void repl(Args::Config& config)
{
	inRepl = true;

	#if EXTERNAL_REPL
		replxx::Replxx rx{};
		handleReplHistory(rx);
	#endif

	printReplIntro();
	while (true)
	{
		std::string line{};

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

			normalizeInput(line);
			FileID id{sourceManager.addFile("", line)};
			sourceManager.setFile(id, CH_STR("<repl #{}>", id + 1));
			config.run(id, line);
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
	Args::Config config{Args::parseArgs(argc, argv)};

	switch (config.runOption)
	{
		case Args::RUN_FILE:	runFile(config);	break;
		case Args::RUN_REPL:	repl(config);		break;
		case Args::RUN_DIRECT:	config.run();		break;
	}

	return 0;
}