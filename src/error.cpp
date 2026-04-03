#include "../include/error.h"
#include "../include/common.h"  // For CH_PRINT macro, fixed-size integer types.
#include "../include/token.h"
#include <cstdio>               // For stderr.
#include <string>
#include <string_view>

// LexError.

LexError::LexError(char c, ui16 line, ui8 position,
    std::string_view message) :
	errorChar{c}, line{line}, position{position},
    message{message} {}

void LexError::report() const
{
    if (errorChar == (char) EOF)
    {
        CH_PRINT(stderr, "Scan error at line end [{}]: {}\n",
            line, message);
    }
    else
    {
        CH_PRINT(stderr, "Scan error at '{:c}' [{}:{}]: {}\n",
            errorChar, line, position, message);
    }
}

// CompileError.

CompileError::CompileError(const Token& token,
    const std::string& message) :
	token{token}, message{message} {}

void CompileError::report() const
{
    CH_PRINT(stderr, "Compile error at ");
    if (token.type != TOK_EOF)
    {
        CH_PRINT(stderr, "'{}' [{}:{}]: {}\n",
            token.text, token.line, token.position, message);
    }
    else
        CH_PRINT(stderr, "end: {}\n", message);
}

// RuntimeError.

RuntimeError::RuntimeError(const Token& token,
    const std::string& message) :
    token{token}, message{message} {}

void RuntimeError::report() const
{
    CH_PRINT(stderr, "Runtime error at ");
    if (token.type != TOK_EOF)
    {
        CH_PRINT(stderr, "'{}' [{}:{}]: {}\n",
            token.text, token.line, token.position, message);
    }
    else
        CH_PRINT(stderr, "end: {}\n", message);
}