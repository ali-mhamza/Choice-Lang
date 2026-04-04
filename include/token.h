#pragma once
#include "common.h"		// For fixed-size integer types.
#include <string_view>

enum TokenType : ui8
{
	/* Characters. */

	TOK_LEFT_BRACKET,	// [
	TOK_RIGHT_BRACKET,	// ]
	TOK_LEFT_PAREN,		// (
	TOK_RIGHT_PAREN,	// )
	TOK_LEFT_BRACE,		// {
	TOK_RIGHT_BRACE,	// }
	TOK_SEMICOLON,		// ;
	TOK_COMMA,          // ,
	TOK_QMARK,			// ?
	
	/* Literals. */

	TOK_NUM,			// 123 (default)
	TOK_NUM_DEC,		// 1.23
	TOK_RANGE,			// 1..10..2
	TOK_STR_LIT,		// "Hello, world!"
	TOK_RAW_STR,		// "Hello,\tworld!\n"
	TOK_TRUE,			// true
	TOK_FALSE,			// false
	TOK_NULL,           // null

	/* Keywords. */

	// Types.

	TOK_INT,            // int
	TOK_DEC,            // dec
	TOK_BOOL,           // boolean
	TOK_STRING,         // string
	TOK_FUNC,			// func
	TOK_ARRAY,			// array
	TOK_TABLE,			// table
	TOK_ANY,			// any
	TOK_CLASS,			// class

	// Control flow.

	TOK_IF,				// if
	TOK_ELIF,			// elif
	TOK_ELSE,			// else
	TOK_WHILE,			// while
	TOK_FOR,			// for
	TOK_WHERE,			// where
	TOK_REPEAT,			// repeat
	TOK_UNTIL,			// until
	TOK_BREAK,			// break
	TOK_CONT,			// continue
	TOK_MATCH,			// match
	TOK_IS,				// is
	TOK_FALL,			// fallthrough
	TOK_END,			// end

	// Miscellaneous.

	TOK_AND,			// and
	TOK_OR,				// or
	TOK_NOT,			// not
	TOK_RETURN,			// return
	TOK_NEW,			// new
	TOK_DEF,			// def
	TOK_FIELDS,			// fields
	TOK_IN,				// in

	/* Arithmetic operators. */

	TOK_PLUS,			// +
	TOK_MINUS,			// -
	TOK_STAR,			// *
	TOK_SLASH,			// /
	TOK_PERCENT,		// %
	TOK_STAR_STAR,		// **

	TOK_INCR,			// ++
	TOK_DECR,			// --

	/* Boolean/comparison operators. */

	TOK_EQ_EQ,			// ==
	TOK_BANG_EQ,		// !=
	TOK_GT,				// >
	TOK_GT_EQ,			// >=
	TOK_LT,				// <
	TOK_LT_EQ,			// <=

	TOK_BANG,			// !
	TOK_AMP_AMP,		// &&
	TOK_BAR_BAR,		// ||

	/* Bit-wise operators. */

	TOK_AMP,			// &
	TOK_BAR,			// |
	TOK_UARROW,			// ^
	TOK_TILDE,          // ~
	TOK_LEFT_SHIFT,		// <<
	TOK_RIGHT_SHIFT,	// >>

	/* Compound assignment operators. */

	TOK_PLUS_EQ,		// +=
	TOK_MINUS_EQ,		// -=
	TOK_STAR_EQ,		// *=
	TOK_SLASH_EQ,		// /=
	TOK_PERCENT_EQ,		// %=
	TOK_STAR_STAR_EQ,	// **=

	TOK_AMP_EQ,			// &=
	TOK_BAR_EQ,			// |=
	TOK_UARROW_EQ,		// ^=
	TOK_TILDE_EQ,		// ~=
	TOK_LSHIFT_EQ,		// <<=
	TOK_RSHIFT_EQ,		// >>=

	/* Variables and declarations. */

	TOK_IDENTIFIER,     // variableName
	TOK_MAKE,			// make
	TOK_FIX,			// fix
	TOK_COLON,			// :
	TOK_EQUAL,			// =

	/* Classes. */

	TOK_DOT,			// .
	TOK_UNDER_UNDER,	// __
	TOK_RARROW,			// ->

	TOK_EOF,
	NUM_TOK_TYPES
};

// Can hold a literal of any needed size.
union Value {
	void*			s; // For NULL exclusively.
	i64				i;
	double			d;
	bool			b;
};

struct Token
{
	std::string_view text{};	// The actual text of the token.
	Value content{};			// The literal's actual value.
	ui16 line{};				// The line holding the token.
	ui8 position{};				// The starting position of the token.
	TokenType type{TOK_EOF};

	Token() = default;
	Token(
		TokenType type,
		std::string_view text,
		Value content,
		ui16 line,
		ui8 position
	);
	Token(const Token& other) noexcept;
	Token& operator=(const Token& other) noexcept;
	Token(Token&& other) noexcept;
	Token& operator=(Token&& other) noexcept;
};

#define IS_LITERAL_TOK(type)	((TOK_NUM <= (type)) && ((type) <= TOK_NULL))
#define IS_TYPE_TOK(type)		((TOK_INT <= (type)) && ((type) <= TOK_ANY))
#define IS_ASSIGN_TOK(type)										\
	(((type) == TOK_EQUAL)										\
	|| (((type) >= TOK_PLUS_EQ) && ((type) <= TOK_RSHIFT_EQ)))
