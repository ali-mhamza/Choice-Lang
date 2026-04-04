#include "../include/escape_seq.h"
#include "../include/common.h"
#include "../include/error.h"
#include <string>
#include <string_view>

// Set to 1 if missing arguments for \b, \o,
// or \x in a string should be an error.
// If set to 0, missing arguments will not translate
// these escape sequences (i.e., "\x" -> "\x", no change).
#define MISSING_ARG_ERROR   1

struct ParseRules
{
    char escape{};
    int maxDigits{}, shift{};
    bool (*check)(char);
    ui8 (*convert)(char);
};

static inline bool isBinary(char c)     { return ((c == '0') || (c == '1')); }
static inline bool isOctal(char c)      { return ((c >= '0') && (c <= '7')); }
static inline bool isHex(char c)        { return isxdigit(c); }

static inline ui8 fromBinary(char c)    { return (c - '0'); }
static inline ui8 fromOctal(char c)     { return (c - '0'); }
static inline ui8 fromHex(char c)
{
    if (isdigit(c)) return (c - '0');
    if (isupper(c)) return (c - 'A' + 10);
    return (c - 'a' + 10);
}

constexpr ParseRules BinaryRules{
    'b', 8, 1,  // 8 digits allowed, shift by 1 bit.
    isBinary, fromBinary
};

constexpr ParseRules OctalRules{
    'o', 3, 3, // 3 digits allowed, shift by 3 bits.
    isOctal, fromOctal
};

constexpr ParseRules HexRules{
    'x', 2, 4, // 2 digits allowed, shift by 4 bits.
    isHex, fromHex
};

static bool checkParseArgs(
    const svIter it,
    const svIter end,
    const ParseRules& rules,
    std::string& errorMsg
)
{
    if ((it == end) || !rules.check(*it))
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
    const ParseRules& rules
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

bool parseCharSequence(std::string& str, svIter& it)
{
    switch (it[1])
	{
		case 'n':	str.push_back('\n');    it += 2;    return true;
		case 't':	str.push_back('\t');    it += 2;    return true;
		case 'r':	str.push_back('\r');    it += 2;    return true;
		case '\\':	str.push_back('\\');    it += 2;    return true;
		case '"':	str.push_back('"');     it += 2;    return true;
		default:	return false;
	}
}

bool parseNumericSequence(
    std::string& str,
    svIter& it,
    svIter end,
    std::string& errorMsg
)
{
    ParseRules rules{};
    switch (it[1])
    {
        case 'b':   rules = BinaryRules;    break;
        case 'o':   rules = OctalRules;     break;
        case 'x':   rules = HexRules;       break;
        default:    return false;
    }    

    if (!checkParseArgs(it + 2, end, rules, errorMsg))
        return false;

    it += 2;
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