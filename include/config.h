#pragma once

// General.

constexpr inline int MAX_SCOPE_DEPTH = 256;

// Lexer.

constexpr inline int TAB_SIZE = 4;
constexpr inline int AVG_TOK_SIZE = 4;
constexpr inline int LEX_ERROR_MAX = 10;

// Compiler(s).

constexpr inline int PARAMETER_MAX = 256;
constexpr inline int MATCH_CASES_MAX = 100;
constexpr inline int DEFAULT_LIST_SIZE = 16;
constexpr inline int LIST_ENTRY_GROUP = 50;
constexpr inline int COMPILE_ERROR_MAX = 10;

// VM.

constexpr inline int CALL_FRAMES_DEFAULT = 256;