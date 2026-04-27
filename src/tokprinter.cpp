#include "../include/tokprinter.h"
#include "../include/common.h"
#include "../include/token.h"
#include <array>
#include <string>
#include <string_view>

#define NEWLINE_REPLACEMENT "<NL>"

TokenPrinter::TokenPrinter(const vT& tokens) :
    tokens{tokens} {}

static std::string formatMultiLineString(const std::string_view& sv)
{
    std::string newStr{sv};

    auto it{newStr.find('\n')};
    while (it != newStr.npos)
    {
        newStr.replace(it, 1, NEWLINE_REPLACEMENT);
        it = newStr.find('\n', it + 1);
    }

    return newStr;
}

std::pair<size_t, size_t>
TokenPrinter::stringTokenValues(TokenType type) const
{
    switch (type)
    {
        case TOK_STR_LIT:       return std::make_pair(1, 2);
        case TOK_RAW_STR:       return std::make_pair(2, 3);
        case TOK_INTER_START:   return std::make_pair(1, 1);
        case TOK_INTER_PART:    return std::make_pair(0, 0);
        case TOK_INTER_END:     return std::make_pair(0, 1);
        default: CH_UNREACHABLE();
    }
}

void TokenPrinter::printValue(const Token& token) const
{
    switch (token.type)
    {
        case TOK_NUM:       CH_PRINT("{}", token.content.i);    break;
        case TOK_NUM_DEC:   CH_PRINT("{}", token.content.d);    break;
        case TOK_STR_LIT:       case TOK_RAW_STR:       case TOK_INTER_START:
        case TOK_INTER_PART:    case TOK_INTER_END:
        {
            auto tokenPair{stringTokenValues(token.type)};
            CH_PRINT("{}", formatMultiLineString(
                token.text.substr(tokenPair.first, token.text.size() - tokenPair.second)
            ));
            break;
        }
        case TOK_TRUE:      CH_PRINT("true");                   break;
        case TOK_FALSE:     CH_PRINT("false");                  break;
        case TOK_NULL:      CH_PRINT("{}", token.content.s);    break;
        default: CH_UNREACHABLE();
    }
}

constexpr std::array<const char*, NUM_TOK_TYPES> typeStrings{
    "TOK_LEFT_BRACKET", "TOK_RIGHT_BRACKET", "TOK_LEFT_PAREN",
    "TOK_RIGHT_PAREN", "TOK_LEFT_BRACE", "TOK_RIGHT_BRACE",
    "TOK_SEMICOLON", "TOK_COMMA", "TOK_QMARK",

    "TOK_NUM", "TOK_NUM_DEC", "TOK_STR_LIT", "TOK_RAW_STR",
    "TOK_TRUE", "TOK_FALSE", "TOK_NULL", "TOK_INTER_START",
    "TOK_INTER_PART", "TOK_INTER_END",

    "TOK_INT", "TOK_DEC", "TOK_BOOL", "TOK_STRING",
    "TOK_FUNC", "TOK_ARRAY", "TOK_TABLE", "TOK_ANY", "TOK_CLASS",

    "TOK_IF", "TOK_ELIF", "TOK_ELSE", "TOK_WHILE", "TOK_FOR",
    "TOK_WHERE", "TOK_REPEAT", "TOK_UNTIL", "TOK_BREAK", "TOK_CONT",
    "TOK_MATCH", "TOK_IS", "TOK_FALL", "TOK_END",

    "TOK_AND", "TOK_OR", "TOK_NOT", "TOK_RETURN", "TOK_NEW",
    "TOK_DEF", "TOK_FIELDS", "TOK_IN",

    "TOK_PLUS", "TOK_MINUS", "TOK_STAR", "TOK_SLASH", "TOK_PERCENT",
    "TOK_STAR_STAR", "TOK_INCR", "TOK_DECR",
    
    "TOK_EQ_EQ", "TOK_BANG_EQ","TOK_GT", "TOK_GT_EQ", "TOK_LT",
    "TOK_LT_EQ", "TOK_BANG", "TOK_AMP_AMP", "TOK_BAR_BAR",

    "TOK_AMP", "TOK_BAR", "TOK_UARROW", "TOK_TILDE",
    "TOK_LEFT_SHIFT", "TOK_RIGHT_SHIFT",

    "TOK_DOT_DOT",

    "TOK_PLUS_EQ", "TOK_MINUS_EQ", "TOK_STAR_EQ", "TOK_SLASH_EQ",
	"TOK_PERCENT_EQ", "TOK_STAR_STAR_EQ", "TOK_AMP_EQ", "TOK_BAR_EQ",
	"TOK_UARROW_EQ", "TOK_TILDE_EQ", "TOK_LSHIFT_EQ", "TOK_RSHIFT_EQ",

    "TOK_IDENTIFIER", "TOK_MAKE", "TOK_FIX", "TOK_COLON", "TOK_EQUAL",

    "TOK_DOT", "TOK_UNDER_UNDER", "TOK_RARROW",

    "TOK_EOF"
};

void TokenPrinter::printToken(const Token& token) const
{
    CH_PRINT("{:<20}", typeStrings[token.type]);
    if (token.type != TOK_EOF)
    {
        std::string format{CH_STR("({}:{})", token.line, token.position)};
        CH_PRINT("{:<10}", format);

        if ((token.type != TOK_STR_LIT) && (token.type != TOK_RAW_STR))
            CH_PRINT("'{}' ", token.text);
        else
            CH_PRINT("'{}' ", formatMultiLineString(token.text));

        if (IS_LITERAL_TOK(token.type) || IS_INTER_TOK(token.type))
            printValue(token);
    }
    
    CH_PRINT("\n");
}

void TokenPrinter::printTokens() const
{
    for (const Token& token : tokens)
        printToken(token);
    CH_PRINT("TOK COUNT: {}\n", tokens.size());
}