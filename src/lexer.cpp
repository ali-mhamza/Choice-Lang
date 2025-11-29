#include "../include/lexer.h"
#include "../include/error.h"
#include "../include/token.h"
#include "../include/utils.h"
#include <cctype>
#include <cstring>
#include <string>
#include <unordered_map>

static std::unordered_map<std::string_view, TokenType> keywords = {
	{"int",     TOK_INT},
	{"uint",	TOK_UINT},
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
	{"new",		TOK_NEW},
	{"def",		TOK_DEF},
	{"fields",	TOK_FIELDS},
	{"in",		TOK_IN}
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

Value Lexer::intSizedValue(std::string_view text)
{
	if (ends_with(text, "8"))
	{
		text = text.substr(0, text.size() - 3);
		return NumLiteral(OBJ_INT, 8, static_cast<int8_t>(
				std::stoi(std::string(text)))
			);
	}

	else if (ends_with(text, "16"))
	{
		text = text.substr(0, text.size() - 4);
		return NumLiteral(OBJ_INT, 16, static_cast<int16_t>(
				std::stoi(std::string(text)))
			);
	}

	else if (ends_with(text, "32"))
	{
		text = text.substr(0, text.size() - 4);
		return NumLiteral(OBJ_INT, 32, static_cast<int32_t>(
				std::stol(std::string(text)))
			);
	}

	else if (ends_with(text, "64"))
	{
		text = text.substr(0, text.size() - 4);
		return NumLiteral(OBJ_INT, 64, static_cast<int64_t>(
				std::stoll(std::string(text)))
			);
	}
}

Value Lexer::uIntSizedValue(std::string_view text)
{
	if (ends_with(text, "_u"))
	{
		text = text.substr(0, text.size() - 2);
		return NumLiteral(OBJ_UINT, 0, static_cast<uint64_t>(
				std::stoull(std::string(text))
			));
	}
	
	else if (ends_with(text, "8"))
	{
		text = text.substr(0, text.size() - 3);
		return NumLiteral(OBJ_UINT, 8, static_cast<uint8_t>(
				std::stoi(std::string(text))
			));
	}

	else if (ends_with(text, "16"))
	{
		text = text.substr(0, text.size() - 4);
		return NumLiteral(OBJ_UINT, 16, static_cast<uint16_t>(
				std::stoi(std::string(text))
			));
	}

	else if (ends_with(text, "32"))
	{
		text = text.substr(0, text.size() - 4);
		return NumLiteral(OBJ_UINT, 32, static_cast<uint32_t>(
				std::stoul(std::string(text))
			));
	}

	else if (ends_with(text, "64"))
	{
		text = text.substr(0, text.size() - 4);
		return NumLiteral(OBJ_UINT, 64, static_cast<uint64_t>(
				std::stoull(std::string(text))
			));
	}
}

Value Lexer::intValue(const std::string_view& text)
{	
	return NumLiteral(OBJ_INT, 0, static_cast<int64_t>(
			std::stoll(std::string(text))
		));
}

Value Lexer::decValue(std::string_view text)
{
	if (ends_with(text, "32"))
	{
		text = text.substr(0, text.size() - 4);
		return NumLiteral(OBJ_DEC, 32, std::stof(std::string(text)));
	}

	else if (ends_with(text, "64"))
	{
		text = text.substr(0, text.size() - 4);
		return NumLiteral(OBJ_DEC, 64, std::stod(std::string(text)));
	}
	
	return NumLiteral(OBJ_DEC, 0, std::stod(std::string(text)));
}

Value Lexer::stringValue(const std::string_view& text)
{
	// -2 to cut off the quote marks.
	return text.substr(1, text.length() - 2);
}

Value Lexer::boolValue(TokenType type)
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
			case TOK_NUM:		value = intValue(text);       	break;
			case TOK_NUM_DEC:	value = decValue(text);       	break;
			case TOK_NUM_S:		value = intSizedValue(text);	break;
			case TOK_NUM_U:
			case TOK_NUM_US:
				value = uIntSizedValue(text);
				break;
			case TOK_NUM_DEC_S:	value = decValue(text);			break;
			case TOK_STR_LIT:	value = stringValue(text);		break;
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
	
	stream.emplace_back(type, text, value, line, column);
}

