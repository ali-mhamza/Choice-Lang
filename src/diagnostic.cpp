#include "fast_float.h"
#include "../include/diagnostic.h"
#include "../include/common.h"
#include "../include/utils.h"
#include "../include/token.h"
#include <array>
#include <cstdio>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

static constexpr sv RED{"\033[31m"};
static constexpr sv YELLOW{"\033[33m"};
static constexpr sv BOLD{"\033[1m"};
static constexpr sv NORMAL{"\033[0m"};

static constexpr std::array<DiagCode, NUM_FAMILIES> familyMarkers{
    INVALID_UTF_CODEPOINT, PARAM_ALREADY_DEFINED, RANGE_ONLY_INTS,
    HIT_SHIFT_MAX, HIT_ARGS_MAX, MOD_CONST_VARIABLE,
    IF_EXPR_MISSING_FALSE, UNREACHABLE_CODE, REF_TO_CONST_VAR
};

static constexpr std::array<sv, NUM_FAMILIES> familyTitles{
    "Syntax Error", "Variable Error", "Type Error",
    "Value Error", "Function-call Error", "Assignment Error",
    "Control-flow Error", "Unused Warning", "Reference Warning"
};

// Temporarily.
using DiagnosticEntry = sv;

static constexpr std::array<DiagnosticEntry, NUM_CODES> reportData{
    // Syntax errors.

    "Unterminated comment.", "Invalid use of digit separator.",
    "Failed to parse numeric literal.", "Invalid character for numeric literal.",
    "Invalid character for exponent.", "Unexpected character.",
    "Single-line string cannot contain newlines.", "String not terminated.",
    "String interpolation not terminated.",
    "Interpolation nesting exceeded maximum depth.", "Unrecognized token.",
    "Missing initializer for immutable variable.", "Unexpected token.",
    "Invalid token in current position.", "Maximum scope depth exceeded.",
    "Too many parameters in function/lambda declaration.",
    "Unexpected end of input.", "Octal escape value too large.",
    "Codepoint value outside valid UTF-8 range.",

    // Variable errors.

    "Undefined variable.", "Undefined function called.",
    "Variable already defined in current scope.",
    "Function already defined in current scope.",
    "Parameter with the same name already in use.",

    // Type errors.

    "Failed to apply binary operator.", "Failed to apply unary operator.",
    "Object is not iterable.",
    "Operand does not match member type of iterable object.",
    "Wrong argument type.", "Can only construct range object from integers.",

    // Value errors.

    "Division by zero.", "Modulus with base zero.", "Shift value too large.",

    // Call errors.

    "Object is not callable.", "No built-in found with given name.",
    "Function has no overload for given number of arguments.",
    "Built-in (called with '!') must be called by name.",
    "Too many arguments in function call.",

    // Assign errors.

    "Invalid assignment target.",
    "Invalid increment/decrement target.",
    "Cannot assign value to a fixed-value variable.",
    "Cannot modify a fixed-value variable.",

    // Control-flow errors.

    "Loop label is not assigned to any active loop.",
    "Cannot use 'fallthrough' outside a match-is structure.",
    "Cannot use 'end' outside a match-is structure.",
    "Cannot have a statement in same scope after 'fallthrough'.",
    "Too many cases in match-is structure.",
    "Case found after default case in match-is structure.",
    "Cannot use 'return' outside of a function/lambda.",
    "A conditional expression must have a false-case branch.",

    // Unused warnings.

    "Unused variable.", "Expression result not used.", "Unreachable code segment.",

    // Constant reference warning.

    "Reference created to fixed-value variable."
};

FileID SourceManager::id{0};

void SourceManager::computeLineMarkers(FileData& data)
{
    const auto& content{data.content};
    auto& markers{data.lineMarkers};

    u64 pos{0};
    while ((pos = content.find('\n', pos)) != std::string::npos)
    {
		markers.push_back(pos);
		if (pos == data.content.size() - 1)
			break;
        // Skip the \n.
		pos++;
	}
}

