#include "../include/args.h"
#include "../include/diagnostic.h"
#include "../include/options.h"
#include "../include/utils.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string_view>
#include <unordered_map>

namespace Args
{
    const std::unordered_map<std::string_view, Option> options{
        {"-token", EMIT_TOKENS},		{"-t", EMIT_TOKENS},
        {"-bytecode", EMIT_BYTECODE},	{"-b", EMIT_BYTECODE},
        {"-cache", CACHE_BYTECODE},		{"-c", CACHE_BYTECODE},
        {"-dis", DIS_PROGRAM},			{"-d", DIS_PROGRAM},
        {"-load", LOAD_PROGRAM},		{"-l", LOAD_PROGRAM},
        {"-check", CHECK_PROGRAM},      {"-k", CHECK_PROGRAM},
        {"-explain", EXPLAIN_ERROR},    {"-e", EXPLAIN_ERROR}
    };

    const std::unordered_map<Option, Handler> optionHandlers{
        {EMIT_TOKENS, optionEmitTokens},
        {EMIT_BYTECODE, optionEmitBytecode},
        {CACHE_BYTECODE, optionCacheBytecode},
        {DIS_PROGRAM, optionDisProgram},
        {LOAD_PROGRAM, optionLoadProgram},
        {CHECK_PROGRAM, optionCheckProgram},
        {EXPLAIN_ERROR, optionExplainError}
    };

    const std::unordered_map<std::string_view, DebugInfoState> debugInfoOptions{
        {"-c", DEBUG_COMBINED},	{"-combined", DEBUG_COMBINED},
        {"-s", DEBUG_SEPARATE},	{"-separate", DEBUG_SEPARATE},
        {"-n", DEBUG_STRIPPED},	{"-nodebug", DEBUG_STRIPPED}
    };

    const std::array fileOnlyOptions{
        CACHE_BYTECODE, DIS_PROGRAM, LOAD_PROGRAM, CHECK_PROGRAM
    };

    // Options that potentially handle source files.
    const std::array optionsUsingSourceFiles{
        EXECUTE, EMIT_TOKENS, EMIT_BYTECODE, CACHE_BYTECODE, CHECK_PROGRAM
    };

    void Config::run()
    {
        auto it{std::find(
            optionsUsingSourceFiles.begin(),
            optionsUsingSourceFiles.end(),
            option
        )};
        bool usingSourceFile{it != optionsUsingSourceFiles.end()};

        if (usingSourceFile)
        {
            // Read and store source file.
            std::string code{readFile(arg)};
            normalizeInput(code);

            FileID id{sourceManager.addFile(std::string{arg}, code)};
            handler(id, code);
        }
        else
            handler(FileID{}, arg);
    }

    void Config::run(FileID id, std::string_view input)
    {
        handler(id, input);
    }

    void invalidOption()
    {
        PRINT_ERROR("Invalid command-line option.\n");
        exit(64);
    }

    void validateChoiceFile(std::string_view fileName, bool isCacheFile)
    {
        if (!std::filesystem::exists(fileName))
        {
            PRINT_ERROR("File does not exist.\n");
            exit(66);
        }

        if (!isCacheFile && !ends_with(fileName, CH_FILE_EXT))
        {
            PRINT_ERROR("Invalid Choice source file.\n");
            exit(65);
        }
        else if (isCacheFile && !ends_with(fileName, CH_BYTECODE_EXT))
        {
            PRINT_ERROR("Invalid Choice bytecode file.\n");
            exit(65);
        }
    }

    Config parseArgs(int argc, const char* argv[])
    {
        if (argc == 4)
        {
            auto it{options.find(argv[1])};
            if ((it == options.end()) || (it->second != CACHE_BYTECODE))
                invalidOption();

            auto stateIt{debugInfoOptions.find(argv[2])};
            if (stateIt == debugInfoOptions.end())
                invalidOption();

            debugInfoState = stateIt->second;
            validateChoiceFile(argv[3], false);
            return { RUN_FILE, it->second, optionCacheBytecode, argv[3] };
        }

        else if (argc == 3)
        {
            auto it{options.find(argv[1])};
            if (it == options.end()) invalidOption();

            if (it->second == EXPLAIN_ERROR)
                return { RUN_DIRECT, it->second, optionExplainError, argv[2] };
            else
            {
                auto checkIt{std::find(optionsUsingSourceFiles.begin(),
                    optionsUsingSourceFiles.end(), it->second)};
                bool isCacheFile{checkIt == optionsUsingSourceFiles.end()};
                validateChoiceFile(argv[2], isCacheFile);
                return { RUN_FILE, it->second, optionHandlers.at(it->second), argv[2] };
            }
        }

        else if (argc == 2)
        {
            auto it{options.find(argv[1])};
            if (it != options.end())
            {
                auto checkIt{std::find(fileOnlyOptions.begin(), fileOnlyOptions.end(),
                    it->second)};
                if (checkIt != fileOnlyOptions.end())
                {
                    PRINT_ERROR("Invalid command-line option for REPL mode.\n");
                    exit(64);
                }

                return { RUN_REPL, it->second, optionHandlers.at(it->second) };
            }
            else
            {
                validateChoiceFile(argv[1], false);
                return { RUN_FILE, EXECUTE, optionExecute, argv[1] };
            }
        }

        else if (argc == 1)
            return Config{ RUN_REPL };

        else
        {
            PRINT_ERROR("Too many command-line arguments.\n");
            exit(1);
        }
    }
}