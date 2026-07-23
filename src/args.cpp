/*
 * Main argument parsing, validation, and pipeline driver.
 */

#include "../include/args.h"
#include "../include/common.h"
#include "../include/debug.h"
#include "../include/diagnostic.h"
#include "../include/options.h"
#include "../include/utils.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Args
{
    const std::unordered_map<std::string_view, Option> options{
        {"-token", Option::EmitTokens},         {"-t", Option::EmitTokens},
        {"-bytecode", Option::EmitBytecode},    {"-b", Option::EmitBytecode},
        {"-cache", Option::CacheBytecode},		{"-c", Option::CacheBytecode},
        {"-dis", Option::DisProgram},			{"-d", Option::DisProgram},
        {"-load", Option::LoadProgram},		    {"-l", Option::LoadProgram},
        {"-check", Option::CheckProgram},       {"-k", Option::CheckProgram},
        {"-inspect", Option::InspectBytecode},  {"-i", Option::InspectBytecode},
        {"-explain", Option::ExplainError},     {"-e", Option::ExplainError}
    };

    const std::unordered_map<Option, Handler> optionHandlers{
        {Option::EmitTokens, optionEmitTokens},
        {Option::EmitBytecode, optionEmitBytecode},
        {Option::CacheBytecode, optionCacheBytecode},
        {Option::DisProgram, optionDisProgram},
        {Option::LoadProgram, optionLoadProgram},
        {Option::CheckProgram, optionCheckProgram},
        {Option::InspectBytecode, optionInspectBytecode},
        {Option::ExplainError, optionExplainError}
    };

    const std::unordered_map<std::string_view, DebugInfoState> debugInfoOptions{
        {"-c", DebugInfoState::Combined},   {"-combined", DebugInfoState::Combined},
        {"-s", DebugInfoState::Separate},	{"-separate", DebugInfoState::Separate},
        {"-n", DebugInfoState::Stripped},	{"-nodebug", DebugInfoState::Stripped}
    };

    const std::array fileOnlyOptions{
        Option::CacheBytecode, Option::DisProgram, Option::LoadProgram,
        Option::CheckProgram, Option::InspectBytecode
    };

    // Options that potentially handle source files.
    const std::array optionsUsingSourceFiles{
        Option::Execute, Option::EmitTokens, Option::EmitBytecode,
        Option::CacheBytecode, Option::CheckProgram
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

            FileID id{sourceManager.addFile(arg, code)};
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
        CH_PRINT_ERROR("Invalid command-line option.\n");
        exit(64);
    }

    void validateChoiceFile(std::string_view fileName, bool isCacheFile)
    {
        if (!std::filesystem::exists(fileName))
        {
            CH_PRINT_ERROR("File does not exist.\n");
            exit(66);
        }

        if (!isCacheFile && !ends_with(fileName, CH_FILE_EXT))
        {
            CH_PRINT_ERROR("Invalid Choice source file.\n");
            exit(65);
        }
        else if (isCacheFile && !ends_with(fileName, CH_BYTECODE_EXT))
        {
            CH_PRINT_ERROR("Invalid Choice bytecode file.\n");
            exit(65);
        }
    }

    Config parseArgs(int argc, const char* argv[])
    {
        if (argc == 4)
        {
            auto it{options.find(argv[1])};
            if ((it == options.end()) || (it->second != Option::CacheBytecode))
                invalidOption();

            auto stateIt{debugInfoOptions.find(argv[2])};
            if (stateIt == debugInfoOptions.end())
                invalidOption();

            debugInfoState = stateIt->second;
            validateChoiceFile(argv[3], false);
            return { RunOption::RunFile, it->second, optionCacheBytecode, argv[3] };
        }

        else if (argc == 3)
        {
            auto it{options.find(argv[1])};
            if (it == options.end()) invalidOption();

            if (it->second == Option::ExplainError)
                return { RunOption::RunDirect, it->second, optionExplainError, argv[2] };
            else
            {
                auto checkIt{std::find(optionsUsingSourceFiles.begin(),
                    optionsUsingSourceFiles.end(), it->second)};
                bool isCacheFile{checkIt == optionsUsingSourceFiles.end()};
                validateChoiceFile(argv[2], isCacheFile);
                return { RunOption::RunFile, it->second, optionHandlers.at(it->second),
                    argv[2] };
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
                    CH_PRINT_ERROR("Invalid command-line option for REPL mode.\n");
                    exit(64);
                }

                return { RunOption::RunRepl, it->second, optionHandlers.at(it->second) };
            }
            else
            {
                validateChoiceFile(argv[1], false);
                return { RunOption::RunFile, Option::Execute, optionExecute, argv[1] };
            }
        }

        else if (argc == 1)
            return Config{ RunOption::RunRepl };

        else
        {
            CH_PRINT_ERROR("Too many command-line arguments.\n");
            exit(1);
        }
    }
}