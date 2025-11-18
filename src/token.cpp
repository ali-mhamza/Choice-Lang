#include "../include/token.h"

Token::Token(TokenType type, std::string_view text, Value content,
                uint16_t line, uint8_t position) :
    type(type), text(text), content(content), 
    line(line), position(position) {}