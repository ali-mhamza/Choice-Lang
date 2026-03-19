#include "../include/lexer.h"
#include "../include/common.h"
#include "../include/error.h"
#include "../include/object.h"
#include "../include/token.h"
#include <cctype>
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
	{"not", TOK_NOT},		{"return", TOK_RETURN},	{"new", TOK_NEW},
	{"def", TOK_DEF},		{"fields", TOK_FIELDS},	{"in", TOK_IN}
};

Lexer::Lexer() :
	start(nullptr), current(nullptr), end(nullptr),
	line(1), column(1), state({false, 0}), hitError(false),
	errorCount(0) {}

inline void Lexer::setUp(const std::string_view& code)
{
	start = code.data();
	current = start;
	end = start + code.size();

	line = 1;
	column = 1;
	state = {false, 0};
	hitError = false;

	stream.clear();
	stream.reserve(code.size() / AVG_TOK_SIZE);
}

inline bool Lexer::hitEnd()
{
	return (current >= end);
}

inline char Lexer::advance()
{
	if (!hitEnd())
	{
		current++;
		if (current[-1] == '\n')
		{
			line++;
			column = 1;
		}
		else
			column++;
		return current[-1];
	}

	return EOF;
}

inline bool Lexer::checkChar(char c)
{
	if (!hitEnd())
		return (*current == c);
	return false;
}

inline bool Lexer::consumeChar(char c)
{
	if (checkChar(c))
	{
		advance();
		return true;
	}

	return false;
}

inline void Lexer::consumeChars(int count /* = 1 */)
{
	for (int i = 0; i < count; i++)
		advance();
}

inline char Lexer::peekChar(int distance /* = 0 */)
{
	if (current + distance < end)
		return current[distance];
	return EOF;
}

inline char Lexer::previousChar(int distance /* = 0 */)
{
	if (current - distance - 1 > start)
		return current[- distance - 1];
	return EOF;
}

i64 Lexer::intValue(std::string_view text)
{
	i64 ret = 0;
	for (char c : text)
	{
		if (isdigit(c))
			ret = (ret * 10) + (c - '0');
		else if (c != '\'')
			break;
	}
	
	return ret;
}

double Lexer::decValue(std::string_view text)
{
	double ret = 0;
	auto it = text.begin();
	auto end = text.end();
	for (; it < end; it++)
	{
		char c = *it;
		if (isdigit(c))
			ret = (ret * 10) + (c - '0');
		else if (c != '\'')
			break;
	}

	it++; // Skip the '.'.

	double div = (double) 1 / 10;
	for (; it < end; it++)
	{
		char c = *it;
		ret += (c - '0') * div;
		div /= 10;
	}
	
	return ret;
}

inline bool Lexer::boolValue(TokenType type)
{
	return (type == TOK_TRUE);
}

void Lexer::makeToken(TokenType type)
{
	std::string_view text(start, current - start);

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
		column - static_cast<ui8>(current - start));
}

void Lexer::rangeToken()
{
	if (!isdigit(peekChar()))
		REPORT_ERROR(peekChar(), line, column + 1, // Check column value here.
			"Expecting range-end value after '..'.");

	while ((isdigit(peekChar()) || peekChar() == '\'') && !hitEnd())
		advance();

	if (matchSequence('.', 2))
	{
		consumeChars(2);
		if (!isdigit(peekChar()))
			REPORT_ERROR(peekChar(), line, column + 1, // Check column value here.
				"Expecting skip value after '..'.");

		while ((isdigit(peekChar()) || peekChar() == '\'') && !hitEnd())
			advance();
	}

	makeToken(TOK_RANGE);
}

void Lexer::numToken()
{
	TokenType type;	
	while ((isdigit(peekChar()) || peekChar() == '\'') && !hitEnd())
		advance();

	if (consumeChar('.'))
	{
		if (consumeChar('.'))
		{
			rangeToken();
			return;
		}
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
			REPORT_ERROR(previousChar(), line, column + 1,
				"Incorrect syntax for multi-line string.");
		advance();
	}

	if (hitEnd())
		REPORT_ERROR(EOF, line, 0, "Unterminated string."); // Column is irrelevant.

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
		REPORT_ERROR(EOF, line, 0, // Column is irrelevant.
			"Unterminated multi-line string.");

	advance(); // Consume final `.

	stream.emplace_back(TOK_STR_LIT, std::string_view(start, current - start),
		Value(), tempLine, tempColumn);
}

