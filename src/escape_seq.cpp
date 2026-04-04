#include "../include/escape_seq.h"
#include "../include/common.h"
#include "../include/error.h"
#include <array>
#include <string>
#include <string_view>

// Set to 1 if missing arguments for \b, \o,
// or \x in a string should be an error.
// If set to 0, missing arguments will not translate
// these escape sequences (i.e., "\x" -> "\x", no change).
#define MISSING_ARG_ERROR   1

// For unicode.

constexpr ui32 oneByteMax{0x7f};
constexpr ui32 twoByteMax{0x7ff};
constexpr ui32 threeByteMax{0xffff};
constexpr ui32 fourByteMax{0x10ffff};

// Surrogate range for UTF-16.
// No valid UTF-8 value in this range (inclusive).
constexpr ui32 surrogateRangeStart{0xd800};
constexpr ui32 surrogateRangeEnd{0xdfff};

constexpr ui8 commonByteStart{1 << 7};
constexpr ui8 lastSixBits{0x3f};
constexpr ui8 remainingBits{0xff};

struct NumParseRules
{
    char escape{};
    int maxDigits{}, shift{};
    bool (*check)(char);
    ui8 (*convert)(char);
};

static inline bool isBinary(char c)
{
    return ((c == '0') || (c == '1'));
}

static inline bool isOctal(char c)
{
    return ((c >= '0') && (c <= '7'));
}

static inline bool isHex(char c)
{
    return isxdigit(c);
}

static inline ui8 fromBinary(char c)
{
    return (c - '0');
}

static inline ui8 fromOctal(char c)
{
    return (c - '0');
}

static inline ui8 fromHex(char c)
{
    if (isdigit(c)) return (c - '0');
    if (isupper(c)) return (c - 'A' + 10);
    return (c - 'a' + 10);
}

static inline ui32 strToHex(const svIter it, int count)
{
	constexpr ui32 hexShift{4};
    ui32 result{0};
	for (int i{0}; i < count; i++)
		result = (result << hexShift) + fromHex(it[i]);

	return result;
}

// Assume value in valid UTF-8 range.
static void encodeUTF8(std::string& str, ui32 value)
{
    constexpr ui32 utf8EncodeMax{4};
    constexpr ui8 bitShiftMax{7};

    std::array<char, utf8EncodeMax> buffer{0};
	ui8 numBytes{};
	if (value > threeByteMax)       numBytes = 4;
	else if (value > twoByteMax)    numBytes = 3;
	else if (value > oneByteMax)    numBytes = 2;
	else
	{
		str.push_back(static_cast<char>(value));
		return;
	}

	for (ui8 i{static_cast<ui8>(numBytes - 1)}; i > 0; i--)
	{
		buffer[i] |= commonByteStart | (value & lastSixBits);
		value >>= 6;
	}

	ui8 start{0};
	for (ui8 i{0}; i < numBytes; i++)
		start |= (1 << (bitShiftMax - i));

	buffer[0] |= start | (value & remainingBits);
    str.append(buffer.data(), numBytes);
}

static bool setError(std::string& error, const std::string& msg)
{
    if (error.empty()) error = msg;
    return false;
}

constexpr NumParseRules BinaryRules{
    'b', 8, 1,  // 8 digits allowed, shift by 1 bit.
    isBinary, fromBinary
};

constexpr NumParseRules OctalRules{
    'o', 3, 3, // 3 digits allowed, shift by 3 bits.
    isOctal, fromOctal
};

constexpr NumParseRules HexRules{
    'x', 2, 4, // 2 digits allowed, shift by 4 bits.
    isHex, fromHex
};

bool parseCharSequence(
    std::string& str,
    svIter& it,
    svIter end
)
{
    // In case previous functions modify the iterator.
    if (it >= end - 1) return false; 

    char c{};
    switch (it[1])
	{
		case 'n':	c = '\n';   break;
		case 't':	c = '\t';   break;
		case 'r':	c = '\r';   break;
		case '\\':	c = '\\';   break;
		case '"':	c = '"';    break;
		default:	return false;
	}

    str.push_back(c);
    it += 2;
    return true;
}

