#pragma once
#include "bytecode.h"
#include "common.h"
#include "debug.h"
#include "object.h"
#include "options.h"
#include <filesystem>
#include <string_view>
#include <variant>

namespace Args
{
    enum class Option : u8
    {
        // Show the tokens for the given
        // script or REPL input.
        EmitTokens,

        // Compile and show the bytecode for the
        // given script or REPL input.
        EmitBytecode,

        // Compile and store the bytecode for the
        // given script in a file.
        CacheBytecode,

        // Load a bytecode file/program and display
        // the bytecode held in it.
        DisProgram,

        // Load a bytecode file/program and run it.
        LoadProgram,

        // Check that a source compiles successfully.
        // Emits a success message if so, and diagnostics otherwise.
        CheckProgram,

        // Inspect the sections and info in a bytecode file.
        InspectBytecode,

        // Execute testing functions.
        RunTests,

        // Explain a particular error/warning code.
        ExplainError,

        // Entire execution pipeline.
        // Scan, compile, and execute given program
        // or REPL input.
        Execute
    };

    enum class RunOption : u8
    {
        RunFile,
        RunRepl,
        RunDirect
    };

    using Handler = void (*)(FileID, std::string_view);

    struct Config
    {
        RunOption runOption{};
        Option option{Option::Execute};
        Handler handler{optionExecute};
        std::string arg{}; // Currently: file or error code.

        void run();
        void run(FileID id, std::string_view input);
    };

    Config parseArgs(int argc, const char* argv[]);
}