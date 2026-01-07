#include "../include/tokprinter.h"
#include "../include/common.h"
#include "../include/token.h"
#include <algorithm>

TokenPrinter::TokenPrinter(const vT& tokens) :
    tokens(tokens) {}

static std::string formatMultiLineString(const std::string_view& sv)
{
    std::string newStr(sv);

    // Clear any unwanted whitespace characters.
    newStr.erase(std::remove_if(newStr.begin(), newStr.end(), [](char c){
        return ((c == '\r') || (c == '\f') || (c == '\v'));
    }), newStr.end());

    auto it = newStr.find('\n');
    while (it != newStr.npos)
    {
        newStr.replace(it, 1, "\\n");
        it = newStr.find('\n', it + 1);
    }

    return newStr;
}

void TokenPrinter::printValue(const Token& token)
{
    switch (token.type)
    {
        case TOK_NUM:   FORMAT_PRINT("{}", token.content.i);    break;
        case TOK_DEC:   FORMAT_PRINT("{}", token.content.d);    break;
        case TOK_STR_LIT:
            FORMAT_PRINT("{}", formatMultiLineString(
                token.text.substr(1, token.text.size() - 2)
            ));
            break;
        case TOK_TRUE:  FORMAT_PRINT("true");                   break;
        case TOK_FALSE: FORMAT_PRINT("false");                  break;
        case TOK_NULL:  FORMAT_PRINT("{}", token.content.s);    break;
        default: UNREACHABLE();
    }
}

static const char* typeStrings[] = {
    "TOK_LEFT_BRACKET", "TOK_RIGHT_BRACKET", "TOK_LEFT_PAREN",
    "TOK_RIGHT_PAREN", "TOK_LEFT_BRACE", "TOK_RIGHT_BRACE",
    "TOK_NEWLINE", "TOK_SEMICOLON", "TOK_COMMA", "TOK_QMARK",

    "TOK_NUM", "TOK_NUM_DEC","TOK_STR_LIT", "TOK_TRUE",
    "TOK_FALSE", "TOK_NULL",

    "TOK_INT", "TOK_DEC", "TOK_BOOL", "TOK_STRING",
    "TOK_FUNC", "TOK_ARRAY", "TOK_TABLE", "TOK_ANY", "TOK_CLASS",

    "TOK_IF", "TOK_ELIF", "TOK_ELSE", "TOK_WHILE", "TOK_FOR",
    "TOK_WHERE", "TOK_REPEAT", "TOK_UNTIL", "TOK_BREAK", "TOK_CONT",
    "TOK_MATCH", "TOK_IS", "TOK_FALL", "TOK_END",

    "TOK_AND", "TOK_OR", "TOK_RETURN", "TOK_NEW", "TOK_DEF",
    "TOK_FIELDS", "TOK_IN",

    "TOK_PLUS", "TOK_MINUS", "TOK_STAR", "TOK_SLASH", "TOK_PERCENT",
    "TOK_STAR_STAR", "TOK_INCR", "TOK_DECR",
    
    "TOK_EQ_EQ", "TOK_BANG_EQ","TOK_GT", "TOK_GT_EQ", "TOK_LT",
    "TOK_LT_EQ", "TOK_BANG", "TOK_AMP_AMP", "TOK_BAR_BAR",

    "TOK_AMP", "TOK_BAR", "TOK_UARROW", "TOK_TILDE",
    "TOK_LEFT_SHIFT", "TOK_RIGHT_SHIFT",

    "TOK_PLUS_EQ", "TOK_MINUS_EQ", "TOK_STAR_EQ", "TOK_SLASH_EQ",
	"TOK_PERCENT_EQ", "TOK_STAR_STAR_EQ", "TOK_AMP_EQ", "TOK_BAR_EQ",
	"TOK_UARROW_EQ", "TOK_TILDE_EQ", "TOK_LSHIFT_EQ", "TOK_RSHIFT_EQ",

    "TOK_IDENTIFIER", "TOK_MAKE", "TOK_FIX", "TOK_COLON", "TOK_EQUAL",

    "TOK_DOT", "TOK_UNDER_UNDER", "TOK_RARROW",

    "TOK_EOF"
};

void TokenPrinter::printToken(const Token& token)
{
    FORMAT_PRINT("{:<20}", typeStrings[token.type]);
    if (token.type != TOK_EOF)
    {
        std::string format = FORMAT_STR("({}:{})",
            token.line, token.position);
        FORMAT_PRINT("{:<10}", format);

        if ((token.type != TOK_NEWLINE) && (token.type != TOK_STR_LIT))
            FORMAT_PRINT("'{}' ", token.text);
        else if (token.type == TOK_STR_LIT)
            FORMAT_PRINT("'{}' ", formatMultiLineString(token.text));
        else
            FORMAT_PRINT("'\\n ");

        if (IS_LITERAL(token.type))
            printValue(token);
    }
    
    FORMAT_PRINT("\n");
}

void TokenPrinter::printTokens()
{
    for (const Token& token : tokens)
        printToken(token);
    FORMAT_PRINT("TOK COUNT: {}\n", tokens.size());
}