void Lexer::charToken(TokenType type, int length /* = 1*/)
{
	makeToken(type);
	column += length;
}

bool Lexer::matchString(std::string_view str, bool consume /* = false */)
{
	int i = 0;
	for (char c : str)
	{
		if (peekChar(i) != c)
			return false;
		i++;
	}

	if (consume)
	{
		for (char c : str)
			advance();
	}

	return true;
}

void Lexer::numLiteral(TokenType type)
{
	if (type == TOK_NUM)
	{
		switch (peekChar())
		{
			case 'u':
			{
				advance();
				if (isdigit(peekChar()))
				{
					if (matchString("8", true) || matchString("16", true)
						|| matchString("32", true) || matchString("64", true))
					{
						makeToken(TOK_NUM_US);
						return;
					}
					else
						// Column value may be inaccurate.
						throw LexError(peekChar(), line, column,
							"Invalid unsigned integer size.");
				}
				makeToken(TOK_NUM_U);
				break;
			}

			case 'i':
			{
				advance();
				if (isdigit(peekChar()))
				{
					if (matchString("8", true) || matchString("16", true)
						|| matchString("32", true) || matchString("64", true))
					{
						makeToken(TOK_NUM_S);
						return;
					}
					else
						// Column value may be inaccurate.
						throw LexError(peekChar(), line, column,
							"Invalid signed integer size.");
				}
				else
					throw LexError(peekChar(), line, column,
						"Must specify integer size when using _i modifier.");
			}

			default:
				throw LexError(peekChar(), line, column,
					"Invalid literal modifier for integer value.");
		}
	}

	else if (type == TOK_NUM_DEC)
	{
		if (consumeChar('d'))
		{
			if (matchString("32", true) || matchString("64", true))
				makeToken(TOK_NUM_DEC_S);
			else
				throw LexError(peekChar(), line, column,
					"Invalid floating-point value size.");
		}
		else
			throw LexError(peekChar(), line, column,
				"Must specify floating-point size using _d modifier.");
	}
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
	if (consumeChar('_'))
		numLiteral(type);
	else
		makeToken(type);
	updateColumn();
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
		throw LexError('\0', line, 0, // Column is irrelevant.
			"Unterminated multi-line string.");
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
		// Single character tokens.
		
		case '[': charToken(TOK_LEFT_BRACKET); break;
		case ']': charToken(TOK_RIGHT_BRACKET); break;
		case '(': charToken(TOK_LEFT_PAREN); break;
		case ')': charToken(TOK_RIGHT_PAREN); break;
		case '{':
		{
			if (state.inClass)
				state.braceCount++;
			charToken(TOK_LEFT_BRACE);
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
			charToken(TOK_RIGHT_BRACE);
			break;
		}
		case ';': charToken(TOK_SEMICOLON); break;
		case ',': charToken(TOK_COMMA); break;

		case '+': charToken(TOK_PLUS); break;
		case '-':
		{
			if (consumeChar('>'))
				charToken(TOK_RARROW, 2);
			else
				charToken(TOK_MINUS);
			break;
		}
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
				throw LexError(peekChar(), line, column,
					"Unterminated comment.");
			advance();
			column++;
			break;
		}

		case '_':
		{
			if (state.inClass && consumeChar('_'))
			{
				charToken(TOK_UNDER_UNDER, 2);
				break;
			}
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
				throw LexError(c, line, column, "Unrecognized token.");
		}
	}
}

std::vector<Token>& Lexer::tokenize(std::string_view code)
{
	this->code = code;
	column = 1;
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