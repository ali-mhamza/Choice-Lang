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
#include <string_view>
#include <utility>

std::pair<bool, ModuleTable>
getModuleTable(std::string_view file, std::string_view dir)
{
    std::filesystem::path scriptPath{dir};
    scriptPath.append(file);
    scriptPath.concat(CH_FILE_EXT);

    if (!std::filesystem::exists(scriptPath))
        throw RuntimeError(MODULE_FILE_MISSING);

    std::string content{readFile(scriptPath)};
    normalizeInput(content);
    FileID id{sourceManager.addFile(scriptPath.string(), content)};

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
        const auto& symbolTable{compiler.getSymbolTable()};
        const auto* registers{vm.getRegisters()};

        for (const auto& [entry, reg] : symbolTable)
        {
            if (reg < BUILTIN_GLOBALS) continue;
            table.add(entry.name, registers[reg]);
        }
    }

    CH_DEALLOC(script);
    return std::make_pair(true, table);
}