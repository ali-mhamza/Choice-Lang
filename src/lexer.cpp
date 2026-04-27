#include "../include/lexer.h"

#include "fast_float.h"

#include "../include/common.h"
#include "../include/config.h"
#include "../include/error.h"
#include "../include/token.h"
#include "../include/utils.h"
#include <cctype>				// For isdigit, isalpha, is alnum.
#include <cstring>				// For strchr.
#include <string>
#include <string_view>
#include <unordered_map>		// For keywords map.

#undef EOF
#define EOF static_cast<char>(-1)

#undef REPORT_RETURN
#define REPORT_RETURN(...)         	\
	do {                        	\
		reportError(__VA_ARGS__);	\
		return;                 	\
	} while (false)

static const std::unordered_map<std::string_view, TokenType> keywords{
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

void Lexer::setUp(const std::string_view& code)
{
	start = code.data();
	current = start;
	end = start + code.size();

	line = 1;
	column = 1;
	hitError = false;

	stream.clear();
	stream.reserve(code.size() / AVG_TOK_SIZE);
}

bool Lexer::hitEnd() const
{
	return (current >= end);
}

char Lexer::advance()
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

bool Lexer::checkChar(char c) const
{
	if (!hitEnd())
		return (*current == c);
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

void Lexer::consumeChars(size_t count /* = 1 */)
{
	for (size_t i{0}; i < count; i++)
		advance();
}

char Lexer::peekChar(size_t distance /* = 0 */) const
{
	if (current + distance < end)
		return current[distance];
	return EOF;
}

char Lexer::previousChar(size_t distance /* = 0 */) const
{
	if (current - distance - 1 > start)
		return *(current - distance - 1);
	return EOF;
}

TokenType Lexer::identifierType()
{	
	if (static_cast<ui8>(current - start) < 2)
		return TOK_IDENTIFIER;

	std::string_view text{start, static_cast<ui8>(current - start)};
	auto it{keywords.find(text)};
	if (it != keywords.end())
		return it->second;

	return TOK_IDENTIFIER;
}

bool Lexer::matchSequence(char c, int length) const
{
	for (int i{0}; i < length; i++)
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
			reportError(peekChar(), line, static_cast<ui8>(column + 1),
				"Unterminated nested comment.");
		}
		return true;
	}
	else
		return false;
}

bool Lexer::checkRawString(char start)
{
	if ((start == 'r') && consumeChar('"'))
	{
		stringToken(true);
		return true;
	}

	if ((start == 'r') && consumeChar('`'))
	{
		multiLineStringToken(true);
		return true;
	}

	return false;
}

bool Lexer::checkNumericLiteral(char start)
{
	if (start != '0') return false;

	if (consumeChar('b'))
	{
		base = BIN;
		numericToken(isBinary);
		base = DEC;
		return true;
	}
	else if (consumeChar('o'))
	{
		base = OCT;
		numericToken(isOctal);
		base = DEC;
		return true;
	}
	else if (consumeChar('x'))
	{
		base = HEX;
		numericToken(isHex);
		base = DEC;
		return true;
	}

	return false;
}

void Lexer::reportError(
	const char c,
	const ui16 line,
	const ui8 position,
	const std::string_view message
)
{
	if (errorCount == LEX_ERROR_MAX)
		CH_PRINT("SCANNING ERROR MAXIMUM REACHED.\n");
	else if (errorCount < LEX_ERROR_MAX)
		LexError{c, line, position, message}.report();
	hitError = true;
	errorCount++;
}

std::string& Lexer::formatNumber(const std::string_view text, bool dec)
{
	ui8 textSize{static_cast<ui8>(text.size())};
	ui8 columnStart{static_cast<ui8>(column - textSize)};
	const char* allowedChars{".+-"};

	static std::string str{};
	str.clear();
	str.reserve(textSize);

	ui8 i{0};
	if (starts_with(text, "0b") || starts_with(text, "0o")
		|| starts_with(text, "0x"))
	{
		i += 2;
	}

	auto isValidChar = [dec](char c) -> bool {
		// Either a digit, or a hex digit.
		// Exception: 'e' suffix in floating-point values.
		return (isdigit(c) || (isHex(c) && !dec));
	};

	for (; i < textSize; i++, columnStart++)
	{
		const char c{text[i]};
		if (isalnum(c) || (strchr(allowedChars, c) != nullptr))
			str.push_back(text[i]);
		else if ((c == '\'') && ((i == 0) || (i == textSize - 1) ||
				!isValidChar(text[i - 1]) || !isValidChar(text[i + 1])))
		{
			reportError(c, line, columnStart, "Invalid use of digit separator.");
		}
	}

	return str;
}

