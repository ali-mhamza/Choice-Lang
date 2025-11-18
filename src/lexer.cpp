#include "../include/lexer.h"
#include "../include/error.h"
#include "../include/token.h"
#include <cctype>
#include <string>
#include <unordered_map>

static std::unordered_map<std::string_view, TokenType> keywords = {
	{"int",     TOK_INT},
	{"u_int",	TOK_UINT},
	{"dec",     TOK_DEC},
	{"boolean", TOK_BOOL},
	{"string",  TOK_STRING},
	{"func",	TOK_FUNC},
	{"array",	TOK_ARRAY},
	{"table",	TOK_TABLE},
	{"class",	TOK_CLASS},
	{"any",		TOK_ANY},
	{"make",    TOK_MAKE},
	{"fix",     TOK_FIX},
	{"true",    TOK_TRUE},
	{"false",   TOK_FALSE},
	{"null",    TOK_NULL},
	{"and",     TOK_AND},
	{"or",      TOK_OR},
	{"new",		TOK_NEW}
};

Lexer::Lexer() :
	line(1), column(1), start(0), current(0),
	inGrouping(false) {}

bool Lexer::hitEnd()
{
	return (current >= code.length());
}

char Lexer::advance()
{
	if (!hitEnd())
	{
		current++;
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

char Lexer::peekChar(int distance /* = 1 */)
{
	if (current + distance < code.length())
		return code[current + distance];
	return EOF;
}

char Lexer::previousChar(int distance /* = 1 */)
{
	if (current - distance > 1)
		return code[current - distance - 1];
	return EOF;
}

void Lexer::updateColumn()
{
	std::string_view lastTokText = stream.back().text;
	column += static_cast<uint8_t>(lastTokText.length());
}

void Lexer::eofToken()
{
	Token token(TOKEN_EOF, "", nullptr, 0, 0); // All sentinel values.
	stream.push_back(token);
}

int Lexer::intValue(const std::string_view& text)
{
	return std::stoi(std::string(text));
}

double Lexer::decValue(const std::string_view& text)
{
	return std::stod(std::string(text));
}

std::string_view Lexer::stringValue(const std::string_view& text)
{
	// -2 to cut off the quote marks.
	return text.substr(1, text.length() - 2);
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
			case TOK_NUM_INT: value = intValue(text); break;
			case TOK_NUM_DEC: value = decValue(text); break;
			case TOK_STR_LIT: value = stringValue(text); break;
			case TOK_TRUE:
			case TOK_FALSE:
				value = boolValue(type);
				break;
			case TOK_NULL:
				value = "NULL";
				break;
			default:
				break;
		}
	}
	
	Token token(type, text, value, line, column);
	stream.push_back(token);
}

void Lexer::charToken(TokenType type, int length /* = 1*/)
{
	makeToken(type);
	column += length;
}

void Lexer::numToken()
{
	while (isdigit(peekChar()) && !hitEnd())
		advance();
	if (consumeChar('.'))
	{
		while (isdigit(peekChar()) && !hitEnd())
			advance();
		makeToken(TOK_NUM_DEC);
	}
	else
		makeToken(TOK_NUM_INT);
	updateColumn();
}

void Lexer::stringToken()
{
	while ((peekChar() != '"') && !hitEnd())
	{
		if (peekChar() == '\n')
			throw LexError(); // Unsupported multi-line string.
		advance();
	}
	if (hitEnd())
		throw LexError(); // Handle unterminated string.
	advance(); // Consume final ".
	makeToken(TOK_STR_LIT);
	updateColumn();
}

void Lexer::multiStringToken()
{
	// Before processing the quote.
	int tempLine = line;
	int tempColumn = column;
	bool hitNewline = false;
	
	while ((peekChar() != '`') && !hitEnd())
	{
		advance();
		if (previousChar() == '\n')
		{
			line++;
			column = 1;
			hitNewline = true;
		}
		else
			column++;
	}

	if (hitEnd())
		throw LexError(); // Handle unterminated string.
	advance(); // Consume final `.
	column++; // To account for final `.
	
	// To restore both afterwards.
	int diffLine = line - tempLine;
	int prevColumn = column; // After processing the quote.

	// Set up before adding the token.
	line = tempLine;
	column = tempColumn;
	makeToken(TOK_STR_LIT);

	// Restoring values.
	line += diffLine;

	// If the entire "multi-line" string is on
	// a single line, we end up not counting the
	// opening ` character for any following
	// tokens, so we add 1 to the column before
	// scanning any new tokens if there was no
	// newline in the string.
	column = prevColumn + (hitNewline ? 0 : 1);
}

