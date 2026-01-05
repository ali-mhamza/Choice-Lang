#include "../include/lexer.h"
#include "../include/common.h"
#include "../include/error.h"
#include "../include/object.h"
#include "../include/token.h"
#include "../include/utils.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <unordered_map>

static std::unordered_map<std::string_view, TokenType> keywords = {
	{"int", TOK_INT},		{"dec", TOK_DEC},		{"boolean", TOK_BOOL},
	{"string", TOK_STRING},	{"func", TOK_FUNC},		{"array", TOK_ARRAY},
	{"table", TOK_TABLE},	{"class", TOK_CLASS},	{"any",	TOK_ANY},
	{"if", TOK_IF},			{"elif", TOK_ELIF},		{"else", TOK_ELSE},
	{"while", TOK_WHILE},	{"for", TOK_FOR},		{"where", TOK_WHERE},
	{"repeat", TOK_REPEAT},	{"until", TOK_UNTIL},	{"break", TOK_BREAK},
	{"continue", TOK_CONT},	{"match", TOK_MATCH},	{"is", TOK_IS},
	{"fallthrough", TOK_FALL}, {"end", TOK_END},	{"make", TOK_MAKE},
	{"fix", TOK_FIX},		{"true", TOK_TRUE},		{"false", TOK_FALSE},
	{"null", TOK_NULL},		{"and", TOK_AND},		{"or", TOK_OR},
	{"new", TOK_NEW},		{"def", TOK_DEF},		{"fields", TOK_FIELDS},
	{"in", TOK_IN}
};

Lexer::Lexer() :
	line(1), column(1), start(0), current(0),
	state({false, 0})/*, inGrouping(false)*/ {}

bool Lexer::hitEnd()
{
	return (current >= code.length());
}

char Lexer::advance()
{
	if (!hitEnd())
	{
		current++;
		if (code[current - 1] == '\n')
		{
			line++;
			column = 1;
		}
		else
			column++;
		return code[current - 1];
	}

	return EOF;
}

bool Lexer::checkChar(char c)
{
	if (!hitEnd())
		return (code[current] == c);
	return false;
}

bool Lexer::consumeChar(char c)
{
	if (checkChar(c))
	{
		advance();
		return true;
	}

	return false;
}

void Lexer::consumeChars(char c, int count /* = 1 */)
{
	for (int i = 0; i < count; i++)
		consumeChar(c);
}

char Lexer::peekChar(int distance /* = 0 */)
{
	if (current + distance < code.length())
		return code[current + distance];
	return EOF;
}

char Lexer::previousChar(int distance /* = 0 */)
{
	if (current - distance > 1)
		return code[current - distance - 1];
	return EOF;
}

i64 Lexer::intValue(std::string_view text)
{
	return static_cast<i64>(std::stoll(std::string(text)));
}

double Lexer::decValue(std::string_view text)
{
	return static_cast<double>(std::stod(std::string(text)));
}

bool Lexer::boolValue(TokenType type)
{
	return (type == TOK_TRUE);
}

void Lexer::makeToken(TokenType type)
{
	std::string_view text = code.substr(start, current - start);

	Value value;
	if (IS_LITERAL(type))
	{
		switch (type)
		{
			case TOK_NUM:		value.i = intValue(text);	break;
			case TOK_NUM_DEC:	value.d = decValue(text);	break;
			case TOK_TRUE:
			case TOK_FALSE:		value.b = boolValue(type);	break;
			case TOK_NULL:		value.s = "NULL";			break;
			// For string literals we use the token's own text later.
			default: break;
		}
	}
	
	stream.emplace_back(type, text, value, line, 
		column - (current - start));
}

void Lexer::numToken()
{
	TokenType type;	
	while (isdigit(peekChar()) && !hitEnd())
		advance();

	if (consumeChar('.'))
	{
		while (isdigit(peekChar()) && !hitEnd())
			advance();
		type = TOK_NUM_DEC;
	}
	else
		type = TOK_NUM;

	makeToken(type);
}

void Lexer::stringToken()
{
	while ((peekChar() != '"') && !hitEnd())
	{
		if (peekChar() == '\n')
			// Column value here will not be accurate.
			throw LexError(previousChar(), line, column + 1,
				"Incorrect syntax for multi-line string.");
		advance();
	}

	if (hitEnd())
		throw LexError('\0', line, 0, "Unterminated string."); // Column is irrelevant.

	advance(); // Consume final ".
	makeToken(TOK_STR_LIT);
}

void Lexer::multiStringToken()
{
	// Before processing the quote.
	ui16 tempLine = line;
	ui8 tempColumn = column - 1; // Step back across the opening `.
	
	while ((peekChar() != '`') && !hitEnd())
		advance();

	if (hitEnd())
		throw LexError('\0', line, 0, // Column is irrelevant.
			"Unterminated multi-line string.");

	advance(); // Consume final `.

	stream.emplace_back(TOK_STR_LIT, code.substr(start, current - start),
		Value(), tempLine, tempColumn);
}

TokenType Lexer::identifierType()
{
	std::string_view text = code.substr(start, current - start);
	auto it = keywords.find(text);
	if (it != keywords.end())
	{
		if (it->second == TOK_CLASS)
			state.inClass = true;
		return it->second;
	}
	return TOK_IDENTIFIER;
}

void Lexer::identifierToken()
{
	char c = peekChar();
	while ((isalnum(c) || c == '_') && !hitEnd())
	{
		advance();
		c = peekChar();
	}
	makeToken(identifierType());
}

