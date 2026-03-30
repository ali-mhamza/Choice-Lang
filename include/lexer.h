#pragma once
#include "common.h"     // For vT, fixed-size integer types.
#include "token.h"
#include <string_view>

struct ClassState
{
    bool inClass;
    ui8 braceCount;
};

class Lexer
{
    private:
        const char* start{nullptr};
        const char* current{nullptr};
        const char* end{nullptr};
        vT stream;
        ui16 line{1};
        ui8 column{1};
        ClassState state{ false, 0 };
        bool hitError{false};
        int errorCount{0};

        /* Utilities. */

        // Prepares our lexer state.
        void setUp(const std::string_view& code);
        // Check if we've reached the end.
        bool hitEnd();
        // Move to next character.
        char advance();
        // Check next character.
        bool checkChar(char c);
        // Only advance if char matches.
        bool consumeChar(char c);
        void consumeChars(int count = 1);
        char peekChar(int distance = 0);
        char previousChar(int distance = 0);
        TokenType identifierType();
        bool matchSequence(char c, int length);

        /* Value conversion methods. */

        i64 intValue(std::string_view text);
        double decValue(std::string_view text);
        bool boolValue(TokenType type);

        /* Token makers. */

        void makeToken(TokenType type);
        void rangeToken();
        void numToken();
        void stringToken();
        // For multi-line strings.
        void multiStringToken();
        void identifierToken();
        // For nested comments with ###.
        // Returns true if nested comment was hit, false otherwise.
        bool checkHyperComment();
        // Largely inspired by similar function in Wren source code.
        void conditionalToken(char c, TokenType two, TokenType one);
        void singleToken();
    
    public:
        Lexer() = default;
        vT& tokenize(std::string_view code);
};