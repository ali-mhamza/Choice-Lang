#include "../include/escape_seq.h"
#include "../include/common.h"
#include "../include/utils.h"
#include <array>
#include <limits>
#include <string>

// Set to 1 if missing arguments for \b, \o,
// or \x in a string should be an error.
// If set to 0, missing arguments will not translate
// these escape sequences (i.e., "\x" -> "\x", no change).
#define MISSING_ARG_ERROR   1

// For unicode.

constexpr u32 oneByteMax{0x7f};
constexpr u32 twoByteMax{0x7ff};
constexpr u32 threeByteMax{0xffff};
constexpr u32 fourByteMax{0x10ffff};

// Surrogate range for UTF-16.
// No valid UTF-8 value in this range (inclusive).
constexpr u32 surrogateRangeStart{0xd800};
constexpr u32 surrogateRangeEnd{0xdfff};

constexpr u8 commonByteStart{1 << 7};
constexpr u8 lastSixBits{0x3f};
constexpr u8 remainingBits{0xff};

struct NumParseRules
{
    char escape{};
    int maxDigits{}, shift{};
    bool (*check)(char);
    u8 (*convert)(char);
};

[[nodiscard]]
static inline u32 strToHex(const svIter it, int count)
{
	constexpr u32 hexShift{4};
    u32 result{0};
	for (int i{0}; i < count; i++)
		result = (result << hexShift) + fromHex(it[i]);

	return result;
}

// Assume value in valid UTF-8 range.
static void encodeUTF8(std::string& str, u32 value)
{
    constexpr u32 utf8EncodeMax{4};
    constexpr u8 bitShiftMax{7};

    std::array<char, utf8EncodeMax> buffer{0};
	u8 numBytes{};
	if (value > threeByteMax)       numBytes = 4;
	else if (value > twoByteMax)    numBytes = 3;
	else if (value > oneByteMax)    numBytes = 2;
	else
	{
		str.push_back(static_cast<char>(value));
		return;
	}

	for (u8 i{static_cast<u8>(numBytes - 1)}; i > 0; i--)
	{
		buffer[i] |= commonByteStart | (value & lastSixBits);
		value >>= 6;
	}

	u8 start{0};
	for (u8 i{0}; i < numBytes; i++)
		start |= (1 << (bitShiftMax - i));

	buffer[0] |= start | (value & remainingBits);
    str.append(buffer.data(), numBytes);
}

static bool setCode(DiagCode& code, DiagCode error)
{
    code = error;
    return false;
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
        case '`':   c = '`';    break;
        case '%':   c = '%';    break;
		default:	return false;
	}

    str.push_back(c);
    it += 2;
    return true;
}

[[nodiscard]] static bool checkParseArgs(
    const svIter it,
    const svIter end,
    const NumParseRules& rules,
    ErrorPair& pair
)
{
    if ((it >= end) || !rules.check(*it))
    {
        #if MISSING_ARG_ERROR
            setCode(pair.first, WRONG_CHAR_FOUND);
            setError(
                pair.second,
                CH_STR("missing arguments for '\\{}' escape character", rules.escape)
            );
        #endif

        return false;
    }

    return true;
}

#undef MISSING_ARG_ERROR

[[nodiscard]] static u32 parseEscapeString(
    svIter& it,
    const svIter end,
    const NumParseRules& rules
)
{
    u32 replace{0};
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
    ErrorPair& pair
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
    if (!checkParseArgs(it, end, rules, pair))
        return false;

    u32 replace{parseEscapeString(it, end, rules)};
    if ((rules.escape == 'o') // Check only for octal.
        && (replace > std::numeric_limits<u8>::max()))
    {
        return setCode(pair.first, HIT_OCTAL_CHAR_MAX);
    }

    str.push_back(static_cast<char>(replace));
    return true;
}

static int consumeUnicodeSequence(
    svIter& it,
    svIter end,
    ErrorPair& pair
)
{
    int count{0};
    while (it + count != end)
    {
        if (!isHex(it[count]))
        {
            if (it[count] != '}')
            {
                setCode(pair.first, WRONG_CHAR_FOUND);
                setError(pair.second, "invalid hex digit in codepoint");
                return -1;
            }

            break;
        }
        if (count++ == 6)
        {
            // More than 6 digits (3).
            it += 6;
            setCode(pair.first, WRONG_CHAR_FOUND);
            setError(pair.second,
                "too many digits for unicode character; maximum is 6");
            return -1;
        }
    }

    // Empty braces/no digits (2).
    if (count == 0)
    {
        setCode(pair.first, WRONG_CHAR_FOUND);
        setError(pair.second, "expect hex digits after opening brace for '\\u'");
        return -1;
    }
    // No closing brace (4).
    if ((it + count == end) || (it[count] != '}'))
    {
        it += count;
        setCode(pair.first, WRONG_CHAR_FOUND);
        setError(pair.second, "expect '}' after unicode sequence");
        return -1;
    }

    return count;
}

bool parseUnicodeSequence(
    std::string& str,
    svIter& it,
    const svIter end,
    ErrorPair& pair
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
    {
        setCode(pair.first, WRONG_CHAR_FOUND);
        return setError(pair.second, "expect '{' after '\\u'");
    }

    // Checks 2-4.
    it++; // Skip the {.
    int count{consumeUnicodeSequence(it, end, pair)};
    if (count == -1) return false;

    u32 value{strToHex(it, count)};
    it += count + 1; // Skip characters and closing brace.
    // Result outside unicode range (5).
    if ((value > fourByteMax)
        || ((value >= surrogateRangeStart) && (value <= surrogateRangeEnd)))
    {
        return setCode(pair.first, INVALID_UTF_CODEPOINT);
    }

    encodeUTF8(str, value);
    return true;
}