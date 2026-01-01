#include "../include/error.h"
#include <iostream>

// Error.

Error::Error(std::string message) :
	message(message) {}

// LexError.

LexError::LexError(char c, ui16 line, ui8 position, std::string message) :
	Error(message), errorChar(c), line(line), position(position) {}

void LexError::report()
{
    if (errorChar != '\0')
        std::cerr << "Scan Error at '" << errorChar << "' [" << line << ":"
        << static_cast<int>(position) << "]: ";
    else
        std::cerr << "Scan Error at line end [" << line << "]: ";
    std::cerr << message << '\n';
}

// CompileError.

CompileError::CompileError(const Token& token, std::string message) :
	Error(message), token(token) {}

void CompileError::report()
{
    std::cerr << "Compile Error";
    if (token.type != TOK_EOF)
    {
        std::cerr << " at '" << token.text << "'";
        std::cerr << " [" << token.line << ":"
            << static_cast<int>(token.position) << "]: ";
    }
    else
        std::cerr << " at end: ";
    std::cerr << message << '\n';
}