i64 Lexer::intValue(std::string_view text)
{
	i64 ret{};
	int baseValue{};

	switch (base)
	{
		case DEC:	baseValue = 10;	break;
		case BIN:	baseValue = 2;	break;
		case OCT:	baseValue = 8;	break;
		case HEX:	baseValue = 16;	break;
	}

	const std::string& formatted{formatNumber(text, false)};
	auto answer{fast_float::from_chars(formatted.data(),
		formatted.data() + formatted.size(), ret, baseValue)};

	if (!answer)
	{
		reportError(
			text.back(), line, static_cast<ui8>(column - 1),
			"Failed to parse numeric literal."
		);
	}

	return ret;
}

double Lexer::decValue(std::string_view text)
{
	double ret{};

	const std::string& formatted{formatNumber(text, true)};
	auto answer{fast_float::from_chars(formatted.data(),
		formatted.data() + formatted.size(), ret)};

	if (!answer)
	{
		reportError(
			text.back(), line, static_cast<ui8>(column - 1),
			"Failed to parse floating-point literal."
		);
	}

	return ret;
}

bool Lexer::boolValue(TokenType type) const
{
	return (type == TOK_TRUE);
}

void Lexer::makeToken(TokenType type)
{
	using sizeT = std::string_view::size_type;
	std::string_view text{start, static_cast<sizeT>(current - start)};

	Value value{};
	if (IS_LITERAL_TOK(type))
	{
		switch (type)
		{
			case TOK_NUM:		value.i = intValue(text);	break;
			case TOK_NUM_DEC:	value.d = decValue(text);	break;
			case TOK_TRUE:
			case TOK_FALSE:		value.b = boolValue(type);	break;
			case TOK_NULL:		value.s = nullptr;			break;
			// For string literals we use the token's own text later.
			default: break;
		}
	}

	stream.emplace_back(type, text, value, line,
		column - static_cast<ui8>(current - start));
}

void Lexer::numToken()
{
	TokenType type{TOK_NUM};
	while ((isdigit(peekChar()) || (peekChar() == '\'')) && !hitEnd())
		advance();

	if (peekChar() == '.')
	{
		if (peekChar(1) == '.') // Range (1..2), not a decimal (1.2).
		{
			makeToken(type);
			return;
		}

		advance(); // Skip the single '.'.
		while ((isdigit(peekChar()) || (peekChar() == '\'')) && !hitEnd())
			advance();
		type = TOK_NUM_DEC;
	}

	if (consumeChar('e'))
	{
		if (!consumeChar('-') && !consumeChar('+') && !isdigit(peekChar()))
		{
			REPORT_RETURN(peekChar(), line, column,
				"Invalid character for scientific notation.");
		}
		if (!isdigit(peekChar()))
			REPORT_RETURN(peekChar(), line, column, "Expect exponent.");

		while ((isdigit(peekChar()) || (peekChar() == '\'')) && !hitEnd())
			advance();
		type = TOK_NUM_DEC;
	}

	makeToken(type);
}

void Lexer::numericToken(bool (*check)(char))
{
	if (!check(peekChar()))
	{
		std::string_view sv{};
		switch (base)
		{
			case BIN:
				sv = "Expect binary digit after '0b' prefix.";
				break;
			case OCT:
				sv = "Expect octal digit after '0o' prefix.";
				break;
			case HEX:
				sv = "Expect hexadecimal digit after '0x' prefix.";
				break;
			default:
				CH_UNREACHABLE();
		}

		REPORT_RETURN(peekChar(), line, column, sv);
	}

	while ((check(peekChar()) || (peekChar() == '\'')) && !hitEnd())
		advance();
	if (!hitEnd() && isalnum(peekChar()))
	{
		REPORT_RETURN(peekChar(), line, column,
			"Invalid character for numeric literal.");
	}
	makeToken(TOK_NUM);
}

#define FORMAT_PARAM(c) \
	(((c) == '%') && (previousChar() != '\\') && (peekChar(1) == '('))

void Lexer::stringToken(bool raw)
{
	int escapeCharCount{0};
	while (!hitEnd())
	{
		char c{peekChar()};
		if ((c == '"') && (escapeCharCount % 2 == 0))
			break;
		else if (!raw && FORMAT_PARAM(c))
		{
			formatStringToken('"');
			return;
		}
		else if (c == '\n')
		{
			REPORT_RETURN(previousChar(), line, static_cast<ui8>(column + 1),
				"Incorrect syntax for multi-line string.");
		}

		escapeCharCount = ((c == '\\') ? escapeCharCount + 1 : 0);
		advance();
	}

	if (hitEnd())
		REPORT_RETURN(EOF, line, 0, "Unterminated string."); // Column is irrelevant.

	advance(); // Consume final ".
	makeToken(raw ? TOK_RAW_STR : TOK_STR_LIT);
}

