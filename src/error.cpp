#include "../include/error.h"
#include "../include/common.h"
#include <cstdio> // For stderr.

// LexError.

LexError::LexError(char c, ui16 line, ui8 position,
    std::string_view message) :
	errorChar(c), line(line), position(position),
    message(message) {}

void LexError::report() const
{
    if (errorChar == (char) EOF)
        FORMAT_PRINT(stderr, "Scan error at line end [{}]: {}\n",
            line, message);
    else
        FORMAT_PRINT(stderr, "Scan error at '{:c}' [{}:{}]: {}\n",
            errorChar, line, position, message);
}

// CompileError.

CompileError::CompileError(const Token& token,
    const std::string& message) :
	token(token), message(message) {}

void CompileError::report() const
{
    FORMAT_PRINT(stderr, "Compile error");
    if (token.type != TOK_EOF)
    {
        FORMAT_PRINT(stderr, " at '{}' [{}:{}]: {}\n",
            token.text, token.line, token.position, message);
    }
    else
        FORMAT_PRINT(stderr, " at end: {}\n", message);
}

// RuntimeError.

RuntimeError::RuntimeError(const Token& token,
    const std::string& message) :
    token(token), message(message) {}

void RuntimeError::report() const
{
    FORMAT_PRINT(stderr, "Runtime error");
    if (token.type != TOK_EOF)
    {
        FORMAT_PRINT(stderr, " at '{}' [{}:{}]: {}\n",
            token.text, token.line, token.position, message);
    }
    else
        FORMAT_PRINT(stderr, " at end: {}\n", message);
}