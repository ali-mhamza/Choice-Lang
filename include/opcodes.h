#pragma once
#include "common.h"
#include <string_view>
#include <unordered_map>

enum Opcode : ui8 // Each opcode is a single byte.
{
	// Basic values.

	OP_NEG_TWO,		// -2
	OP_NEG_ONE,		// -1
	OP_ZERO,		// 0
	OP_ONE,			// 1
	OP_TWO,			// 2
	OP_TRUE,		// true
	OP_FALSE,		// false
	OP_NULL,		// null
	
	// Arithmetic.

	OP_ADD,			// Add two values.
	OP_SUB,			// Subtract two values.
	OP_MULT,		// Multiply two values.
	OP_DIV,			// Divide two values.
	OP_POWER,		// Raise a value to a power.
	OP_MOD,			// Take the modulus between two values.
	OP_NEGATE,		// Invert a value's sign.
	OP_INCREMENT,	// Increment a value.
	OP_DECREMENT,	// Decrement a value.

	// Variables.

	OP_GET_VAR,		// Retrieve/load a variable.
	OP_SET_VAR,		// Assign to a variable.

	// Other types.

	OP_LIST,		// Create a list.
	OP_TABLE,		// Create a key-value table.

	// Comparison.

	OP_EQUAL,		// Check for equality.
	OP_GT,			// Check if greater than.
	OP_LT,			// Check if less than.

	// Boolean operators.

	OP_NOT,			// Invert a Boolean value.
	// && and || are implemented as control flow.
	// They don't get their own opcodes.

    // Bit-wise operators.
    OP_BIT_AND,     // AND two numeric values by bits.
    OP_BIT_OR,      // OR two numeric values by bits.
    OP_BIT_COMP,    // Invert the bits of a number.
    OP_BIT_XOR,     // XOR the bits of two numeric values.
    OP_BIT_SHIFT_R, // Shift a value's bits to the right.
    OP_BIT_SHIFT_L, // Shift a value's bits to the left.

	// Functions.

	OP_CALL_NAT,	// Call a native/built-in function.
	OP_CALL_DEF,	// Call a user-defined function.
	OP_RETURN,		// Return a value.

	// Internal opcodes.

	OP_JUMP,		// Jump forward through the byte-code (unconditional).
	OP_JUMP_TRUE,	// Jump only if previous condition evaluated to true.
	OP_JUMP_FALSE,	// Jump only if previous condition evaluated to false.
	OP_LOOP,		// Loop back through the byte-code.
	OP_BYTE_OPER,	// Operand is a single byte.
	OP_SHORT_OPER,	// Operand is two bytes.
	OP_LONG_OPER,	// Operand is four bytes.

	OP_LOAD_R,		// Load a constant into a register.
	OP_MOVE_R,		// Store a register's value in another register.
};

#define IS_VALID_OP(op)	(((op) >= OP_NEG_TWO) && ((op) <= OP_MOVE_R))

static std::string_view opNames[] = {
	"OP_NEG_TWO", "OP_NEG_ONE", "OP_ZERO", "OP_ONE",
	"OP_TWO", "OP_TRUE", "OP_FALSE", "OP_NULL",

	"OP_ADD", "OP_SUB", "OP_MULT", "OP_DIV", "OP_POWER",
	"OP_MOD", "OP_NEGATE", "OP_INCREMENT", "OP_DECREMENT",

	"OP_GET_VAR", "OP_SET_VAR",

	"OP_LIST", "OP_TABLE",

	"OP_EQUAL", "OP_GT", "OP_LT", "OP_NOT",

    "OP_BIT_AND", "OP_BIT_OR", "OP_BIT_COMP", "OP_BIT_XOR",
    "OP_BIT_SHIFT_R", "OP_BIT_SHIFT_L",

	"OP_CALL_NAT", "OP_CALL_DEF", "OP_RETURN",

	"OP_JUMP", "OP_JUMP_TRUE", "OP_JUMP_FALSE", "OP_LOOP",
	"OP_BYTE_OPER", "OP_SHORT_OPER", "OP_LONG_OPER",
	
	"OP_LOAD_R", "OP_MOVE_R"
};