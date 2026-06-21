#pragma once
#include "astnodes.h"
#include "common.h"
#include "token.h"
#include "vm.h"
#include <personal/hash_table.h>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

using sv = std::string_view;
struct Function;

struct FileData
{
    std::string fileName;
    std::string content;
    std::vector<u64> lineMarkers{};
};

class SourceManager
{
    private:
        std::vector<FileData> sourceData;
        static FileID id;

        void computeLineMarkers(FileData& data);

    public:
        SourceManager() = default;

        [[nodiscard]]
        FileID addFile(const std::string& name, const std::string& content = "");
        [[nodiscard]] bool hasLineData(FileID id) const;

        void setFile(FileID id, const std::string& name);
        void setContent(FileID id, const std::string& content);
        void setLineMarkers(FileID id, const std::vector<u64>& lineMarkers);

        [[nodiscard]] const std::string& getFile(FileID id) const;
        [[nodiscard]] const std::vector<u64>& getLineMarkers(FileID id) const;

        [[nodiscard]] u64 getLineNumber(FileID id, u64 offset) const;
        [[nodiscard]] u64 getColumnNumber(FileID id, u64 offset) const;
        [[nodiscard]] sv getLineText(FileID id, u64 line) const;

        [[nodiscard]]
        std::pair<u64, u64> getLineColumn(FileID id, u64 offset) const;
        [[nodiscard]]
        std::tuple<u64, u64, sv> getPositionData(FileID id, u64 offset) const;
};

enum DiagFamily : u8
{
    // Errors.

    SYNTAX_ERROR,       // General syntax issue.
    VARIABLE_ERROR,     // Variable undefined, redefined, etc.
    TYPE_ERROR,         // Incorrect type for statement/expression.
    VALUE_ERROR,        // Valus is not permissible in context (e.g., division by zero).
    CALL_ERROR,         // General issue with function call (arity, callee type, etc.).
    ASSIGN_ERROR,       // Invalid assignment target.
    MUTATION_ERROR,     // Mutation of an immutable variable or value.
    CONTROL_FLOW_ERROR, // Invalid use of control-flow keyword or structure.

    // Warnings.

    UNUSED_WARNING,     // Variable, value or code is unused.
    MUTABILITY_WARNING, // Misuse of mutability modifiers.

    NUM_FAMILIES
};

enum DiagCode : u8
{
    /* Syntax Errors */

    // Single-line or multi-line comment not terminated.
    UNTERMINATED_COMMENT,
    // Invalid use of digit separator in numeric literal.
    INVALID_DIGIT_SEP,
    // Failure to parse a numeric literal (arbitrary reason).
    NUMERIC_LIT_PARSE_FAIL,
    // Invalid character for numeric literal type.
    INVALID_NUM_LIT_CHAR,
    // Invalid character for exponent in scientific notation.
    INVALID_SCI_NOTATION,
    // General error for "Expect [x] [in/after/before] [y].".
    WRONG_CHAR_FOUND,
    // Used single-line string for multi-line text.
    WRONG_STRING_SYNTAX,
    // Single-line or multi-line string was not terminated.
    UNTERMINATED_STRING,
    // String interpolation was not terminated.
    UNTERMINATED_INTER,
    // String interpolation max was exceeded.
    HIT_INTER_NEST_MAX,
    // General error when current character/token is not recognized
    // by the lexer.
    UNRECOGNIZED_TOKEN,
    // Initializer missing for immutable variable.
    MISSING_INITIALIZER,
    // General error for "Expect [x] [in/after/before] [y].".
    WRONG_TOKEN_FOUND,
    // Token makes no sense in current location.
    INVALID_TOKEN,
    // Too many parameters in function/lambda declaration.
    HIT_PARAM_MAX,
    // Hit end of line or end of input unexpectedly.
    UNEXPECTED_INPUT_END,
    // Octal value in escape sequence is too large.
    HIT_OCTAL_CHAR_MAX,
    // Codepoint value is not within valid UTF-8 range.
    INVALID_UTF_CODEPOINT,
    // Maximum block nesting depth reached.
    HIT_BLOCK_NESTING_MAX,
    // Maximum expression nesting depth reached.
    HIT_EXPR_NESTING_MAX,
    // Expect default paramater.
    EXPECT_DEFAULT_PARAM,
    // Parameter after variadic parameter.
    PARAM_AFTER_VARIADIC,
    // Function name too long.
    FUNC_NAME_TOO_LONG,
    // Too few values to unpack.
    UNPACK_TOO_FEW,
    // Too many values to unpack.
    UNPACK_TOO_MANY,


