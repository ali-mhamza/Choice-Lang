/*
 * Enum and string names for opcodes used in bytecode
 * chunks.
 */

#pragma once
#include "common.h"
#include <array>
#include <string_view>

enum Opcode : u8 // Each opcode is a single byte.
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

	OP_DEF_START,		// Begin a variable declaration/definition.
	OP_DEF_END,			// End a variable declaration/definition.

	OP_GET_GLOBAL,		// Retrieve/load a global variable.
	OP_SET_GLOBAL,		// Assign to a global variable.

	OP_GET_CELL,		// Retrieve/load a captured variable.
	OP_SET_CELL,		// Assign to a captured variable.

	OP_GET_LOCAL,		// Retrieve/load a local variable.
	OP_SET_LOCAL,		// Assign to a local variable.

	OP_VAR_REF,         // Construct a reference to a variable.
	OP_INDEX_REF,       // Construct a reference to an element within another object.
	OP_FIELD_REF,       // Construct a reference to an instance field.

	/* Built-in Types. */

	OP_LIST,			// Create a list.
	OP_EXT_LIST,		// Extend a list with additional elements.

	OP_TABLE,			// Create a key-value table.
	OP_EXT_TABLE,		// Extend a table with additional key-value pairs.

	OP_RANGE,			// Create a range from two integers.

	OP_FORMAT_STR,		// Create a formatted string from multiple parts.

	/* Collections. */

	OP_GET_INDEX,		// Get an element of another object.
	OP_SET_INDEX,		// Assign to an element of another object.

	/* Functions. */

	OP_CALL_NAT,		// Call a native/built-in function.
	OP_CALL_DEF,		// Call a user-defined function.
	OP_RETURN,			// Return a value.
	OP_VOID,			// Load an invalid (void) return value.

	OP_VAR_ARGS,		// Initialize a parameter with a variable argument list.

	OP_METHOD,          // Create and store a type method from a loaded function object.

	OP_CLOSURE,			// Create a closure with an environment from a loaded function object.
	OP_CAPTURE_GLOBAL,  // Capture a value from the global scope into a cell.
	OP_CAPTURE_LOCAL,   // Capture a value from a surrounding local scope into a cell.
	OP_CAPTURE_CELL,	// Capture a cell from a surrounding scope.

	/* User Types. */

	OP_INSTANCE,        // Create a new instance of a given type.
	OP_FINISH_FIELDS,   // Finish initializing all fields for an instance.
	OP_INIT_FIELD,      // Initialize a field with a value. Does not check for mutability.
	OP_GET_FIELD,       // Retrieve an instance field.
	OP_SET_FIELD,       // Assign to an instance field.

	/* Modules. */

	OP_MODULE,          // Construct a module object at runtime.
	OP_GET_ENTRY,       // Retrieve a module entry.

	/* Loop specifics. */

	OP_MAKE_ITER,		// Generate an iterator over an object.
	OP_UPDATE_ITER,		// Increment an iterator over an object and loop.

	/* Internal opcodes. */

	OP_JUMP,			// Jump forward through the byte-code (unconditional).
	OP_JUMP_TRUE,		// Jump only if previous condition evaluated to true.
	OP_JUMP_FALSE,		// Jump only if previous condition evaluated to false.
	OP_LOOP,			// Loop back through the byte-code.

	OP_VAR,				// Make a variable mutable.
	OP_FIX,				// Make a variable immutable.
	OP_IMMUT,			// Mark a value as being immutable.
	OP_MUT,				// Mark a value as being mutable.

	OP_UNPACK,			// Distribute a collection of values across multiple registers.

	OP_BYTE_OPER,		// Operand is a single byte.
	OP_SHORT_OPER,		// Operand is two bytes.
	OP_LONG_OPER,		// Operand is four bytes.

	OP_ENTER_SCOPE,		// Mark that a scope is being entered (for internal use).
	OP_EXIT_SCOPE,		// Mark that a scope is being exited (for internal use).

	OP_LOAD_R,			// Load a constant into a register.
	OP_MOVE_R,			// Move a register's value into another register.
	OP_PRINT_VALID,		// Print the result of an expression (with exceptions).

	OP_HALT,			// Halt the program (executes at the very end).

	TOTAL_OPS
};

#define IS_VALID_OP(op)	(((op) >= OP_NEG_TWO) && ((op) < TOTAL_OPS))

constexpr std::array<std::string_view, TOTAL_OPS> opNames{
    #if OPCODE_STRIP_PREFIX
        #define LABEL(name, ...) std::string_view{#name}.substr(3),
	#else
	    #define LABEL(name) #name
	#endif

	#include "opcode_list.inc"

	#undef LABEL
};