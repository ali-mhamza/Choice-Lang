/*
 * Configuration constants to adjust certain behavior
 * in the interpreter (and avoid magic numbers).
 */

#pragma once

/* General */

// Maximum value we can encode with a single byte.
constexpr inline unsigned int CODE_MAX{(1 << 8) - 1};

/* Lexer */

// Configured character width for tab characters.
constexpr inline unsigned int TAB_SIZE{4};
// Used to estimate space to reserve early in token array.
constexpr inline unsigned int AVG_TOK_SIZE{4};
// Maximum level of nesting for string interpolation.
constexpr inline unsigned int INTERPOLATION_MAX{2};

/* Compiler */

// Maximum number of bytes we can jump across in the bytecode.
constexpr inline unsigned int BYTE_JUMP_MAX{(1 << 16) - 1};
// Maximum number of parameters for a function or lambda.
constexpr inline unsigned int PARAMETER_MAX{CODE_MAX};
// Maximum number of cases in a match-is structure.
constexpr inline unsigned int MATCH_CASES_MAX{100};
// Default list size upon initialization.
constexpr inline unsigned int DEFAULT_LIST_SIZE{16};
// Default partition size for list elements (we add X elements at a time).
constexpr inline unsigned int LIST_ENTRY_GROUP{50};
// Default partition size for table pairs.
constexpr inline unsigned int TABLE_ENTRY_GROUP{20};
// Cut-off for errors across lexer, parser, or compiler.
constexpr inline unsigned int COMPILE_ERROR_MAX{10};
// Maximum expression nesting.
constexpr inline unsigned int MAX_EXPR_NEST_DEPTH{100};
// Maximum block scope nesting.
constexpr inline unsigned int MAX_BLOCK_SCOPE_DEPTH{100};

// Built-ins.

// Number of pre-defined global identifiers (constants or functions).
constexpr inline unsigned int BUILTIN_GLOBALS{17};
// Number of pre-defined local identifiers (constant or functions).
constexpr inline unsigned int BUILTIN_LOCALS{1};
// Register location of '_file_' global variable.
constexpr inline unsigned int FILENAME_LOC{0};

/* VM */

// Number of available registers across all stack frames.
constexpr inline unsigned int NUM_REGS{1 << 12};
// Maximum depth level in call-stack.
constexpr inline unsigned int MAX_CALL_DEPTH{1000};
// Used to estimate space to reserve early for scope start markers.
constexpr inline unsigned int SCOPE_DEPTH_DEFAULT{100};
// Used to estimate space to reserve early in call-stack.
constexpr inline unsigned int CALL_FRAMES_DEFAULT{10};

/* Disassembler */

// Whether or not to disassemble loaded function objects.
constexpr inline bool DIS_FUNCTION_OBJS{true};

/* Diagnostics. */

// Maximum line length for diagnostics (in characters).
constexpr inline unsigned int DIAG_LINE_LENGTH_MAX{80};
// Maximum length for offending part/line upon truncation (if too long).
constexpr inline unsigned int DIAG_MAX_TRUNC_LENGTH{10};