FileID SourceManager::addFile(
    const std::string& name,
    const std::string& content
)
{
    FileData data{name, content, {}};
    computeLineMarkers(data);
    sourceData.push_back(data);
    id++;
    return id - 1;
}

// For now, we use this to save only one content entry for the REPL.
// Later, we may wish to keep each line as an independent entry so
// that we can combine diagnostics from different lines (would
// require some more expansion work here, though).
void SourceManager::setFileContent(FileID id, const std::string& content)
{
    sourceData[id].content = content;
    sourceData[id].lineMarkers.clear();
    computeLineMarkers(sourceData[id]);
}

std::tuple<u64, u64, sv> SourceManager::getLineColumn(
    FileID id,
    u64 offset
)
{
    const FileData& data{sourceData[id]};
    u64 line{1};
    if (!data.lineMarkers.empty())
    {
        for (const auto& pos : data.lineMarkers)
        {
            if (offset <= pos)
                break;
            line++;
        }
    }

    u64 lineStart{
        (line == 1) ? 0 : data.lineMarkers[line - 2] + 1
    };
    u64 lineEnd{};

    if (data.lineMarkers.empty() || (offset > data.lineMarkers.back()))
        lineEnd = data.content.size() - 1;
    else
        lineEnd = data.lineMarkers[line - 1] - 1;

    u64 column{offset - lineStart + 1};
    sv lineStr{&(data.content[lineStart]), lineEnd - lineStart + 1};

    return std::make_tuple(line, column, lineStr);
}

// [[nodiscard]]
// static std::pair<u64, u64> getChangeSpan(sv a, sv b)
// {
//     if (a.empty() || b.empty())
//         return std::make_pair(0, 0);

//     auto aIter{a.begin()};
//     auto bIter{b.begin()};
//     while (*aIter == *bIter)
//     {
//         if ((aIter == a.end()) || (bIter == b.end()))
//             break;
//         aIter++;
//         bIter++;
//     }

//     u64 start{static_cast<u64>(bIter - b.begin())};

//     u64 end{b.size()};
//     aIter = a.end() - 1;
//     bIter = b.end() - 1;
//     while (*aIter == *bIter)
//     {
//         if ((aIter == a.begin()) || (bIter == b.begin()))
//             break;
//         aIter--;
//         bIter--;
//         end--;
//     };
//     if ((aIter == a.begin()) && (bIter != b.begin()))
//         end--;

//     return std::make_pair(start, end - start);
// }

DiagFamily Diagnostic::getDiagCodeFamily(DiagCode code)
{
    for (u8 i{0}; i < NUM_FAMILIES; i++)
    {
        if (code <= familyMarkers[i])
            return static_cast<DiagFamily>(i);
    }

    CH_UNREACHABLE();
}

DiagFamily Diagnostic::getDiagCodeFamily() const
{
    return getDiagCodeFamily(this->code);
}

void Diagnostic::displayReportTitle() const
{
    DiagFamily family{getDiagCodeFamily()};
    DiagnosticEntry entry{reportData[code]};

    if (isError)
    {
        CH_PRINT(stderr, "{}{} [E{:0>4}]{}: ", RED, familyTitles[family],
            static_cast<u8>(code) + 1, NORMAL);
        CH_PRINT(stderr, "{}{}{}\n", BOLD, entry, NORMAL);
    }
    else
    {
        CH_PRINT(stderr, "{}{} [W{:0>4}]{}: ", YELLOW, familyTitles[family],
            static_cast<u8>(code) + 1, NORMAL);
        CH_PRINT(stderr, "{}{}{}\n", BOLD, entry, NORMAL);
    }
}

