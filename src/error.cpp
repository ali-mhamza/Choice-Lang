#include "../include/error.h"
#include "../include/common.h"
#include "../include/token.h"
#include <cstdio>               // For stderr.
#include <string>

// RuntimeError.

RuntimeError::RuntimeError(const Token& token,
    const std::string& message) :
    token{token}, message{message} {}

void RuntimeError::report() const
{
    CH_PRINT(stderr, "Runtime error at ");
    if (token.type != TOK_EOF)
    {
        // CH_PRINT(stderr, "'{}' [{}:{}]: {}\n",
        //     token.text, token.line, token.position, message);
    }
    else
        CH_PRINT(stderr, "end: {}\n", message);
}