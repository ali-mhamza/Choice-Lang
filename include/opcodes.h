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
	OP_INCR,		// Increment a value.
	OP_DECR,		// Decrement a value.

	// Comparison.

	OP_EQUAL,		// Check for equality.
	OP_GT,			// Check if greater than.
	OP_LT,			// Check if less than.
	OP_IN,			// Check if a value or object is contained within an iterable object.

	// Boolean operators.

	OP_NOT,			// Invert a Boolean value.
	// && and || are implemented as control flow.
	// They don't get their own opcodes.

	// Bit-wise operators.

    OP_AND,			// OP_AND two numeric values by bits.
    OP_OR,			// OP_OR two numeric values by bits.
    OP_COMP,		// Invert the bits of a number.
    OP_XOR,			// XOR the bits of two numeric values.
    OP_SHIFT_R,		// Shift a value's bits to the right.
    OP_SHIFT_L,		// Shift a value's bits to the left.

	// Variables.

	OP_GET_VAR,		// Retrieve/load a variable.
	OP_SET_VAR,		// Assign to a variable.

	// Types.

	OP_LIST,		// Create a list.
	OP_EXT_LIST,	// Extend a list with additional elements.

	OP_TABLE,		// Create a key-value table.

	OP_TUPLE,		// Create a tuple.
	OP_EXT_TUPLE,	// Extend a tuple with additional elements.

	// Functions.

	OP_CALL_NAT,	// Call a native/built-in function.
	OP_CALL_DEF,	// Call a user-defined function.
	OP_RETURN,		// Return a value.
	OP_VOID,		// Load an invalid (void) return value.

	// Loop specifics.

	OP_MAKE_ITER,	// Generate an iterator over an object.
	OP_UPDATE_ITER,	// Increment an iterator over an object and loop.

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
	OP_PRINT_VALID,	// Print the result of an expression, except calls to void (non-returning) functions.
	TOTAL_OPS
};

#define IS_VALID_OP(op)	(((op) >= 0) && ((op) < TOTAL_OPS))
/*	Opcodes that can directly modify variables,
*	and thus have a necessary offset argument. */
#define CAN_MODIFY(op) \
	(((op) >= OP_ADD) && ((op) <= OP_DECR)) \
	|| (((op) <= OP_AND) && ((op) >= OP_SHIFT_L))

static std::string_view opNames[] = {
	"OP_NEG_TWO", "OP_NEG_ONE", "OP_ZERO", "OP_ONE",
	"OP_TWO", "OP_TRUE", "OP_FALSE", "OP_NULL",

	"OP_ADD", "OP_SUB", "OP_MULT", "OP_DIV", "OP_POWER",
	"OP_MOD", "OP_NEGATE", "OP_INCR", "OP_DECR",

	"OP_EQUAL", "OP_GT", "OP_LT", "OP_IN", "OP_NOT",

	"OP_AND", "OP_OR", "OP_COMP", "OP_XOR",
    "OP_SHIFT_R", "OP_SHIFT_L",

	"OP_GET_VAR", "OP_SET_VAR",

	"OP_LIST", "OP_EXT_LIST",
	"OP_TABLE",
	"OP_TUPLE", "OP_EXT_TUPLE",

	"OP_CALL_NAT", "OP_CALL_DEF", "OP_RETURN", "OP_VOID",

	"OP_MAKE_ITER", "OP_UPDATE_ITER",

	"OP_JUMP", "OP_JUMP_TRUE", "OP_JUMP_FALSE", "OP_LOOP",
	"OP_BYTE_OPER", "OP_SHORT_OPER", "OP_LONG_OPER",

	"OP_LOAD_R", "OP_MOVE_R", "OP_PRINT_VALID"
};