// void Diagnostic::displayNoteHelp(
//     const sv& lineStr,
//     const u64& lineNo,
//     const std::string& gap
// ) const
// {
//     if (!note.empty())
//         CH_PRINT("\n{}note:{} {}\n", BOLD, NORMAL, note);
//     if (!helpMsg.empty())
//         CH_PRINT("\n{}help:{} {}\n", BOLD, NORMAL, helpMsg);
//     if (!helpBody.empty())
//     {
//         auto pair{getChangeSpan(lineStr, helpBody)};
//         std::string helpSpace(pair.first, ' ');
//         std::string helpHighlight(pair.second, '^');
//         CH_PRINT(stderr, "{}|\n", gap);
//         CH_PRINT(stderr, " {} | {}\n", lineNo, helpBody);
//         CH_PRINT(stderr, "{}| {}{}\n", gap, helpSpace, helpHighlight);
//     }
// }

void Diagnostic::report() const
{
    constexpr auto EXTRA_SPACES{2u};
    const auto [lineNo, start, lineStr] = manager->getLineColumn(
        id, byteOffset
    );

    displayReportTitle();

    std::string space(start - 1, ' ');
    std::string highlight(length, '^');
    auto size{std::to_string(lineNo).size()};
    std::string gap(size + EXTRA_SPACES, ' ');

    if (!label.empty())
    {
        highlight += " ";
        highlight += label;
    }

    sv file{manager->getFile(id)};
    if (file.empty()) file = "<script>";

    CH_PRINT(stderr, "  --> {} ({}:{})\n", file, lineNo,
        start);
    CH_PRINT(stderr, "{}|\n", gap);
    CH_PRINT(stderr, " {} | {}\n", lineNo, lineStr);
    CH_PRINT(stderr, "{}| {}{}\n", gap, space, highlight);

    // displayNoteHelp(lineStr, lineNo, gap);
    CH_PRINT("\n");
}

DiagnosticEngine::DiagnosticEngine(SourceManager* manager) :
    manager{manager} {}

std::pair<bool, DiagCode> DiagnosticEngine::validateCode(sv code)
{
    #define IS_VALID_CODE(code) (((code) >= 0) && ((code) < NUM_CODES))

    constexpr u64 codeLength{5};
    constexpr u8 warningStart{static_cast<u8>(UNUSED_VARIABLE)};
    bool isError{starts_with(code, "E")};
    bool isWarning{starts_with(code, "W")};

    if ((!isError && !isWarning) || (code.size() != 5))
    {
        CH_PRINT(stderr, "{}Invalid error/warning code.{}\n", RED, NORMAL);
        return std::make_pair(false, static_cast<DiagCode>(0));
    }

    u8 explainCode{};
    auto result{fast_float::from_chars(code.data() + 1,
        code.data() + codeLength, explainCode)};

    // Codes start from 1, but the enum starts from 0.
    explainCode--;
    if (isWarning) explainCode += warningStart;

    if (!result || !IS_VALID_CODE(explainCode))
    {
        CH_PRINT(stderr, "{}Invalid error/warning code.{}\n", RED, NORMAL);
        return std::make_pair(false, static_cast<DiagCode>(0));
    }

    return std::make_pair(true, static_cast<DiagCode>(explainCode));
}

void DiagnosticEngine::explain(sv code)
{
    auto pair{validateCode(code)};
    if (!pair.first) return;

    DiagFamily family{Diagnostic::getDiagCodeFamily(pair.second)};
    CH_PRINT("Code:     {}.\n", code);
    CH_PRINT("Message:  {}\n", reportData[pair.second]);
    CH_PRINT("Category: {}.\n", familyTitles[family]);
}

void DiagnosticEngine::recordError(
    FileID id,
    DiagCode code,
    u64 byteOffset,
    u64 length,
    const std::string& label
)
{
    reports.push_back(Diagnostic{
       manager, true, id, byteOffset, length, code, label
    });
}

void DiagnosticEngine::recordError(
    FileID id,
    DiagCode code,
    u64 byteOffset,
    const std::string& label
)
{
    recordError(id, code, byteOffset, 1, label);
}

void DiagnosticEngine::recordError(
    FileID id,
    DiagCode code,
    const Token& token,
    const std::string& label
)
{
    recordError(id, code, token.byteOffset, token.text.size(), label);
}

void DiagnosticEngine::emitReports()
{
    for (const auto& diag : reports)
        diag.report();
    reports.clear();
}