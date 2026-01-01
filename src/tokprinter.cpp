#include "../include/tokprinter.h"
#include "../include/token.h"
#include <iomanip>
#include <iostream>
#include <sstream>

TokenPrinter::TokenPrinter(const vT& tokens) :
    tokens(tokens) {}

static void printValue(Value value)
{   
    std::visit([](auto&& val){
        std::cout << std::boolalpha << val;
    }, value);
}

static const char* typeStrings[] = {
    "TOK_LEFT_BRACKET", "TOK_RIGHT_BRACKET", "TOK_LEFT_PAREN",
    "TOK_RIGHT_PAREN", "TOK_LEFT_BRACE", "TOK_RIGHT_BRACE",
    "TOK_NEWLINE", "TOK_SEMICOLON", "TOK_COMMA",

    "TOK_NUM", "TOK_NUM_DEC","TOK_STR_LIT", "TOK_TRUE",
    "TOK_FALSE", "TOK_NULL",

    "TOK_INT", "TOK_DEC", "TOK_BOOL", "TOK_STRING",
    "TOK_FUNC", "TOK_ARRAY", "TOK_TABLE", "TOK_ANY", "TOK_CLASS",
    "TOK_AND", "TOK_OR", "TOK_RETURN", "TOK_NEW", "TOK_DEF",
    "TOK_FIELDS", "TOK_IN",

    "TOK_PLUS", "TOK_MINUS", "TOK_STAR", "TOK_SLASH", "TOK_PERCENT",
    "TOK_STAR_STAR", "TOK_EQ_EQ", "TOK_BANG_EQ","TOK_GT",
    "TOK_GT_EQ", "TOK_LT", "TOK_LT_EQ", "TOK_BANG", "TOK_AMP_AMP",
    "TOK_BAR_BAR",

    "TOK_AMP", "TOK_BAR", "TOK_UARROW", "TOK_TILDE",
    "TOK_LEFT_SHIFT", "TOK_RIGHT_SHIFT",

    "TOK_IDENTIFIER", "TOK_MAKE", "TOK_FIX", "TOK_COLON", "TOK_EQUAL",

    "TOK_DOT", "TOK_UNDER_UNDER", "TOK_RARROW",

    "TOKEN_EOF"
};

void TokenPrinter::printToken(const Token& token)
{
    std::cout << std::left << std::setw(20) << typeStrings[token.type];
    if (token.type != TOKEN_EOF)
    {
        // Line up the line:column printing.
        std::ostringstream oss;
        oss << "(" << token.line << ":" << static_cast<int>(token.position) << ")";
        std::cout << std::left << std::setw(10) << oss.str();

        if (token.type != TOK_NEWLINE)
            std::cout << "'" << std::string(token.text) << "' ";
        else
            std::cout << "'\\n' ";

        if (IS_LITERAL(token.type))
            printValue(token.content);
    }
    
    std::cout << '\n';
}

void TokenPrinter::printTokens()
{
    for (const Token& token : tokens)
        printToken(token);
    std::cout << "TOK COUNT: " << tokens.size() << '\n';
}