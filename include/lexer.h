#pragma once
#include "common.h"
#include "diagnostic.h"
#include "token.h"
#include <string>
#include <string_view>

class Lexer
{
    private:
        enum class NumBase : u8 { Dec, Bin, Oct, Hex };

        FileID id{};
        const char* start{};
        const char* current{};
        const char* end{};

        vT stream{};
        u64 offset{0};
        NumBase base{NumBase::Dec};
        // `hitError`: Across entire input.
        // `inError`: In current token being constructed/scanned.
        bool hitError{false}, inError{false};

        /* Utilities. */

        // Prepares our lexer state.
        void setUp(FileID id, const std::string_view& code);
        // Check if we've reached the end.
        [[nodiscard]] bool hitEnd() const;
        // Move to next character.
        char advance();
        // Check next character.
        [[nodiscard]] bool checkChar(char c) const;
        // Only advance if char matches.
        [[nodiscard]] bool consumeChar(char c);
        void consumeChars(size_t count = 1);
        [[nodiscard]] char peekChar(size_t distance = 0) const;
        [[nodiscard]] char previousChar(size_t distance = 0) const;

        TokenType identifierType();
        [[nodiscard]] bool matchSequence(char c, int length) const;

        // For nested comments with ###.
        // Returns true if nested comment was hit, false otherwise.
        [[nodiscard]] bool checkHyperComment();

        [[nodiscard]] bool checkRawString(char start);
        [[nodiscard]] bool checkNumericLiteral(char start);

        void reportError(DiagCode code, u64 offset, std::string_view message = "");

        /* Value conversion methods. */

        // `dec` - True if parsing a floating-point value; false otherwise.
        [[nodiscard]]
        std::string& formatNumber(const std::string_view text, bool dec);

        [[nodiscard]] i64 intValue(std::string_view text);
        [[nodiscard]] double decValue(std::string_view text);
        [[nodiscard]] bool boolValue(TokenType type) const;

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
        [[nodiscard]] bool formatParam();
        // `endDelim` - " for regular strings, ` for multi-line strings.
        void formatStringToken(char endDelim);
        void identifierToken();
        // Largely inspired by similar function in Wren source code.
        void conditionalToken(char c, TokenType two, TokenType one);
        void singleToken();

    public:
        Lexer() = default;
        vT& tokenize(FileID id, const std::string_view code);
};