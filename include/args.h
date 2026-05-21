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
    enum Option : u8
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

        // Load a bytecode file/program and display
        // the bytecode held in it.
        DIS_PROGRAM,

        // Load a bytecode file/program and run it.
        LOAD_PROGRAM,

        // Check that a source compiles successfully.
        // Emits a success message if so, and diagnostics otherwise.
        CHECK_PROGRAM,

        // Explain a particular error/warning code.
        EXPLAIN_ERROR,

        // Entire execution pipeline.
        // Scan, compile, and execute given program
        // or REPL input.
        EXECUTE
    };

    enum RunOption : u8
    {
        RUN_FILE,
        RUN_REPL,
        RUN_DIRECT
    };

    using Handler = void (*)(FileID, std::string_view);

    struct Config
    {
        RunOption runOption{};
        Option option{EXECUTE};
        Handler handler{optionExecute};
        std::string arg{}; // Currently: file or error code.

        void run();
        void run(FileID id, std::string_view input);
    };

    Config parseArgs(int argc, const char* argv[]);
}