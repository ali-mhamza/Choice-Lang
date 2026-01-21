#include "../include/error.h"
#include "../include/common.h"
#include <cstdio> // For stderr.

// Error.

Error::Error(const std::string& message) :
	message(message) {}

// LexError.

LexError::LexError(char c, ui16 line, ui8 position,
    const std::string& message) :
	Error(message), errorChar(c), line(line), position(position) {}

void LexError::report()
{
    if ((errorChar == 0) || (errorChar == (char) EOF))
        FORMAT_PRINT(stderr, "Scan Error at line end [{}]: {}\n",
            line, message);
    else
        FORMAT_PRINT(stderr, "Scan error at '{:c}' [{}:{}]: {}\n",
            errorChar, line, position, message);
}

// CompileError.

CompileError::CompileError(const Token& token,
    const std::string& message) :
	Error(message), token(token) {}

void CompileError::report()
{
    FORMAT_PRINT(stderr, "Compile Error");
    if (token.type != TOK_EOF)
    {
        FORMAT_PRINT(" at '{}' [{}:{}]: {}\n",
            token.text, token.line, token.position, message);
    }
    else
        FORMAT_PRINT(" at end: {}\n", message);
}

// RuntimeError.

RuntimeError::RuntimeError(const Token& token,
    const std::string& message) :
    Error(message), token(token) {}

void RuntimeError::report()
{
    FORMAT_PRINT(stderr, "Runtime Error");
    if (token.type != TOK_EOF)
    {
        FORMAT_PRINT(" at '{}' [{}:{}]: {}\n",
            token.text, token.line, token.position, message);
    }
    else
        FORMAT_PRINT(" at end: {}\n", message);
}