#pragma once

/* General */

// Maximum value we can encode with a single byte.
constexpr inline unsigned int CODE_MAX{(1 << 8) - 1};

/* Lexer */

// Configured character width for tab characters.
constexpr inline unsigned int TAB_SIZE{4};
// Used to estimate space to reserve early in token array.
constexpr inline unsigned int AVG_TOK_SIZE{4};
// Cut-off for errors in the lexer.
constexpr inline unsigned int LEX_ERROR_MAX{10};
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
// Cut-off for errors in the compiler (+ parser, if applicable).
constexpr inline unsigned int COMPILE_ERROR_MAX{10};

/* VM */

// Maximum depth of nested scopes.
constexpr inline unsigned int MAX_SCOPE_DEPTH{CODE_MAX + 1};
// Used to estimate space to reserve early in call-stack.
constexpr inline unsigned int CALL_FRAMES_DEFAULT{10};

/* Disassembler */

// Whether or not to disassemble loaded function objects.
constexpr inline bool DIS_FUNCTION_OBJS{true};