bool Lexer::matchSequence(char c, int length)
{
	for (int i = 0; i < length; i++)
	{
		if (peekChar(i) != c)
			return false;
	}

	return true;
}

bool Lexer::checkHyperComment()
{
	// Check for hyper-comment.
	if (matchSequence('#', 2)) // We already consumed one.
	{
		// Skip the remaining ##.
		advance(); advance();

		while (!matchSequence('#', 3) && !hitEnd())
			advance();

		// Check for closing ###.
		if (matchSequence('#', 3))
			consumeChars('#', 3);
		else
			throw LexError(peekChar(), line, column + 1,
				"Unterminated nested comment.");
		return true;
	}
	else
		return false;
}

void Lexer::singleToken()
{
	char c = advance();
	
	switch (c)
	{
		case '[': makeToken(TOK_LEFT_BRACKET);	break;
		case ']': makeToken(TOK_RIGHT_BRACKET);	break;
		case '(': makeToken(TOK_LEFT_PAREN);	break;
		case ')': makeToken(TOK_RIGHT_PAREN);	break;
		case '{':
		{
			if (state.inClass)
				state.braceCount++;
			makeToken(TOK_LEFT_BRACE);
			break;
		}
		case '}':
		{
			if (state.inClass)
			{
				state.braceCount--;
				if (state.braceCount == 0)
					state.inClass = false;
			}
			makeToken(TOK_RIGHT_BRACE);
			break;
		}
		case ';': makeToken(TOK_SEMICOLON);	break;
		case ',': makeToken(TOK_COMMA);		break;

		case '+': makeToken(TOK_PLUS);		break;
		case '-':
		{
			if (consumeChar('>'))
				makeToken(TOK_RARROW);
			else
				makeToken(TOK_MINUS);
			break;
		}
		case '*':
		{
			if (consumeChar('*'))
				makeToken(TOK_STAR_STAR);
			else
				makeToken(TOK_STAR);
			break;
		}
		case '/':
		{
			if (consumeChar('/'))
			{
				while ((peekChar() != '\n') && !hitEnd())
					advance();
			}
			else
				makeToken(TOK_SLASH);
			break;
		}
		case '%': makeToken(TOK_PERCENT);	break;

		case '^': makeToken(TOK_UARROW);	break;
		case '~': makeToken(TOK_TILDE);		break;
		case ':': makeToken(TOK_COLON);		break;
		case '.': makeToken(TOK_DOT);		break;
		case '?': makeToken(TOK_QMARK);		break;

		case '=':
		{
			if (consumeChar('='))
				makeToken(TOK_EQ_EQ);
			else
				makeToken(TOK_EQUAL);
			break;
		}
		case '!':
		{
			if (consumeChar('='))
				makeToken(TOK_BANG_EQ);
			else
				makeToken(TOK_BANG);
			break;
		}
		case '>':
		{
			if (consumeChar('>'))
				makeToken(TOK_RIGHT_SHIFT);
			else if (consumeChar('='))
				makeToken(TOK_GT_EQ);
			else
				makeToken(TOK_GT);
			break;
		}
		case '<':
		{
			if (consumeChar('<'))
				makeToken(TOK_LEFT_SHIFT);
			else if (consumeChar('='))
				makeToken(TOK_LT_EQ);
			else
				makeToken(TOK_LT);
			break;
		}

		case '&':
		{
			if (consumeChar('&'))
				makeToken(TOK_AMP_AMP);
			else
				makeToken(TOK_AMP);
			break;
		}
		case '|':
		{
			if (consumeChar('|'))
				makeToken(TOK_BAR_BAR);
			else
				makeToken(TOK_BAR);
			break;
		}
		
		// Strings.

		case '"':
			stringToken();
			break;
		
		case '`':
			multiStringToken();
			break;

		// Whitespace.
		
		case ' ':
		case '\r':
			break;
		case '\n':
		{
			// if (!inGrouping)
				// makeToken(TOK_NEWLINE);
			break;
		}
		case '\t':
			column += TAB_SIZE - 1; // Open to change.
			break;

		// Multi-line comment.

		case '#':
		{
			if (checkHyperComment())
				break;

			while ((peekChar() != '#') && !hitEnd())
				advance();
			if (hitEnd())
				throw LexError('\0', line, 0, "Unterminated comment.");
			advance();
			break;
		}

		case '_':
		{
			if (state.inClass && consumeChar('_'))
			{
				makeToken(TOK_UNDER_UNDER);
				break;
			}
			FALLTHROUGH();
			// No break since we interpret it
			// as the first character in an
			// identifier instead.
		}

		default:
		{
			if (isdigit(c))
				numToken();
			else if (isalpha(c) || c == '_')
				identifierToken();
			else
				// Column has been incremented, so we subtract 1.
				throw LexError(c, line, column - 1, "Unrecognized token.");
		}
	}
}

std::vector<Token>& Lexer::tokenize(std::string_view code)
{
	this->code = code;
	
	// Consider placing the try-catch block inside
	// the while-loop so that the lexer continues to
	// try scanning the source code instead of halting
	// at the first error.
	
	try
	{
		while (!hitEnd())
		{
			start = current;
			singleToken();
		}
	}
	catch (LexError& error)
	{
		error.report();
	}
	stream.emplace_back(); // Default is EOF token.
	return stream;
}