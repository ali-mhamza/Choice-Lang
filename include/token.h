#pragma once
#include "common.h"
#include "object.h"
#include <iostream>
#include <string_view>
#include <variant>

// Can hold a literal of any needed size.
using Value = std::variant<i64, double, bool, std::string_view, void*>;
class Lexer;
class TokenPrinter;
class Compiler; class AltCompiler;
class CompileError;
class VM;

enum TokenType : ui8
{
	// Characters.
	TOK_LEFT_BRACKET,	// [
	TOK_RIGHT_BRACKET,	// ]
	TOK_LEFT_PAREN,		// (
	TOK_RIGHT_PAREN,	// )
	TOK_LEFT_BRACE,		// {
	TOK_RIGHT_BRACE,	// }
	TOK_NEWLINE,		// \n
	TOK_SEMICOLON,		// ;
	TOK_COMMA,          // ,
	
	// Literals.

	TOK_NUM,			// 123 (default)
	TOK_NUM_DEC,		// 1.23
	TOK_STR_LIT,		// "Hello, world!"
	TOK_TRUE,			// true
	TOK_FALSE,			// false
	TOK_NULL,           // null

	// Keywords.

	TOK_INT,            // int
	TOK_DEC,            // dec
	TOK_BOOL,           // boolean
	TOK_STRING,         // string
	TOK_FUNC,			// func
	TOK_ARRAY,			// array
	TOK_TABLE,			// table
	TOK_ANY,			// any
	TOK_CLASS,			// class
	TOK_AND,			// and
	TOK_OR,				// or
	TOK_RETURN,			// return
	TOK_NEW,			// new
	TOK_DEF,			// def
	TOK_FIELDS,			// fields
	TOK_IN,				// in

	// Arithmetic operators.

	TOK_PLUS,			// +
	TOK_MINUS,			// -
	TOK_STAR,			// *
	TOK_SLASH,			// /
	TOK_PERCENT,		// %
	TOK_STAR_STAR,		// **

	// Boolean/comparison operators.

	TOK_EQ_EQ,			// ==
	TOK_BANG_EQ,		// !=
	TOK_GT,				// >
	TOK_GT_EQ,			// >=
	TOK_LT,				// <
	TOK_LT_EQ,			// <=
	TOK_BANG,			// !
	TOK_AMP_AMP,		// &&
	TOK_BAR_BAR,		// ||

	// Bit-wise operators.
	TOK_AMP,			// &
	TOK_BAR,			// |
	TOK_UARROW,			// ^
	TOK_TILDE,          // ~
	TOK_LEFT_SHIFT,		// <<
	TOK_RIGHT_SHIFT,	// >>

	// Variables and declarations.

	TOK_IDENTIFIER,     // variableName
	TOK_MAKE,			// make
	TOK_FIX,			// fix
	TOK_COLON,			// :
	TOK_EQUAL,			// =

	// Classes.

	TOK_DOT,			// .
	TOK_UNDER_UNDER,	// __
	TOK_RARROW,			// ->

	TOKEN_EOF
};

class Token
{
	private:
		Value content; // The literal's actual value.
		std::string_view text; // The actual text of the token.
		ui16 line; // The line holding the token.
		ui8 position; // The starting position of the token.
		TokenType type;
	
	public:
		Token();
		Token(TokenType type, std::string_view text, Value content,
				ui16 line, ui8 position);

		friend class Lexer;
		friend class TokenPrinter;
		friend class Parser;
		friend class Compiler;
		friend class ASTCompiler;
		friend class CompileError;
		friend class VM;
};

#define IS_LITERAL(type)	((TOK_NUM <= type) && (type <= TOK_NULL))
#define IS_TYPE(type)		((TOK_INT <= type) && (type <= TOK_ANY))