#include "../include/diagnostic.h"
#include "../include/common.h"
#include "../include/config.h"
#include "../include/object.h"
#include "../include/token.h"
#include "../include/utils.h"
#include <fast_float/fast_float.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

constexpr u8 warningStart{static_cast<u8>(UNUSED_VARIABLE)};

static constexpr std::array<DiagCode, NUM_FAMILIES> familyMarkers{
    INVALID_UTF_CODEPOINT, PARAM_ALREADY_DEFINED, WRONG_ARG_TYPE,
    FORMAT_STR_PROBLEM, HIT_ARGS_MAX, MOD_CONST_VARIABLE,
    IF_EXPR_MISSING_FALSE, UNREACHABLE_CODE, REF_TO_CONST_VAR
};

static constexpr std::array<sv, NUM_FAMILIES> familyTitles{
    "Syntax Error", "Variable Error", "Type Error",
    "Value Error", "Function-Call Error", "Assignment Error",
    "Control-Flow Error", "Unused Warning", "Reference Warning"
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
    "Object is not an index-able collection type.",
    "Object cannot be used as an index.", "Object is not iterable.",
    "Operand does not match member type of iterable object.",
    "Wrong argument type or value.",

    // Value errors.

    "Division by zero.", "Modulus with base zero.", "Shift value too large.",
    "Invalid format argument.",

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

    "Unused variable.", "Expression result not used.",
    "Unreachable code segment.",

    // Constant reference warning.

    "Reference created to fixed-value variable."
};

FileID SourceManager::id{0};

void SourceManager::computeLineMarkers(FileData& data)
{
    const auto& content{data.content};
    auto& markers{data.lineMarkers};

    if (content.empty()) return;

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

// Worth revising for different cases.
// Needs more detailed review.
bool SourceManager::hasLineData(FileID id) const
{
    const FileData& data{sourceData[id]};
    bool lineMarkersAvailable{!data.lineMarkers.empty()};
    bool singleLine{!data.content.empty()
        && (data.content.find('\n') == data.content.npos)
    };
    return (lineMarkersAvailable || singleLine);
}

void SourceManager::setFile(FileID id, const std::string& name)
{
    sourceData[id].fileName = name;
}

void SourceManager::setContent(FileID id, const std::string& content)
{
    sourceData[id].content = content;
    sourceData[id].lineMarkers.clear();
    computeLineMarkers(sourceData[id]);
}

void SourceManager::setLineMarkers(
    FileID id,
    const std::vector<u64>& lineMarkers
)
{
    sourceData[id].lineMarkers = lineMarkers;
}

const std::string& SourceManager::getFile(FileID id) const
{
    return sourceData[id].fileName;
}

const std::vector<u64>& SourceManager::getLineMarkers(FileID id) const
{
    return sourceData[id].lineMarkers;
}

u64 SourceManager::getLineNumber(FileID id, u64 offset) const
{
    const FileData& data{sourceData[id]};

    u64 line{1};
    if (!data.lineMarkers.empty())
    {
        line = std::lower_bound(
            data.lineMarkers.begin(), data.lineMarkers.end(),
            offset
        ) - data.lineMarkers.begin() + 1;
    }

    return line;
}

u64 SourceManager::getColumnNumber(FileID id, u64 offset) const
{
    const FileData& data{sourceData[id]};
    u64 line{getLineNumber(id, offset)};

    u64 lineStart{
        (line == 1) ? 0 : data.lineMarkers[line - 2] + 1
    };
    u64 column{offset - lineStart + 1};

    return column;
}

sv SourceManager::getLineText(FileID id, u64 line) const
{
    const FileData& data{sourceData[id]};
    if ((data.content.empty()) || (line > data.lineMarkers.size() + 1))
        return "";

    u64 lineStart{
        (line == 1) ? 0 : data.lineMarkers[line - 2] + 1
    };
    u64 lineEnd{};

    if (data.lineMarkers.empty() || (line == data.lineMarkers.size() + 1))
        lineEnd = data.content.size() - 1;
    else
        lineEnd = data.lineMarkers[line - 1] - 1;

    return sv{&(data.content[lineStart]), lineEnd - lineStart + 1};
}

std::pair<u64, u64>
SourceManager::getLineColumn(FileID id, u64 offset) const
{
    return std::make_pair(
        getLineNumber(id, offset),
        getColumnNumber(id, offset)
    );
}

std::tuple<u64, u64, sv>
SourceManager::getPositionData(FileID id, u64 offset) const
{
    u64 line{getLineNumber(id, offset)};
    u64 column{getColumnNumber(id, offset)};
    sv text{getLineText(id, line)};

    return std::make_tuple(line, column, text);
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
        PRINT_ERROR_ARGS("{} [E{:0>4}]", familyTitles[family],
            static_cast<u8>(code) + 1);
        CH_PRINT(stderr, ": {}{}{}\n", BOLD, entry, NORMAL);
    }
    else
    {
        PRINT_WARNING_ARGS("{} [W{:0>4}]", familyTitles[family],
            static_cast<u8>(code) - warningStart + 1);
        CH_PRINT(stderr, ": {}{}{}\n", BOLD, entry, NORMAL);
    }
}

