#pragma once
#include "common.h"     // For vT, fixed-size integer types.
#include "diagnostic.h"
#include "token.h"
#include <string>
#include <string_view>

class Lexer
{
    private:
        enum NumBase : u8 { DEC, BIN, OCT, HEX };

        FileID id{};
        DiagnosticEngine* engine{};

        const char* start{};
        const char* current{};
        const char* end{};

        vT stream{};
        u64 offset{0};
        NumBase base{DEC};

        bool hitError{false};
        int errorCount{0};

        /* Utilities. */

        // Prepares our lexer state.
        void setUp(FileID id, const std::string_view& code);
        // Check if we've reached the end.
        bool hitEnd() const;
        // Move to next character.
        char advance();
        // Check next character.
        bool checkChar(char c) const;
        // Only advance if char matches.
        bool consumeChar(char c);
        void consumeChars(size_t count = 1);
        char peekChar(size_t distance = 0) const;
        char previousChar(size_t distance = 0) const;

        TokenType identifierType();
        bool matchSequence(char c, int length) const;

        // For nested comments with ###.
        // Returns true if nested comment was hit, false otherwise.
        bool checkHyperComment();
        bool checkRawString(char start);
        bool checkNumericLiteral(char start);

        void reportError(DiagCode code, u64 offset, std::string_view message = "");

        /* Value conversion methods. */

        // `dec` - True if parsing a floating-point value; false otherwise.
        std::string& formatNumber(const std::string_view text, bool dec);

        i64 intValue(std::string_view text);
        double decValue(std::string_view text);
        bool boolValue(TokenType type) const;

        /* Token makers. */

        void makeToken(TokenType type);
        void numToken();
        // Binary, octal, and hexadecimal literals.
        void numericToken(bool (*check)(char));
        // `raw` - True if string is a raw string; false otherwise.
        void stringToken(bool raw);
        void multiLineStringToken(bool raw);
        // Consumes a single %(...) parameter.
        // Returns false on error; true otherwise.
        bool formatParam();
        // `endDelim` - " for regular strings, ` for multi-line strings.
        void formatStringToken(char endDelim);
        void identifierToken();
        // Largely inspired by similar function in Wren source code.
        void conditionalToken(char c, TokenType two, TokenType one);
        void singleToken();

    public:
        Lexer(DiagnosticEngine* engine);
        vT& tokenize(FileID id, const std::string_view code);
};