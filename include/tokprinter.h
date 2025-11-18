#pragma once
#include "common.h"

class TokenPrinter
{
    private:
        const vT& tokens;

        void printToken(const Token& token);
    
    public:
        TokenPrinter(const vT& tokens);
        void printTokens();
};