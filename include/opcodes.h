#pragma once
#include "common.h"

enum Opcode : uint8_t // Each opcode is a single byte.
{
	// Basic values.
	OP_ZERO,		// 0
	OP_ONE,			// 1
	OP_TWO,			// 2
	OP_NEG_ONE,		// -1
	OP_NEG_TWO,		// -2
	OP_NULL,		// null
	
	// Arithmetic.

	OP_ADD,			// Add two values.
	OP_SUB,			// Subtract two values.
	OP_MULT,		// Multiply two values.
	OP_DIV,			// Divide two values.
	OP_NEGATE,		// Invert a value's sign.
	OP_POWER,		// Raise a value to a power.
	OP_MOD,			// Take the modulus between two values.

	// Variables.

	OP_DEF_VAR,		// Define a variable.
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
	OP_AND,			// AND two Boolean values.
	OP_OR,			// OR two Boolean values.

    // Bit-wise operators.
    OP_BIT_AND,     // AND two numeric values by bits.
    OP_BIT_OR,      // OR two numeric values by bits.
    OP_BIT_COMP,    // Invert the bits of a number.
    OP_BIT_XOR,     // XOR the bits of two numeric values.
    OP_BIT_SHIFT_R, // Shift a value's bits to the right.
    OP_BIT_SHIFT_L, // Shift a value's bits to the left.

	// Commands.

	OP_PRINT,		// Print a value.
	OP_RETURN,		// Return a value.

	// Internal opcodes.

	OP_JUMP,		// Jump through the byte-code.
	OP_LOOP,		// Loop back through the byte-code.
	OP_BYTE_OPER,	// Operand is a single byte.
	OP_SHORT_OPER,	// Operand is two bytes.
	OP_LONG_OPER	// Operand is four bytes.
};

Opcode voids[] = {OP_ZERO, OP_ONE, OP_TWO, OP_NEG_ONE, OP_NULL,
					OP_BYTE_OPER, OP_SHORT_OPER, OP_LONG_OPER};

bool noOper(Opcode code)
{
	for (Opcode op : voids)
		if (code == op)
			return true;
	return false;
}

bool byteOper(Opcode code);
bool shortOper(Opcode code);
bool longOper(Opcode code);