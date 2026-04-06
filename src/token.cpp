#include "../include/token.h"
#include "../include/common.h"
#include <string_view>

Token::Token(
    TokenType type,
    std::string_view text,
    Value content,
    ui16 line,
    ui8 position
) :
    text{text}, content{content}, line{line},
    position{position}, type{type} {}

Token::Token(const Token& other) noexcept :
    text{other.text}, line{other.line}, position{other.position},
    type{other.type}
{
    this->content.i = other.content.i;
}

Token& Token::operator=(const Token& other) noexcept
{
    if (this != &other)
    {
        this->text = other.text;
        this->content.i = other.content.i;
        this->line = other.line;
        this->position = other.position;
        this->type = other.type;
    }

    return *this;
}

Token::Token(Token&& other) noexcept :
    text{other.text}, line{other.line}, position{other.position},
    type{other.type}
{
    this->content.i = other.content.i;
    other.type = TOK_EOF; // Invalidate the other token.
}

Token& Token::operator=(Token&& other) noexcept
{
    if (this != &other)
    {
        this->text = other.text;
        this->content.i = other.content.i;
        this->line = other.line;
        this->position = other.position;
        this->type = other.type;

        other.type = TOK_EOF;
    }

    return *this;
}