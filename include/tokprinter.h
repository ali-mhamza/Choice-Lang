#pragma once
#include "common.h" // For vT.

class TokenPrinter
{
    private:
        const vT& tokens;

        void printValue(const Token& token);
        void printToken(const Token& token);
    
    public:
        TokenPrinter(const vT& tokens);
        void printTokens();
};