static bool checkParseArgs(
    const svIter it,
    const svIter end,
    const NumParseRules& rules,
    std::string& errorMsg
)
{
    if ((it >= end) || !rules.check(*it))
    {
        #if MISSING_ARG_ERROR
        if (errorMsg.empty())
        {
            errorMsg = CH_STR("Missing arguments for '\\{}' escape character.",
                rules.escape);
        }
        #endif

        return false;
    }

    return true;
}

static ui32 parseEscapeString(
    svIter& it,
    const svIter end,
    const NumParseRules& rules
)
{
    ui32 replace{0};
    int count{0};
    while ((it < end) && rules.check(*it) && (count < rules.maxDigits))
    {
        replace = (replace << rules.shift) + rules.convert(*it);
        count++;
        it++;
    }

    return replace;
}

bool parseNumericSequence(
    std::string& str,
    svIter& it,
    svIter end,
    std::string& errorMsg
)
{
    if (it >= end - 1) return false;    

    NumParseRules rules{};
    switch (it[1])
    {
        case 'b':   rules = BinaryRules;    break;
        case 'o':   rules = OctalRules;     break;
        case 'x':   rules = HexRules;       break;
        default:    return false;
    }    

    it += 2;
    if (!checkParseArgs(it, end, rules, errorMsg))
        return false;

    ui32 replace{parseEscapeString(it, end, rules)};
    if ((rules.escape == 'o') // Check only for octal.
        && (replace > std::numeric_limits<ui8>::max()))
    {
        if (errorMsg.empty()) errorMsg = "Octal escape value too large.";
        return false;
    }

    str.push_back(static_cast<char>(replace));
    return true;
}

static int consumeUnicodeSequence(
    svIter& it,
    svIter end,
    std::string& errorMsg
)
{
    int count{0};
    while (it + count != end)
    {
        if (!isHex(it[count]))
        {
            if (it[count] != '}')
            {
                setError(errorMsg, "Invalid hex digit in codepoint.");
                return -1;
            }

            break;
        }
        if (count++ == 6)
        {
            // More than 6 digits (3).
            it += 6;
            setError(errorMsg,
                "Too many digits for unicode character. Maximum is 6.");
            return -1;
        }
    }

    // Empty braces/no digits (2).
    if (count == 0)
    {
        setError(errorMsg, "Expect hex digits after opening brace for '\\u'.");
        return -1;
    }
    // No closing brace (4).
    if ((it + count == end) || (it[count] != '}'))
    {
        it += count;
        setError(errorMsg, "Expect '}' after unicode sequence.");
        return -1;
    }

    return count;
}

bool parseUnicodeSequence(
    std::string& str,
    svIter& it,
    const svIter end,
    std::string& errorMsg
)
{
    if ((it >= end - 1) || (it[1] != 'u')) // Only lowercase 'u' for now.
        return false;

    // Error cases:
    // 1. No braces.
    // 2. Empty braces/no digits between braces.
    // 3. More than 6 hex digits between braces.
    // 4. No closing brace.
    // 5. Result outside unicode range.

    it += 2; // Skip \ and 'u'.
    // No braces (1).
    if ((it == end) || (*it != '{'))
        return setError(errorMsg, "Expect '{' after '\\u'.");

    // Checks 2-4.
    it++; // Skip the {.
    int count{consumeUnicodeSequence(it, end, errorMsg)};
    if (count == -1) return false;

    ui32 value{strToHex(it, count)};
    it += count + 1; // Skip characters and closing brace.
    // Result outside unicode range.
    if ((value > fourByteMax)
        || ((value >= surrogateRangeStart) && (value <= surrogateRangeEnd)))
    {
        return setError(errorMsg, "Codepoint value outside valid UTF-8 range.");
    }

    encodeUTF8(str, value);
    return true;
}