void Diagnostic::displayErrorLine(u64 line, u64 start, sv text) const
{
    constexpr auto EXTRA_SPACES{2u};

    // If the pointing caret isn't just past the end of the line,
    // we truncate it so it doesn't go past the line (if the token
    // is long, like with a multi-line string).

    u64 caretLength{length};
    if (byteOffset != text.size())
        caretLength = std::min(length, text.size() - byteOffset);

    std::string space(start - 1, ' ');
    std::string highlight(caretLength, '^');
    auto size{std::to_string(line).size()};
    std::string gap(size + EXTRA_SPACES, ' ');

    if (!label.empty())
    {
        highlight += " ";
        highlight += label;
    }

    CH_PRINT(stderr, "{}|\n", gap);
    CH_PRINT(stderr, " {} | {}\n", line, text);
    CH_PRINT(stderr, "{}| {}{}\n", gap, space, highlight);
    if (!label.empty())
        CH_PRINT(stderr, "{}|\n", gap);
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
    const auto [lineNo, start, lineStr] = sourceManager.getPositionData(
        id, byteOffset
    );

    sv fileName{sourceManager.getFile(id)};

    displayReportTitle();
    CH_PRINT(stderr, "  --> {} ({}:{})\n", fileName, lineNo, start);
    displayErrorLine(lineNo, start, lineStr);

    // displayNoteHelp(lineStr, lineNo, gap);
    CH_PRINT("\n");
}

std::optional<DiagCode> DiagnosticEngine::validateCode(sv code)
{
    #define IS_VALID_CODE(code) (((code) >= 0) && ((code) < NUM_CODES))

    constexpr u64 codeLength{5};
    bool isError{starts_with(code, "E")};
    bool isWarning{starts_with(code, "W")};

    if ((!isError && !isWarning) || (code.size() != 5))
    {
        PRINT_ERROR("Invalid error/warning code.\n");
        return std::nullopt;
    }

    u8 explainCode{};
    auto result{fast_float::from_chars(code.data() + 1,
        code.data() + codeLength, explainCode)};

    // Codes start from 1, but the enum starts from 0.
    explainCode--;
    if (isWarning) explainCode += warningStart;

    if (!result || !IS_VALID_CODE(explainCode))
    {
        PRINT_ERROR("Invalid error/warning code.\n");
        return std::nullopt;
    }

    return static_cast<DiagCode>(explainCode);
}