TokenType Lexer::identifierType()
{	
	if (current - start < 2)
		return TOK_IDENTIFIER;
	
	std::string_view text(start, current - start);
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
			consumeChars(3);
		else
		{
			// Must report manually since we return a value below.
			hitError = true;
			if (errorCount < COMPILE_ERROR_MAX)
			{
				LexError(peekChar(), line, column + 1,
					"Unterminated nested comment.").report();
			}
			else if (errorCount == COMPILE_ERROR_MAX)
				CH_PRINT("SCANNING ERROR MAXIMUM REACHED.\n");
			errorCount++;
		}
		return true;
	}
	else
		return false;
}

inline void Lexer::conditionalToken(char c, TokenType two, TokenType one)
{
	if (consumeChar(c))
		makeToken(two);
	else
		makeToken(one);
}

void Lexer::singleToken()
{
	start = current;
	char c = advance();
	
	switch (c)
	{
		case '[': makeToken(TOK_LEFT_BRACKET);	break;
		case ']': makeToken(TOK_RIGHT_BRACKET);	break;
		case '(': makeToken(TOK_LEFT_PAREN);	break;
		case ')': makeToken(TOK_RIGHT_PAREN);	break;
		case '{':
		{
			if (state.inClass) state.braceCount++;
			makeToken(TOK_LEFT_BRACE);
			break;
		}
		case '}':
		{
			if (state.inClass)
			{
				state.braceCount--;
				state.inClass = !(state.braceCount == 0);
			}
			makeToken(TOK_RIGHT_BRACE);
			break;
		}
		case ';':	makeToken(TOK_SEMICOLON);	break;
		case ',':	makeToken(TOK_COMMA);		break;
		case ':':	makeToken(TOK_COLON);		break;
		case '.':	makeToken(TOK_DOT);			break;
		case '?':	makeToken(TOK_QMARK);		break;

		case '+':
		{
			if (consumeChar('+'))
				makeToken(TOK_INCR);
			else
				conditionalToken('=', TOK_PLUS_EQ, TOK_PLUS);
			break;
		}
		case '-':
		{
			if (consumeChar('-'))
				makeToken(TOK_DECR);
			else if (consumeChar('='))
				makeToken(TOK_MINUS_EQ);
			else
				conditionalToken('>', TOK_RARROW, TOK_MINUS);
			break;
		}
		case '*':
		{
			if (consumeChar('*'))
				conditionalToken('=', TOK_STAR_STAR_EQ, TOK_STAR_STAR);
			else
				conditionalToken('=', TOK_STAR_EQ, TOK_STAR);
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
				conditionalToken('=', TOK_SLASH_EQ, TOK_SLASH);
			break;
		}
		case '%':	conditionalToken('=', TOK_PERCENT_EQ, TOK_PERCENT);	break;
		case '^':	conditionalToken('=', TOK_UARROW_EQ, TOK_UARROW);	break;
		case '~':	conditionalToken('=', TOK_TILDE_EQ, TOK_TILDE);		break;
		case '=':	conditionalToken('=', TOK_EQ_EQ, TOK_EQUAL);		break;
		case '!':	conditionalToken('=', TOK_BANG_EQ, TOK_BANG);		break;
		case '>':
		{
			if (consumeChar('>'))
				conditionalToken('=', TOK_RSHIFT_EQ, TOK_RIGHT_SHIFT);
			else
				conditionalToken('=', TOK_GT_EQ, TOK_GT);
			break;
		}
		case '<':
		{
			if (consumeChar('<'))
				conditionalToken('=', TOK_LSHIFT_EQ, TOK_LEFT_SHIFT);
			else
				conditionalToken('=', TOK_LT_EQ, TOK_LT);
			break;
		}

		case '&':
		{
			if (consumeChar('&'))
				makeToken(TOK_AMP_AMP);
			else
				conditionalToken('=', TOK_AMP_EQ, TOK_AMP);
			break;
		}
		case '|':
		{
			if (consumeChar('|'))
				makeToken(TOK_BAR_BAR);
			else
				conditionalToken('=', TOK_BAR_EQ, TOK_BAR);
			break;
		}
		
		// Strings.

		case '"':	stringToken();		break;
		case '`':	multiStringToken();	break;

		// Whitespace.
		
		case ' ':
		case '\r':
		case '\n':
			break;
		// Open to change.
		case '\t':	column += TAB_SIZE - 1; break;

		// Multi-line comment.

		case '#':
		{
			if (checkHyperComment())
				break;

			while ((peekChar() != '#') && !hitEnd())
				advance();
			if (hitEnd())
				REPORT_ERROR(EOF, line, 0, "Unterminated comment.");
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
			CH_FALLTHROUGH();
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
				REPORT_ERROR(c, line, column - 1, "Unrecognized token.");
		}
	}
}

vT& Lexer::tokenize(std::string_view code)
{
	setUp(code);
	while (!hitEnd())
		singleToken();
	if (hitError)
		stream.clear();
	else
		stream.emplace_back(); // Default is EOF token.
	return stream;
}