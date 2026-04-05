#pragma once
#include "common.h"     // For vT, fixed-size integer types.
#include "token.h"
#include <string_view>

class Lexer
{
    private:
        const char* start{};
        const char* current{};
        const char* end{};
        vT stream{};
        ui16 line{1};
        ui8 column{1};
        ClassState state{ false, 0 };
        bool hitError{false};
        int errorCount{0};

        /* Utilities. */

        // Prepares our lexer state.
        void setUp(const std::string_view& code);
        // Check if we've reached the end.
        bool hitEnd() const;
        // Move to next character.
        char advance();
        // Check next character.
        bool checkChar(char c) const;
        // Only advance if char matches.
        bool consumeChar(char c);
        void consumeChars(int count = 1);
        char peekChar(int distance = 0) const;
        char previousChar(int distance = 0) const;
        TokenType identifierType();
        bool matchSequence(char c, int length) const;

        /* Value conversion methods. */

        i64 intValue(std::string_view text) const;
        double decValue(std::string_view text) const;
        bool boolValue(TokenType type) const;

        /* Token makers. */

        void makeToken(TokenType type);
        void rangeToken();
        void numToken();
        // `raw`: True if string is a raw string; false otherwise.
        void stringToken(bool raw);
        void multiStringToken(bool raw); // For multi-line strings.
        void identifierToken();
        // For nested comments with ###.
        // Returns true if nested comment was hit, false otherwise.
        bool checkHyperComment();
        // Largely inspired by similar function in Wren source code.
        void conditionalToken(char c, TokenType two, TokenType one);
        void singleToken();

    public:
        Lexer() = default;
        vT& tokenize(const std::string_view code);
};