void DiagnosticEngine::explain(sv code)
{
    auto codeValue{validateCode(code)};
    if (!codeValue) return;

    DiagFamily family{Diagnostic::getDiagCodeFamily(codeValue.value())};
    CH_PRINT("Code:     {}.\n", code);
    CH_PRINT("Message:  {}\n", reportData[codeValue.value()]);
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
        true, id, byteOffset, length, code, label
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

void DiagnosticEngine::recordWarning(
    FileID id,
    DiagCode code,
    u64 byteOffset,
    u64 length,
    const std::string& label
)
{
    reports.push_back(Diagnostic{
        false, id, byteOffset, length, code, label
    });
}

void DiagnosticEngine::recordWarning(
    FileID id,
    DiagCode code,
    const Token& token,
    const std::string& label
)
{
    recordWarning(id, code, token.byteOffset, token.text.size(), label);
}

void DiagnosticEngine::emitReports()
{
    for (u64 i{0}; i < reports.size(); i++)
    {
        if ((source != ErrorSource::VM) && (i == COMPILE_ERROR_MAX))
        {
            PRINT_ERROR("COMPILATION ERROR MAXIMUM REACHED.\n");
            break;
        }

        const auto& diag{reports[i]};
        diag.report();
    }

    reports.clear();
}

std::string DiagnosticEngine::printStackEntry(
    const std::vector<CallFrame>& frames,
    u64 index
)
{
    const Function* func{frames[index].function};
    std::string output{};
    if (index == frames.size() - 1)
        output = "  at ";
    else
        output = "  called from ";

    if (index == 0)
        return output + "<script>";
    if (func->name == nullptr)
        return output + "[lambda]";
    else
        return output + CH_STR("{}()", func->name);
}

void DiagnosticEngine::displayErrorLine(
    FileID id,
    u64 line,
    u64 col,
    u64 maxLineNo
)
{
    constexpr auto EXTRA_SPACES{2u};
    const auto& text{sourceManager.getLineText(id, line)};

    // In case we aren't using the original source code.
    if (!inRepl && text.empty()) return;

    // If the pointing caret isn't just past the end of the line,
    // we truncate it so it doesn't go past the line.

    const auto& diag{reports.back()};
    u64 caretLength{diag.length};
    if (diag.byteOffset != text.size())
        caretLength = std::min(diag.length, text.size() - diag.byteOffset);

    std::string space(col - 1, ' ');
    std::string highlight(caretLength, '^');
    auto size{std::to_string(maxLineNo).size()};
    std::string gap(size + EXTRA_SPACES, ' ');

    CH_PRINT(stderr, "  {}|\n", gap);
    CH_PRINT(stderr, "   {:>{}} | {}\n", line, size, text);
    CH_PRINT(stderr, "  {}| {}{}\n\n", gap, space, highlight);
}

void
DiagnosticEngine::emitStackTrace(const std::vector<CallFrame>& frames)
{
    const auto& diag{reports.back()}; // Is invalidated after calling emitReports.
    const auto id{diag.id};
    emitReports(); // Emits the only runtime error (since they don't aggregate).

    auto size{frames.size()};
    // Line data is either available for each stack frame, or for none.
    bool lineDataExists{sourceManager.hasLineData(id)};
    std::vector<std::string> outputLines(size);
    std::vector<std::pair<u64, u64>> positions(lineDataExists ? size : 0);

    // Will store stack trace output for call stack in reverse
    // (most recent call last).
    for (u64 i{0}; i < size; i++)
    {
        outputLines[i] = printStackEntry(frames, i);
        if (!lineDataExists) continue;

        const Function* func{frames[i].function};
        const auto& range{func->getErrorRange(frames[i].ip)};

        positions[i] = sourceManager.getLineColumn(func->getID(), range.sourceStart);
        recordError(func->getID(), DiagCode{}, range.sourceStart,
            range.sourceEnd - range.sourceStart, "");
    }

    auto it{std::max_element(outputLines.begin(), outputLines.end(),
        [](auto& str1, auto& str2) { return str1.size() < str2.size(); }
    )};
    u64 maxLineNo{lineDataExists ?
        std::max_element(positions.begin(), positions.end())->first : 0
    };

    CH_PRINT(stderr, "Stack Trace:\n");
    for (u64 i{0}; i < size; i++)
    {
        const Function* func{frames[size - 1 - i].function};

        CH_PRINT(stderr, "{:<{}}", outputLines[size - 1 - i], it->size());
        CH_PRINT(stderr, "     {}", sourceManager.getFile(func->getID()));
        if (!lineDataExists)
        {
            CH_PRINT("\n");
            continue;
        }

        const auto& pos{positions[size - 1 - i]};
        CH_PRINT(stderr, " ({}:{})\n", pos.first, pos.second);
        displayErrorLine(func->getID(), pos.first, pos.second, maxLineNo);
        reports.pop_back();
    }
}

void
DiagnosticEngine::emitMiniStackTrace(const std::vector<CallFrame>& frames)
{
    const auto& fileName{sourceManager.getFile(reports.back().id)};
    reports.back().displayReportTitle();
    CH_PRINT(stderr, "  --> {}\n\n", fileName);

    auto size{frames.size()};
    std::vector<std::string> outputLines(size);

    for (u64 i{0}; i < size; i++)
        outputLines[i] = printStackEntry(frames, i);

    auto it{std::max_element(outputLines.begin(), outputLines.end(),
        [](auto& str1, auto& str2) { return str1.size() < str2.size(); }
    )};

    CH_PRINT(stderr, "Stack Trace:\n");
    for (u64 i{0}; i < size; i++)
    {
        CH_PRINT(stderr, "{:<{}}", outputLines[size - 1 - i], it->size());
        CH_PRINT(stderr, "     {}\n", fileName);
    }
}