void Lexer::multiLineStringToken(bool raw)
{
	// Before processing the quote.
	ui16 tempLine{line};
	// Step back across the opening `.
	ui8 tempColumn{static_cast<ui8>(column - 1)};

	int escapeCharCount{0};
	while (!hitEnd())
	{
		char c{peekChar()};
		if ((c == '`') && (escapeCharCount % 2 == 0))
			break;
		else if (!raw && FORMAT_PARAM(c))
		{
			formatStringToken('`');
			return;
		}

		escapeCharCount = ((c == '\\') ? escapeCharCount + 1 : 0);
		advance();
	}

	if (hitEnd())
	{
		REPORT_RETURN(EOF, line, 0, // Column is irrelevant.
			"Unterminated multi-line string.");
	}

	advance(); // Consume final `.

	stream.emplace_back(
		raw ? TOK_RAW_STR : TOK_STR_LIT,
		std::string_view{start, static_cast<ui8>(current - start)},
		Value{}, tempLine, tempColumn
	);
}

bool Lexer::formatParam()
{
	consumeChars(2); // Skip the %(.
	while ((peekChar() != ')') && !hitEnd())
		singleToken();

	if (hitEnd())
	{
		reportError(EOF, line, -1, "Unterminated string interpolation.");
		return false;
	}

	advance();
	start = current; // Reset before scanning continues.
	return true;
}

void Lexer::formatStringToken(char endDelim)
{
	static ui8 nestingDepth{0};

	nestingDepth++;
	if (nestingDepth > INTERPOLATION_MAX)
	{
		// Not an unrecoverable error, so we don't return yet.
		reportError(peekChar(), line, column + 1,
			"Maximum nesting level reached for string interpolation.");
	}

	makeToken(TOK_INTER_START);
	if (!formatParam()) return;

	nestingDepth--;
	while ((peekChar() != endDelim) && !hitEnd())
	{
		if (FORMAT_PARAM(peekChar()))
		{
			makeToken(TOK_INTER_PART);
			if (!formatParam())
				return;
		}
		else
			advance();
	}

	if (hitEnd())
		REPORT_RETURN(EOF, line, -1, "Unterminated format string.");
	advance();
	makeToken(TOK_INTER_END);
}

#undef FORMAT_PARAM

void Lexer::identifierToken()
{
	char c{peekChar()};
	while ((isalnum(c) || c == '_') && !hitEnd())
	{
		advance();
		c = peekChar();
	}

	makeToken(identifierType());
}

void Lexer::conditionalToken(char c, TokenType two, TokenType one)
{
	if (consumeChar(c))
		makeToken(two);
	else
		makeToken(one);
}

void Lexer::singleToken()
{
	start = current;
	char c{advance()};
	
	switch (c)
	{
		case '[': 	makeToken(TOK_LEFT_BRACKET);	break;
		case ']': 	makeToken(TOK_RIGHT_BRACKET);	break;
		case '(': 	makeToken(TOK_LEFT_PAREN);		break;
		case ')': 	makeToken(TOK_RIGHT_PAREN);		break;
		case '{': 	makeToken(TOK_LEFT_BRACE);		break;
		case '}': 	makeToken(TOK_RIGHT_BRACE);		break;
		case ';':	makeToken(TOK_SEMICOLON);		break;
		case ',':	makeToken(TOK_COMMA);			break;
		case ':':	makeToken(TOK_COLON);			break;
		case '?':	makeToken(TOK_QMARK);			break;

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
		case '.':	conditionalToken('.', TOK_DOT_DOT, TOK_DOT);		break;
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

		// Strings (raw string in default case below).

		case '"':	stringToken(false);				break;
		case '`':	multiLineStringToken(false);	break;

		// Whitespace.

		case ' ':
		case '\n':
			break;
		// Open to change.
		case '\t':
			column += TAB_SIZE - 1;
			break;

		// Multi-line comment.

		case '#':
		{
			if (checkHyperComment())
				break;

			while ((peekChar() != '#') && !hitEnd())
				advance();
			if (hitEnd())
				REPORT_RETURN(EOF, line, 0, "Unterminated comment.");
			advance();
			break;
		}

		default:
		{
			if (checkRawString(c) || checkNumericLiteral(c))
				break;
			else if (isdigit(c))
				numToken();
			else if (isalpha(c) || c == '_')
				identifierToken();
			else
			{
				// Column has been incremented, so we subtract 1.
				REPORT_RETURN(c, line, static_cast<ui8>(column - 1),
					"Unrecognized token.");
			}
		}
	}
}

vT& Lexer::tokenize(const std::string_view code)
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