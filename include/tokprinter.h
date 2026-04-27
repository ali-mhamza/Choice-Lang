#pragma once
#include "common.h"
#include "token.h"
#include <utility>

class TokenPrinter
{
    private:
        const vT& tokens{};

        // Returns the start index and length difference
        // for different string token types.
        std::pair<size_t, size_t> stringTokenValues(TokenType type) const;
        void printValue(const Token& token) const;
        void printToken(const Token& token) const;
    
    public:
        TokenPrinter(const vT& tokens);
        void printTokens() const;
};