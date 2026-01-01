#include "../include/token.h"

Token::Token() :
    text(""), line(0), position(0), type(TOK_EOF)
{
    content.s = nullptr;
}

Token::Token(TokenType type, std::string_view text, Value content,
                ui16 line, ui8 position) :
    text(text), content(content), line(line),
    position(position), type(type) {}