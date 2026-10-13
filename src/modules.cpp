#include "../include/modules.h"
#include "../include/common.h"
#include "../include/compiler.h"
#include "../include/config.h"
#include "../include/diagnostic.h"
#include "../include/error.h"
#include "../include/lexer.h"
#include "../include/object.h"
#include "../include/optimizer.h"
#include "../include/parser.h"
#include "../include/type_checker.h"
#include "../include/utils.h"
#include "../include/vm.h"
#include <filesystem>
#include <string>
#include <utility>

std::pair<bool, ModuleTable>
getModuleTable(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path))
        throw RuntimeError(MODULE_FILE_MISSING);

    std::string content{readFile(path)};
    normalizeInput(content);
    FileID id{sourceManager.addFile(path.string(), content)};

    Lexer lexer{};
    Parser parser{};

    Compiler compiler{};
    compiler.inModule = true;

    VM vm{};
    vm.globalVM = false;

    bool repl{inRepl};
    // To disable any REPL-specific behavior in the interpreter,
    // even if we are importing inside the REPL.
    inRepl = false;

    const auto& tokens{lexer.tokenize(id, content)};
    auto& program{parser.parseToAST(id, tokens)};

    // We don't type-check modules when actually importing them
    // (at runtime).
    // This transfers some compile-time error checks (in the form
    // of type-checking) to runtime, which shouldn't happen.
    // Module type-checking should either happen with global-scope
    // code compilation (whether while compiling a script or REPL
    // input) or not at all.
    // This distinction doesn't have much of an observable effect in
    // the REPL since compilation errors are interspersed with code
    // execution, but it is evident when running scripts since we would
    // end up getting type errors after the script itself has
    // successfully compiled (without any of its own type errors).
    // This wouldn't only be confusing to users (why are we getting new,
    // unmentioned compilation errors after the script has already
    // compiled, possibly during execution at that?), but would also
    // clutter visible output with type errors and arbitrarily abort
    // otherwise well-composed programs.

	#if CH_OPTIMIZATIONS_ON
		Optimizer::optimize(program);
	#endif

    Function* script{compiler.compile(id, program)};
    vm.execute(script);

    inRepl = repl;

    ModuleTable table{};
    if (diagEngine.hasReports())
    {
        diagEngine.emitReports();
        return std::make_pair(false, table);
    }
    else
    {
        const auto& [symbolTable, declTable] = compiler.getSymbolTable();
        const auto* registers{vm.getRegisters()};

        for (const auto& [entry, reg] : symbolTable)
        {
            if (reg < BUILTIN_GLOBALS) continue;
            if (isPrivate(declTable[reg].attr)) continue;
            table.add(entry.name, registers[reg]);
        }
    }

    CH_DEALLOC(script);
    return std::make_pair(true, table);
}