/*
 * Methods to format, display, and explain diagnostic errors,
 * possibly with accompanying error displays, stack traces, etc.
 */

#include "../include/diagnostic.h"
#include "../include/bytecode.h"
#include "../include/common.h"
#include "../include/config.h"
#include "../include/object.h"
#include "../include/silencer.h"
#include "../include/token.h"
#include "../include/utils.h"
#include "../include/vm.h"
#include <fast_float/fast_float.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

constexpr u8 warningStart{static_cast<u8>(UNUSED_VARIABLE)};

static constexpr std::array<DiagCode, NUM_FAMILIES> familyMarkers{
    REF_NOT_ASSIGN, METHOD_PRIVATE, TEST_FUNC_HAS_PARAMS,
    FIELD_NO_INSTANCE, INVALID_NUM_BASE, HIT_CALL_DEPTH_MAX,
    INVALID_INCR_DECR_TARGET, ALIAS_SPEC_MODULE, IMMUT_TO_MUT,
    RETURN_IN_CTOR, CLOSED_NON_FUNCTION, UNREACHABLE_CODE,
    MUT_TO_IMMUT
};

static constexpr std::array<sv, NUM_FAMILIES> familyTitles{
    "Syntax Error", "Variable Error", "Function Error",
    "Type Error", "Value Error", "Function-Call Error",
    "Assignment Error", "Module Error", "Mutation Error",
    "Control-Flow Error", "Attribute Error", "Unused Warning",
    "Mutability Warning"
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
    "Missing initializer for immutable variable(s).", "Unexpected token.",
    "Invalid token in current position.",
    "Too many parameters in function/lambda declaration.",
    "Unexpected end of input.", "Octal escape value too large.",
    "Codepoint value outside valid UTF-8 range.",
    "Reached maximum nesting depth for block scopes.",
    "Reached maximum nesting depth for expressions.",
    "Cannot have a regular parameter after a default parameter.",
    "Cannot have a parameter after a variadic parameter (with '...').",
    "Function name too long.", "Too few values to unpack into left-hand side.",
    "Too many values to unpack into left-hand side.",
    "Value being referenced is not assignable.",

    // Variable errors.

    "Undefined variable.", "Undefined function called.",
    "Variable already defined in current scope.",
    "Function already defined in current scope.",
    "Parameter with the same name already defined.",
    "Named field or method not defined for instance type.",
    "Field with the same name already defined.",
    "Method with the same name already defined.",
    "Method name collides with a field declared before it.",
    "Cannot get the value of a field before it has been initialized.",
    "Field is private.", "Method is private.",

    // Function errors.

    "Closed function cannot use objects declared outside its scope.",
    "'" CH_DESTRUCTOR "' method must have zero parameters.",
    "Test function must have zero parameters.",

    // Type errors.

    "Failed to apply binary operator.", "Failed to apply unary operator.",
    "Object is not an index-able collection type.",
    "Object cannot be used as an index.", "Wrong element type for collection.",
    "Object is not iterable.", "Object cannot be modified with the [] operator.",
    "Operand does not match member type of iterable object.",
    "Wrong argument type or value.", "Cannot unpack non-collection object.",
    "Cannot unpack a table.", "Object is not an instantiable type.",
    "Only type instances have fields.",

    // Value errors.

    "Division by zero.", "Index out of bounds.", "Table key not found.",
    "Invalid step value for range object.", "Modulus with base zero.",
    "Shift value too large.", "Invalid format argument.",
    "Invalid value for numeric base.",

    // Call errors.

    "Object is not callable.", "No built-in found with given name.",
    "Type being called has no available constructor.",
    "Function has no overload for given number of arguments.",
    "Built-in (called with '!') must be called by name.",
    "Too many arguments in function call.",
    "Reached maximum function call depth.",

    // Assignment errors.

    "Invalid assignment target.",
    "Invalid increment/decrement target.",

    // Module errors.

    "Corresponding file for module does not exist.",
    "Module used in circular import.",
    "Cannot access module entries on non-module objects.",
    "Entry not found in module.",
    "Cannot use alias for module when module entries are specified.",

    // Mutation errors.

    "Cannot assign new value to an immutable variable.",
    "Cannot modify the value of an immutable variable.",
    "Cannot modify the value of an immutable field.",
    "Cannot modify an immutable value.",
    "Attempt to make immutable value mutable.",

    // Control-flow errors.

    "Cannot use 'break' outside a loop.",
    "Cannot use 'continue' outside a loop.",
    "'break' is invalid within a comprehension loop.",
    "'continue' is invalid within a comprehension loop.",
    "Loop label is not assigned to any active loop.",
    "Cannot use 'fallthrough' outside a match-is structure.",
    "Cannot use 'end' outside a match-is structure.",
    "Cannot have a statement in same scope after 'fallthrough'.",
    "Too many cases in match-is structure.",
    "Case found after default case in match-is structure.",
    "Cannot use 'return' outside of a function/lambda.",
    "A conditional expression must have a false-case branch.",
    "Cannot return within a constructor.",

    // Attribute errors.

    "Invalid compiler attribute.", "Compiler attribute is repeated.",
    "'private' attribute is only valid for global-scope declarations.",
    "'static' attribute is only valid for local variables in functions.",
    "'computed' attribute is only valid for variables.",
    "'closed' attribute is only valid for functions.",
    "'test' attribute is only valid for global-scope functions.",

    // Unused warnings.

    "Unused variable.", "Expression result not used.",
    "Unreachable code segment.",

    // Mutability warning.

    "Mutability specifier is redundant.",
    "Mutability specifiers used conflict with each other.",
    "'immut' specifier used on mutable object."
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
    // We may have line markers without any actual line content,
    // since line markers can also be taken from debug metadata.
    // Thus, we check for both.
    bool lineMarkersAvailable{
        !data.content.empty() && !data.lineMarkers.empty()
    };
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
        CH_PRINT_ERROR_ARGS("{} [E{:0>4}]", familyTitles[family],
            static_cast<u8>(code) + 1);
        CH_PRINT(stderr, ": {}{}{}\n", BOLD, entry, NORMAL);
    }
    else
    {
        CH_PRINT_WARNING_ARGS("{} [W{:0>4}]", familyTitles[family],
            static_cast<u8>(code) - warningStart + 1);
        CH_PRINT(stderr, ": {}{}{}\n", BOLD, entry, NORMAL);
    }
}

