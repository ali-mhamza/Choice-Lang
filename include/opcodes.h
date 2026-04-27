#pragma once
#include "common.h"		// For ui8 in enum defintion.
#include <array>
#include <string_view>

enum Opcode : ui8 // Each opcode is a single byte.
{
	/* Basic values. */

	OP_NEG_TWO,			// -2
	OP_NEG_ONE,			// -1
	OP_ZERO,			// 0
	OP_ONE,				// 1
	OP_TWO,				// 2
	OP_TRUE,			// true
	OP_FALSE,			// false
	OP_NULL,			// null

	/* Arithmetic. */

	OP_ADD,				// Add two values.
	OP_SUB,				// Subtract two values.
	OP_MULT,			// Multiply two values.
	OP_DIV,				// Divide two values.
	OP_POWER,			// Raise a value to a power.
	OP_MOD,				// Take the modulus between two values.
	OP_NEG,				// Invert a value's sign.
	OP_INCR,			// Increment a value.
	OP_DECR,			// Decrement a value.

	/* Comparison. */

	OP_EQUAL,			// Check for equality.
	OP_GT,				// Check if greater than.
	OP_LT,				// Check if less than.
	OP_IN,				// Check if a value or object is contained within an iterable object.

	/* Boolean operators. */

	OP_NOT,				// Invert a Boolean value.
	// && and || are implemented as control flow.
	// They don't get their own opcodes.

	/* Bit-wise operators. */

    OP_AND,				// OP_AND two numeric values by bits.
    OP_OR,				// OP_OR two numeric values by bits.
    OP_COMP,			// Invert the bits of a number.
    OP_XOR,				// XOR the bits of two numeric values.
    OP_SHIFT_R,			// Shift a value's bits to the right.
    OP_SHIFT_L,			// Shift a value's bits to the left.

	/* Variables. */

	OP_GET_GLOBAL,		// Retrieve/load a global variable.
	OP_SET_GLOBAL,		// Assign to a global variable.

	OP_GET_CELL,		// Retrieve/load a captured variable.
	OP_SET_CELL,		// Assign to a captured variable.

	OP_GET_LOCAL,		// Retrieve/load a local variable.
	OP_SET_LOCAL,		// Assign to a local variable.

	OP_MAKE_REF,		// Construct a reference to a variable.

	/* Types. */

	OP_LIST,			// Create a list.
	OP_EXT_LIST,		// Extend a list with additional elements.

	OP_TABLE,			// Create a key-value table.

	OP_RANGE,			// Create a range from two integers.

	OP_FORMAT_STR,		// Create a formatted string from multiple parts.

	OP_TUPLE,			// Create a tuple.
	OP_EXT_TUPLE,		// Extend a tuple with additional elements.

	/* Functions. */

	OP_CALL_NAT,		// Call a native/built-in function.
	OP_CALL_DEF,		// Call a user-defined function.
	OP_RETURN,			// Return a value.
	OP_VOID,			// Load an invalid (void) return value.

	OP_CLOSURE,			// Create a closure with an environment from a loaded function object.
	OP_CAPTURE_VAL,		// Capture a value from a surrounding scope into a cell.
	OP_CAPTURE_CELL,	// Capture a cell from a surrounding scope.

	/* Loop specifics. */

	OP_MAKE_ITER,		// Generate an iterator over an object.
	OP_UPDATE_ITER,		// Increment an iterator over an object and loop.

	/* Internal opcodes. */

	OP_JUMP,			// Jump forward through the byte-code (unconditional).
	OP_JUMP_TRUE,		// Jump only if previous condition evaluated to true.
	OP_JUMP_FALSE,		// Jump only if previous condition evaluated to false.
	OP_LOOP,			// Loop back through the byte-code.
	OP_BYTE_OPER,		// Operand is a single byte.
	OP_SHORT_OPER,		// Operand is two bytes.
	OP_LONG_OPER,		// Operand is four bytes.

	OP_ENTER_SCOPE,		// Mark that a scope is being entered (for internal use).
	OP_EXIT_SCOPE,		// Mark that a scope is being exited (for internal use).

	OP_LOAD_R,			// Load a constant into a register.
	OP_MOVE_R,			// Move a register's value into another register.
	OP_PRINT_VALID,		// Print the result of an expression (with exceptions).
	TOTAL_OPS
};

#define IS_VALID_OP(op)	(((op) >= OP_NEG_TWO) && ((op) < TOTAL_OPS))

constexpr std::array<std::string_view, TOTAL_OPS> opNames{
	#define LABEL(name, ...) #name,
	#include "opcode_list.inc"
	#undef LABEL
};