#include "../include/error.h"
#include <iostream>

// Error.

Error::Error(std::string_view message) :
	message(message) {}

// LexError.

LexError::LexError(char c, uint16_t line, uint8_t position, std::string_view message) :
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

CompileError::CompileError(const Token& token, std::string_view message) :
	Error(message), token(token) {}

void CompileError::report()
{
    std::cerr << "Compile Error";
    if (token.type != TOKEN_EOF)
    {
        std::cerr << " at '" << token.text << "'";
        std::cerr << " [" << token.line << ":"
            << static_cast<int>(token.position) << "]: ";
    }
    else
        std::cerr << " at end: ";
    std::cerr << message << '\n';
}