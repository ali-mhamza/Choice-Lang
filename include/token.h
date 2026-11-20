#pragma once
#include "common.h"
#include <array>
#include <string_view>

enum TokenType : u8
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
	TOK_STR_LIT,		// "Hello, world!"
	TOK_RAW_STR,		// r"Hello,\tworld!\n"
	TOK_TRUE,			// true
	TOK_FALSE,			// false
	TOK_NULL,           // null
	TOK_INTER_START,	// ["Hello, ]%(NAME) is %(DISTANCE) away."
	TOK_INTER_PART,		// "Hello, %(NAME)[ is ]%(DISTANCE) away."
	TOK_INTER_END,		// "Hello, %(NAME) is %(DISTANCE)[ away."]

	/* Keywords. */

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

	TOK_IMMUT,			// immut
	TOK_MUT,			// mut
	TOK_AND,			// and
	TOK_OR,				// or
	TOK_NOT,			// not
	TOK_RETURN,			// return
	TOK_IN,				// in

	/* Arithmetic operators. */

	TOK_PLUS,			// +
	TOK_MINUS,			// -
	TOK_STAR,			// *
	TOK_SLASH,			// /
	TOK_LT_SLASH,		// </
	TOK_GT_SLASH,		// >/
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

	/* Range operator. */

	TOK_DOT_DOT_EQ,		// ..=
	TOK_DOT_DOT_LT,		// ..<

	/* Compound assignment operators. */

	TOK_PLUS_EQ,		// +=
	TOK_MINUS_EQ,		// -=
	TOK_STAR_EQ,		// *=
	TOK_SLASH_EQ,		// /=
	TOK_LT_SLASH_EQ,	// </=
	TOK_GT_SLASH_EQ,	// >/=
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

	/* Functions. */

	TOK_FUNC,			// func
	TOK_THICK_ARROW,	// =>
	TOK_ELLIPSIS,		// ...

	/* Types. */

	TOK_TYPE,			// type
	TOK_DOT,			// .
	TOK_RARROW,			// ->
	TOK_SUPER,			// super
	TOK_ALIAS,			// alias

	/* Modules. */

	TOK_USE,            // use
	TOK_FROM,           // from
	TOK_AS,             // as
	TOK_SCOPE,          // ::

	/* Attributes. */

	TOK_AT,             // @
	TOK_PRIVATE,        // private
	TOK_STATIC,			// static
	TOK_COMPUTED,		// computed
	TOK_CLOSED,			// closed
	TOK_TEST,			// test

	TOK_EOF,
	NUM_TOK_TYPES
};

// Can hold a literal of any needed size.
union Value {
	void*	s; // For NULL exclusively.
	i64		i;
	double	d;
	bool	b;
};

struct Token
{
	std::string_view text{};	// The actual text of the token.
	Value content{};			// The literal's actual value.
	u64 byteOffset{};			// Byte offset into the source code for the start of the token.
	TokenType type{TOK_EOF};

	Token() = default;
	Token(
		TokenType type,
		std::string_view text,
		Value content,
		u64 offset
	);
	Token(const Token& other) noexcept;
	Token& operator=(const Token& other) noexcept;
	Token(Token&& other) noexcept;
	Token& operator=(Token&& other) noexcept;

	operator bool() const { return type != TOK_EOF; }
};

#define IS_LITERAL_TOK(type)	((TOK_NUM <= (type)) && ((type) <= TOK_NULL))
// Token used in string interpolation.
#define IS_INTER_TOK(type)		((TOK_INTER_START <= (type)) && ((type) <= TOK_INTER_END))
#define IS_ASSIGN_TOK(type) \
	(((type) == TOK_EQUAL) || (((type) >= TOK_PLUS_EQ) && ((type) <= TOK_RSHIFT_EQ)))

inline constexpr std::array<const char*, NUM_TOK_TYPES> tokenTypeStrings{
    "TOK_LEFT_BRACKET", "TOK_RIGHT_BRACKET", "TOK_LEFT_PAREN",
    "TOK_RIGHT_PAREN", "TOK_LEFT_BRACE", "TOK_RIGHT_BRACE",
    "TOK_SEMICOLON", "TOK_COMMA", "TOK_QMARK",

    "TOK_NUM", "TOK_NUM_DEC", "TOK_STR_LIT", "TOK_RAW_STR",
    "TOK_TRUE", "TOK_FALSE", "TOK_NULL", "TOK_INTER_START",
    "TOK_INTER_PART", "TOK_INTER_END",

    "TOK_IF", "TOK_ELIF", "TOK_ELSE", "TOK_WHILE", "TOK_FOR",
    "TOK_WHERE", "TOK_REPEAT", "TOK_UNTIL", "TOK_BREAK", "TOK_CONT",
    "TOK_MATCH", "TOK_IS", "TOK_FALL", "TOK_END",

    "TOK_IMMUT", "TOK_MUT", "TOK_AND", "TOK_OR", "TOK_NOT",
    "TOK_RETURN", "TOK_IN",

    "TOK_PLUS", "TOK_MINUS", "TOK_STAR", "TOK_SLASH", "TOK_LT_SLASH",
    "TOK_GT_SLASH", "TOK_PERCENT", "TOK_STAR_STAR", "TOK_INCR",
    "TOK_DECR",

    "TOK_EQ_EQ", "TOK_BANG_EQ","TOK_GT", "TOK_GT_EQ", "TOK_LT",
    "TOK_LT_EQ", "TOK_BANG", "TOK_AMP_AMP", "TOK_BAR_BAR",

    "TOK_AMP", "TOK_BAR", "TOK_UARROW", "TOK_TILDE",
    "TOK_LEFT_SHIFT", "TOK_RIGHT_SHIFT",

    "TOK_DOT_DOT_EQ", "TOK_DOT_DOT_LT",

    "TOK_PLUS_EQ", "TOK_MINUS_EQ", "TOK_STAR_EQ", "TOK_SLASH_EQ",
    "TOK_LT_SLASH_EQ", "TOK_GT_SLASH_EQ", "TOK_PERCENT_EQ",
    "TOK_STAR_STAR_EQ", "TOK_AMP_EQ", "TOK_BAR_EQ", "TOK_UARROW_EQ",
    "TOK_TILDE_EQ", "TOK_LSHIFT_EQ", "TOK_RSHIFT_EQ",

    "TOK_IDENTIFIER", "TOK_MAKE", "TOK_FIX", "TOK_COLON", "TOK_EQUAL",

    "TOK_FUNC", "TOK_THICK_ARROW", "TOK_ELLIPSIS",

    "TOK_TYPE", "TOK_DOT", "TOK_RARROW", "TOK_SUPER", "TOK_ALIAS",

    "TOK_USE", "TOK_FROM", "TOK_AS", "TOK_SCOPE",

    "TOK_AT", "TOK_PRIVATE", "TOK_STATIC", "TOK_COMPUTED",
    "TOK_CLOSED", "TOK_TEST",

    "TOK_EOF"
};