void Diagnostic::displayTruncatedErrorPart(
    u64 line,
    u64 col,
    sv text,
    u64 maxLineNo
) const
{
    constexpr auto EXTRA_SPACES{2u};
    constexpr auto ELLIPSIS_SIZE{3u};
    if (maxLineNo == 0) maxLineNo = line;

    text = text.substr(col - 1, DIAG_MAX_TRUNC_LENGTH);
    sv before{(col == 1) ? "" : "..."};

    std::string space(before.size(), ' ');
    std::string highlight(DIAG_MAX_TRUNC_LENGTH + ELLIPSIS_SIZE, '^');
    auto size{std::to_string(maxLineNo).size()};
    std::string gap(size + EXTRA_SPACES, ' ');

    if (!label.empty()) highlight += " " + label;

    CH_PRINT(stderr, "{}|\n", gap);
    CH_PRINT(stderr, " {:>{}} | {}{}...\n", line, size, before, text);
    CH_PRINT(stderr, "{}| {}{}\n", gap, space, highlight);
    if (!label.empty())
        CH_PRINT(stderr, "{}|\n", gap);
}

void Diagnostic::displayTruncatedLine(
    u64 line,
    u64 col,
    sv text,
    u64 maxLineNo
) const
{
    constexpr auto EXTRA_SPACES{2u};
    if (maxLineNo == 0) maxLineNo = line;

    // If the pointing caret isn't just past the end of the line,
    // we truncate it so it doesn't go past the line (if the token
    // is long, like with a multi-line string).

    u64 caretLength{length};
    if (byteOffset != text.size())
        caretLength = std::min(length, text.size() - byteOffset);

    sv before{(col == 1) ? "" : "..."};
    sv after{(col + length >= text.size()) ? "" : "..."};
    text = text.substr(col - 1, length);

    std::string space(before.length(), ' ');
    std::string highlight(caretLength, '^');
    auto size{std::to_string(maxLineNo).size()};
    std::string gap(size + EXTRA_SPACES, ' ');

    if (!label.empty()) highlight += " " + label;

    CH_PRINT(stderr, "{}|\n", gap);
    CH_PRINT(stderr, " {:>{}} | {}{}{}\n", line, size, before, text, after);
    CH_PRINT(stderr, "{}| {}{}\n", gap, space, highlight);
    if (!label.empty())
        CH_PRINT(stderr, "{}|\n", gap);
}

void Diagnostic::trimDiagnosticWhiteSpace(
    sv& text,
    u64& col
) const
{
    const auto firstCharPos{text.find_first_not_of(" \t")};
    if ((firstCharPos != text.npos) && (firstCharPos > (2 * TAB_SIZE)))
    {
        const auto newLength{text.size() - (firstCharPos - 2 * TAB_SIZE)};
        col -= (text.size() - newLength);
        text = text.substr(firstCharPos - 2 * TAB_SIZE, newLength);
    }
}

