#include "../include/modules.h"
#include "../include/common.h"
#include "../include/compiler.h"
#include "../include/config.h"
#include "../include/diagnostic.h"
#include "../include/error.h"
#include "../include/lexer.h"
#include "../include/object.h"
#include "../include/parser.h"
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

    bool repl{inRepl};
    // To disable any REPL-specific behavior in the interpreter,
    // even if we are importing inside the REPL.
    inRepl = false;

    const auto& tokens{lexer.tokenize(id, content)};
    const auto& program{parser.parseToAST(id, tokens)};
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
            if (isPrivate(declTable.get(reg)->attr)) continue;
            table.add(entry.name, registers[reg]);
        }
    }

    CH_DEALLOC(script);
    return std::make_pair(true, table);
}