TokenType Lexer::identifierType()
{
	std::string_view text = code.substr(start, current - start);
	auto it = keywords.find(text);
	if (it != keywords.end())
		return it->second;
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
	updateColumn();
}

bool Lexer::matchSequence(char c, int length, bool forward /* = true */)
{
	for (int i = 0; i < length; i++)
	{
		if (forward)
		{
			if (peekChar(i) != c)
				return false;
		}
		else
		{
			if (previousChar(i) != c)
				return false;
		}
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
		// We need to advance the column in case
		// there is code on the same line as the
		// closing ###.
		column += 2;
		while (!matchSequence('#', 3) && !hitEnd())
		{
			advance();
			if (previousChar() == '\n')
			{
				line++;
				column = 1;
			}
			else
				column++;
		}

		// Check for closing ###.
		if (matchSequence('#', 3))
		{
			consumeChars('#', 3);
			column += 3;
		}
		else
			throw LexError(); // Handle unterminated hyper-comment.
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
		// Single character tokens.
		
		case '[': charToken(TOK_LEFT_BRACKET); break;
		case ']': charToken(TOK_RIGHT_BRACKET); break;
		case '(': charToken(TOK_LEFT_PAREN); break;
		case ')': charToken(TOK_RIGHT_PAREN); break;
		case '{': charToken(TOK_LEFT_BRACE); break;
		case '}': charToken(TOK_RIGHT_BRACE); break;
		case ';': charToken(TOK_SEMICOLON); break;
		case ',': charToken(TOK_COMMA); break;

		case '+': charToken(TOK_PLUS); break;
		case '-': charToken(TOK_MINUS); break;
		case '*': charToken(TOK_STAR); break;
		case '/':
		{
			if (consumeChar('/'))
			{
				while ((peekChar() != '\n') && !hitEnd())
					advance();
			}
			else
				charToken(TOK_SLASH);
			break;
		}
		case '%': charToken(TOK_PERCENT); break;

		case '^': charToken(TOK_UARROW); break;
		case '~': charToken(TOK_TILDE); break;
		case ':': charToken(TOK_COLON); break;
		case '!': charToken(TOK_BANG); break;
		case '.': charToken(TOK_DOT); break;

		// Two-character tokens.

		case '=':
		{
			if (consumeChar('='))
				charToken(TOK_EQ_EQ, 2);
			else
				charToken(TOK_EQUAL);
			break;
		}
		case '>':
		{
			if (consumeChar('>'))
				charToken(TOK_RIGHT_SHIFT, 2);
			else if (consumeChar('='))
				charToken(TOK_GT_EQ, 2);
			else
				charToken(TOK_GT);
			break;
		}
		case '<':
		{
			if (consumeChar('<'))
				charToken(TOK_LEFT_SHIFT, 2);
			else if (consumeChar('='))
				charToken(TOK_LT_EQ, 2);
			else
				charToken(TOK_LT);
			break;
		}

		case '&':
		{
			if (consumeChar('&'))
				charToken(TOK_AMP_AMP, 2);
			else
				charToken(TOK_AMP);
			break;
		}
		case '|':
		{
			if (consumeChar('|'))
				charToken(TOK_BAR_BAR, 2);
			else
				charToken(TOK_BAR);
		}
		
		// Strings.

		case '"':
			stringToken();
			break;
		
		case '`':
			multiStringToken();
			break;

		// Whitespace.
		
		case '\n':
		{
			// if (!inGrouping)
				charToken(TOK_NEWLINE);
			line++;
			column = 1;
			break;
		}
		case '\t':
			column += TAB_SIZE; // Open to change.
			break;
		case ' ':
		case '\r':
			column++;
			break;

		// Multi-line comment.

		case '#':
		{
			if (checkHyperComment())
				break;

			while ((peekChar() != '#') && !hitEnd())
			{
				advance();
				// We need to advance the column in case
				// there is code on the same line as the
				// closing #.
				column++;
				if (previousChar() == '\n')
				{
					column = 1;
					line++;
				}
			}
			if (hitEnd())
				throw LexError(); // Handle unterminated comment.
			advance();
			column++;
			break;
		}

		default:
		{
			if (isdigit(c))
				numToken();
			else if (isalpha(c) || c == '_')
				identifierToken();
			else
				throw LexError();
		}
	}
}

#include <iostream> // FOR DEBUGGING. REMOVE.
std::vector<Token>& Lexer::tokenize(std::string_view code)
{
	this->code = code;
	column = 1;
	while (!hitEnd())
	{
		start = current;
		try
		{
			singleToken();
		}
		catch (LexError& error)
		{
			std::cout << "HIT ERROR!\n";
		}
	}
	eofToken();
	return stream;
}