    /* Variable errors. */

    // Variable is not defined.
    VAR_NOT_DEFINED,
    // Function is not defined. Not used when function is
    // defined with a different arity.
    FUNC_NOT_DEFINED,
    // Variable is already defined in *current* scope.
    VAR_ALREADY_DEFINED,
    // Function is already defined in *current* scope.
    FUNC_ALREADY_DEFINED,
    // Function/lambda parameter name is already in-use.
    PARAM_ALREADY_DEFINED,


    /* Type errors. */

    // Could not apply binary operator to given operands.
    BINARY_OP_FAIL,
    // Could not apply unary operator to given operands.
    UNARY_OP_FAIL,
    // Object is not an index-able collection type.
    OBJ_NOT_COLLECTION,
    // Object cannot be used as an index.
    OBJ_NOT_INDEX,
    // Wrong element type to store in a collection.
    // Primarily for strings.
    WRONG_ELEM_TYPE,
    // Object to iterate over is not iterable.
    // Primarily for for-loops and the 'in' operator.
    OBJ_NOT_ITERABLE,
    // Object does not support element assignment.
    OBJ_NO_ELEM_ASSIGN,
    // Wrong type for iterator variable given iterable type.
    // Primarily for the 'in' operator.
    OBJ_WRONG_ITER_TYPE,
    // Argument provided has the wrong type.
    // Primarily used in built-in functions.
    WRONG_ARG_TYPE,
    // Attempting to unpack a non-collection object.
    UNPACK_NON_COLLECTION,
    // Attempting to unpack a Table object in order.
    UNPACK_TABLE,


    /* Value errors. */

    // Attempt to divide by zero.
    DIVISION_BY_ZERO,
    // Collection index is out of bounds.
    INDEX_OUT_OF_BOUNDS,
    // Table key not found.
    TABLE_KEY_NOT_FOUND,
    // Invalid step value for a range object.
    INVALID_RANGE_STEP,
    // Attempt to apply the modulus operator with base zero.
    MODULUS_WITH_ZERO,
    // Bit-shift operator shift value is too high.
    HIT_SHIFT_MAX,
    // General error while using the format!() built-in.
    FORMAT_STR_PROBLEM,


    /* Call errors. */

    // Attempt to call an object that is not callable.
    OBJ_NOT_CALLABLE,
    // Attempt to call a built-in function that does not exist.
    BUILTIN_NOT_FOUND,
    // No overload for function with given arity.
    // Specifically determined at runtime (we assume a compile-time
    // arity mismatch implies the necessary overload will be defined
    // later).
    ARITY_MISMATCH,
    // Attempt to use built-in function call syntax ([IDENTIFIER]!(...))
    // but failing to call the function by name.
    BUILTIN_CALL_NO_NAME,
    // Too many arguments for a function call.
    HIT_ARGS_MAX,
    // Too many call frames in call-stack.
    HIT_CALL_DEPTH_MAX,


    /* Assignment errors. */

    // LHS is not a variable or valid assignment target.
    // Also applies to compound-assignment operators.
    INVALID_ASSIGN_TARGET,
    // LHS is not a variable or valid increment/decrement target.
    INVALID_INCR_DECR_TARGET,


    /* Mutation errors. */

    // Variable is immutable (cannot be assigned to).
    ASSIGN_FIXED_VARIABLE,
    // Variable is immutable (cannot be modified).
    MOD_FIXED_VARIABLE,
    // Value is immutable (cannot be modified).
    MOD_IMMUT_VALUE,
    // Attempt to make immutable value mutable.
    IMMUT_TO_MUT,


    /* Control-flow errors. */

