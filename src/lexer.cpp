#include "../include/lexer.h"
#include "../include/error.h"
#include "../include/object.h"
#include "../include/token.h"
#include "../include/utils.h"
#include <cctype>
#include <cstring>
#include <string>
#include <unordered_map>
using namespace Object;

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

Value Lexer::intSizedValue(std::string_view text)
{
	if (ends_with(text, "8"))
	{
		text = text.substr(0, text.size() - 3);
		return NumLiteral(8, static_cast<i8>(
				std::stoi(std::string(text)))
			);
	}

	else if (ends_with(text, "16"))
	{
		text = text.substr(0, text.size() - 4);
		return NumLiteral(16, static_cast<i16>(
				std::stoi(std::string(text)))
			);
	}

	else if (ends_with(text, "32"))
	{
		text = text.substr(0, text.size() - 4);
		return NumLiteral(32, static_cast<i32>(
				std::stol(std::string(text)))
			);
	}

	else if (ends_with(text, "64"))
	{
		text = text.substr(0, text.size() - 4);
		return NumLiteral(64, static_cast<i64>(
				std::stoll(std::string(text)))
			);
	}

	return nullptr; // Unreachable.
}

Value Lexer::uIntSizedValue(std::string_view text)
{
	if (ends_with(text, "_u"))
	{
		text = text.substr(0, text.size() - 2);
		return NumLiteral(0, static_cast<uint_type>(
				std::stoull(std::string(text))
			));
	}
	
	else if (ends_with(text, "8"))
	{
		text = text.substr(0, text.size() - 3);
		return NumLiteral(8, static_cast<ui8>(
				std::stoi(std::string(text))
			));
	}

	else if (ends_with(text, "16"))
	{
		text = text.substr(0, text.size() - 4);
		return NumLiteral(16, static_cast<ui16>(
				std::stoi(std::string(text))
			));
	}

	else if (ends_with(text, "32"))
	{
		text = text.substr(0, text.size() - 4);
		return NumLiteral(32, static_cast<ui32>(
				std::stoul(std::string(text))
			));
	}

	else if (ends_with(text, "64"))
	{
		text = text.substr(0, text.size() - 4);
		return NumLiteral(64, static_cast<ui64>(
				std::stoull(std::string(text))
			));
	}

	return nullptr; // Unreachable.
}

Value Lexer::intValue(const std::string_view& text)
{	
	return NumLiteral(0, static_cast<int_type>(
			std::stoll(std::string(text))
		));
}

Value Lexer::decValue(std::string_view text)
{
	if (ends_with(text, "32"))
	{
		text = text.substr(0, text.size() - 4);
		return NumLiteral(32, std::stof(std::string(text)));
	}

	else if (ends_with(text, "64"))
	{
		text = text.substr(0, text.size() - 4);
		return NumLiteral(64, std::stod(std::string(text)));
	}
	
	return NumLiteral(0, static_cast<dec_type>(
			std::stod(std::string(text))
		));
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
			case TOK_NUM_S:		value = intSizedValue(text);	break;
			case TOK_NUM_U:
			case TOK_NUM_US:
				value = uIntSizedValue(text);
				break;
			case TOK_NUM_DEC:
			case TOK_NUM_DEC_S:
				value = decValue(text);
				break;
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
	
	stream.emplace_back(type, text, value, line, 
		column - (current - start));
}

bool Lexer::matchString(const std::string_view& str)
{
	int i = 0;
	for (char c : str)
	{
		if (peekChar(i) != c)
			return false;
		i++;
	}

	for (size_t i = 0; i < str.size(); i++)
		advance();

	return true;
}

bool Lexer::matchStrings(const std::vector<std::string_view>& strs)
{
	for (std::string_view sv : strs)
	{
		if (matchString(sv))
		return true;
	}

	return false;
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
					if (matchStrings({"8", "16", "32", "64"}))
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
					if (matchStrings({"8", "16", "32", "64"}))
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
			if (matchStrings({"32", "64"}))
				makeToken(TOK_NUM_DEC_S);
			else
				throw LexError(peekChar(), line, column,
					"Invalid floating-point value size.");
		}
		else
			throw LexError(peekChar(), line, column,
				"Must specify floating-point size using '_d' modifier.");
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
	int tempLine = line;
	int tempColumn = column - 1; // Step back across the opening `.
	
	while ((peekChar() != '`') && !hitEnd())
		advance();

	if (hitEnd())
		throw LexError('\0', line, 0, // Column is irrelevant.
			"Unterminated multi-line string.");

	advance(); // Consume final `.
	stream.emplace_back(TOK_STR_LIT, 
						code.substr(start, current - start),
						code.substr(start, current - start),
						tempLine, tempColumn);
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
		case '[': makeToken(TOK_LEFT_BRACKET); break;
		case ']': makeToken(TOK_RIGHT_BRACKET); break;
		case '(': makeToken(TOK_LEFT_PAREN); break;
		case ')': makeToken(TOK_RIGHT_PAREN); break;
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
		case ';': makeToken(TOK_SEMICOLON); break;
		case ',': makeToken(TOK_COMMA); break;

		case '+': makeToken(TOK_PLUS); break;
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
		case '%': makeToken(TOK_PERCENT); break;

		case '^': makeToken(TOK_UARROW); break;
		case '~': makeToken(TOK_TILDE); break;
		case ':': makeToken(TOK_COLON); break;
		case '.': makeToken(TOK_DOT); break;

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
				throw LexError(peekChar(), line, column,
					"Unterminated comment.");
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
			[[fallthrough]];
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