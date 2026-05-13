#pragma once
#include "common.h"
#include "diagnostic.h"
#include "token.h"
#include <utility>

class TokenPrinter
{
    private:
        FileID id{};
        const vT& tokens{};

        // Returns the start index and length difference
        // for different string token types.
        [[nodiscard]]
        std::pair<size_t, size_t> stringTokenValues(TokenType type) const;
        void printValue(const Token& token) const;
        void printToken(const Token& token) const;

    public:
        TokenPrinter(FileID id, const vT& tokens);
        void printTokens() const;
};