    // Invalid use of 'break' outside a loop.
    INVALID_BREAK,
    // Invalid use of 'continue' outside a loop.
    INVALID_CONTINUE,
    // Invalid use of 'break' in a comprehension loop.
    BREAK_IN_COMPREHEN,
    // Invalid use of 'continue' in a comprehension loop.
    CONT_IN_COMPREHEN,
    // Break/continue label is to assigned to any active loop.
    INVALID_LOOP_LABEL,
    // Invalid use of 'fallthrough' outside match-is structure.
    INVALID_FALLTHROUGH,
    // Invalid use of 'end' outside match-is structure.
    INVALID_END,
    // Statement found after 'fallthrough' statement (not allowed).
    STMT_AFTER_FALLTHROUGH,
    // Too many cases in match-is structure.
    HIT_MATCH_CASE_MAX,
    // Match-is case found after default case.
    CASE_AFTER_DEFAULT,
    // Invalid use of 'return' outside a function.
    INVALID_RETURN,
    // If-expression missing a false-case expression.
    IF_EXPR_MISSING_FALSE,


    /* Unused variable/object warnings. */

    // Variable is declared in current (local) scope, but never used.
    UNUSED_VARIABLE,
    // Expression (other than function call) is computed, but it has
    // no side-effects and its result is not used.
    UNUSED_EXPRESSION,
    // Code segment is not logically reachable.
    UNREACHABLE_CODE,

    /* Mutability warnings. */

    // Mutability specifier is redundant (e.g., immut immut 1).
    REDUNDANT_MUT_SPECIFIER,
    // Mutability specifier conflicts following one (e.g., immut mut 1).
    CONFLICT_MUT_SPECIFIER,
    // 'immut' specifier used on mutable object.
    MUT_TO_IMMUT,

    NUM_CODES
};

struct Diagnostic
{
    bool isError{};
    FileID id;
    u64 byteOffset{}, length{};

    DiagCode code{};
    std::string label{};
    // sv label{};
    // sv note{};
    // sv helpMsg{}, helpBody{};

    // Both helpers effectively return the same information.
    // getDiagCodeSection() helps to identify the opcode integer
    // equivalent.

    [[nodiscard]] static DiagFamily getDiagCodeFamily(DiagCode code);
    [[nodiscard]] DiagFamily getDiagCodeFamily() const;

    void displayReportTitle() const;

    // `maxLineNo`: The largest line number among a series of
    // diagnostics (to properly align line numbers when
    // printing a stack trace).

    void displayTruncatedErrorPart(
        u64 line,
        u64 col,
        sv text,
        u64 maxLineNo = 0
    ) const;
    void displayTruncatedLine(
        u64 line,
        u64 col,
        sv text,
        u64 maxLineNo = 0
    ) const;
    void displayErrorLine(
        u64 line,
        u64 col,
        sv text,
        u64 maxLineNo = 0
    ) const;

    // void displayNoteHelp(
    //     const sv& lineStr,
    //     const u64& lineNo,
    //     const std::string& gap
    // ) const;

    void report() const;
};

enum class ErrorSource : u8
{
    LEXER,
    PARSER,
    COMPILER,
    VM
};

class DiagnosticEngine
{
    private:
        std::vector<Diagnostic> reports{};

        std::optional<DiagCode> validateCode(sv code);
        std::string printStackEntry(const std::vector<CallFrame>& frames, u64 index);
        void displayErrorLine(FileID id, u64 line, u64 col, u64 maxLineNo);

    public:
        ErrorSource source{};

        DiagnosticEngine() = default;
        [[nodiscard]] bool hasReports() const { return !reports.empty(); }
        void explain(sv code);

        // Primitive error-reporting helper.
        void recordError(
            FileID id,
            DiagCode code,
            u64 byteOffset,
            u64 length,
            const std::string& label
        );

        // Direct version for lexer.
        void recordError(
            FileID id,
            DiagCode code,
            u64 byteOffset,
            const std::string& label
        );

        // Direct version for parser & compiler.
        void recordError(
            FileID id,
            DiagCode code,
            const Token& token,
            const std::string& label
        );

        // Primitive warning-reporting helper.
        void recordWarning(
            FileID id,
            DiagCode code,
            u64 byteOffset,
            u64 length,
            const std::string& label
        );

        // Direct version for parser & compiler.
        void recordWarning(
            FileID id,
            DiagCode code,
            const Token& token,
            const std::string& label
        );

        void emitReports();
        // Directly reports a runtime error with a stack trace.
        void emitStackTrace(const std::vector<CallFrame>& frames);
        // For when debug metadata is not available.
        // Only used with files (never in the REPL).
        void emitMiniStackTrace(const std::vector<CallFrame>& frames);
};