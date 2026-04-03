#pragma once
#include "common.h" // For vT.

class TokenPrinter
{
    private:
        const vT& tokens{};

        void printValue(const Token& token) const;
        void printToken(const Token& token) const;
    
    public:
        TokenPrinter(const vT& tokens);
        void printTokens() const;
};