void Diagnostic::displayErrorLine(
    u64 line,
    u64 col,
    sv text,
    u64 maxLineNo
) const
{
    constexpr auto EXTRA_SPACES{2u};
    if (maxLineNo == 0) maxLineNo = line;

    // If the pointing caret isn't just past the end of the line,
    // we truncate it so it doesn't go past the line (if the token
    // is long, like with a multi-line string).

    u64 caretLength{length};
    if (byteOffset != text.size())
        caretLength = std::min(length, text.size() - byteOffset);

    trimDiagnosticWhiteSpace(text, col);
    std::string space(col - 1, ' ');
    std::string highlight(caretLength, '^');
    auto size{std::to_string(maxLineNo).size()};
    std::string gap(size + EXTRA_SPACES, ' ');

    if (!label.empty()) highlight += " " + label;

    CH_PRINT(stderr, "{}|\n", gap);
    CH_PRINT(stderr, " {:>{}} | {}\n", line, size, text);
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
    const auto [lineNo, col, lineStr] = sourceManager.getPositionData(
        id, byteOffset
    );

    sv fileName{sourceManager.getFile(id)};

    displayReportTitle();
    CH_PRINT(stderr, "  --> {} ({}:{})\n", fileName, lineNo, col);

    if (sourceManager.hasLineData(id))
    {
        if (length > DIAG_LINE_LENGTH_MAX)
            displayTruncatedErrorPart(lineNo, col, lineStr);
        else if (lineStr.length() > DIAG_LINE_LENGTH_MAX)
            displayTruncatedLine(lineNo, col, lineStr);
        else
            displayErrorLine(lineNo, col, lineStr);
    }

    CH_PRINT("\n");
}

std::optional<DiagCode> DiagnosticEngine::validateCode(sv code)
{
    #define IS_VALID_CODE(code) (((code) >= 0) && ((code) < NUM_CODES))

    constexpr u64 codeLength{5};
    bool isError{starts_with(code, "E")};
    bool isWarning{starts_with(code, "W")};

    if ((!isError && !isWarning) || (code.size() != codeLength))
    {
        CH_PRINT_ERROR("Invalid error/warning code.\n");
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
        CH_PRINT_ERROR("Invalid error/warning code.\n");
        return std::nullopt;
    }

    return static_cast<DiagCode>(explainCode);

    #undef IS_VALID_CODE
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
    if (Silencer::silenced) return;

    for (u64 i{0}; i < reports.size(); i++)
    {
        if ((source != ErrorSource::VM) && (i == COMPILE_ERROR_MAX))
        {
            CH_PRINT_ERROR("COMPILATION ERROR MAXIMUM REACHED.\n");
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
    // The function/closure object is always just before its
    // respective register window, except script-level code
    // (which has no such object).
    const Function* func{nullptr};
    if (index != 0)
    {
        const Object& obj{frames[index].regStart[-1]};
        func = (IS_CLOSURE(obj) ? AS_CLOSURE(obj)->function : AS_USER_FUNC(obj));
    }

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
    const auto& text{sourceManager.getLineText(id, line)};

    // In case we aren't using the original source code.
    // Must check before trying to fetch (non-existent) diagnostics.
    if (!inRepl && text.empty()) return;

    const auto& diag{reports.back()};

    if (diag.length > DIAG_LINE_LENGTH_MAX)
        diag.displayTruncatedErrorPart(line, col, text, maxLineNo);
    else if (text.length() > DIAG_LINE_LENGTH_MAX)
        diag.displayTruncatedLine(line, col, text, maxLineNo);
    else
        diag.displayErrorLine(line, col, text, maxLineNo);

    CH_PRINT("\n");
}

void
DiagnosticEngine::emitStackTrace(const std::vector<CallFrame>& frames)
{
    if (Silencer::silenced) return;

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

        const ByteCode* code{frames[i].code};
        const auto& range{code->getErrorRange(frames[i].ip)};

        positions[i] = sourceManager.getLineColumn(code->getID(), range.sourceStart);
        recordError(code->getID(), DiagCode{}, range.sourceStart,
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
        const ByteCode* code{frames[size - 1 - i].code};

        CH_PRINT(stderr, "{:<{}}", outputLines[size - 1 - i], it->size());
        CH_PRINT(stderr, "     {}", sourceManager.getFile(code->getID()));
        if (!lineDataExists)
        {
            CH_PRINT("\n");
            continue;
        }

        const auto& pos{positions[size - 1 - i]};
        CH_PRINT(stderr, " ({}:{})\n", pos.first, pos.second);
        displayErrorLine(code->getID(), pos.first, pos.second, maxLineNo);
        reports.pop_back();
    }
}

void
DiagnosticEngine::emitMiniStackTrace(const std::vector<CallFrame>& frames)
{
    if (Silencer::silenced) return;

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
        FileID id{frames[i].code->getID()};
        CH_PRINT(stderr, "     {}\n", sourceManager.getFile(id));
    }
}