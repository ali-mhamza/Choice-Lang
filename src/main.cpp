#include "../include/common.h"
#include "../include/lexer.h"
#include "../include/tokprinter.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

enum ArgvOption
{
    EMIT_TOKENS,
    EMIT_BYTECODE,
    EXECUTE
};

static std::unordered_map<const char*, ArgvOption> options = {
    {"-token", EMIT_TOKENS},
    {"-bytecode", EMIT_BYTECODE}
};

static std::string readFile(std::ifstream& file)
{
    if (file.is_open())
    {
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string fileString = buffer.str();
        file.close();
        return fileString;
    }

    std::cerr << "File is closed.\n";
    exit(66);
}

static void runFile(const char* fileName, ArgvOption option = EXECUTE)
{   
    std::ifstream file(fileName);
    if (file.fail())
    {
        std::cerr << "Failed to open file.\n";
        exit(66);
    }

    std::string code = readFile(file);

    // Performing tokenization outside.
    Lexer lexer;
    vT tokens = lexer.tokenize(code);
    if (option == EMIT_TOKENS)
    {
        TokenPrinter printer(tokens);
        printer.printTokens();
        return;
    }

    // Perform compilation outside.
    // ...
    else if (option == EMIT_BYTECODE)
    {
        // ...
    }

    // Execution logic.
}

static void repl()
{
    std::string line;
    while (true)
    {
        std::cout << ">>> ";
        std::getline(std::cin, line);

        if (!line.empty())
        {
            if (dumpTokens)
            {
                Lexer lexer;
                vT tokens = lexer.tokenize(line);

                TokenPrinter printer(tokens);
                printer.printTokens();
            }
        }
        else
            break;
    }
}

int main(int argc, const char* argv[])
{
    if (argc == 3)
        runFile(argv[2], options[argv[1]]);
    else if (argc == 2)
        runFile(argv[1]);
    else
        repl();
    
    return 0;
}