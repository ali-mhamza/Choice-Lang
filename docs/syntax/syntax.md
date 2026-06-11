## General

Choice's syntax is primarily inspired by a mixture of Python, C, and (much less) Rust. It is primarily designed to be terse and concise, as well as having a good deal of variety between its different constructs (e.g., for-in, repeat-until, etc.) to reduce confusion between similar concepts or syntax.

Choice source files are required to end with the ".ch" extension.\
Additionally, the interpreter has an available option to compile and save the bytecode for a source file in a bytecode cache file of the same name (but with a ".chbc" extension).\
Bytecode cache files are, similarly, required to end with the ".chbc" extension.\
Files used to store debug metadata end instead with a ".chdbg" extension (see [interpreter options](../tools/interpreter_options.md) for more details on the relevant options here).\
The extensions are not interchangeable (e.g., trying to execute a ".chbc" file or directly disassemble a ".ch" file will not work).

## Comments

This is a single-line comment.
```
// Single-line comment.
```
For multi-line comments, wrap the comment body in a `#`:
```
#
Multi-line
comment.
#
```
This syntax for multi-line comments does not support nesting. To comment out a large block that contains any (single-line or multi-line) comments, wrap the block with `###`:
```
###
#
First
comment.
#

var x = 1;

#
Second
comment.
#
###
```

## Reserved Words

Below are all the reserved words within Choice:
```
// Keywords.

and, break, class, continue, def, elif, else, end, func, for,
fallthrough, fix, false, if, is, immut, in, match, make, mut,
null, not, or, repeat, true, until, while, where

// Built-in types.

Int, Dec, Boolean, Null, Type, Func, String, Range, List, Table, Void, Any, Class
```

There are also pre-defined constants and identifiers. While they can still be used as identifiers for declarations, this should strictly be avoided, since they represent important data provided to the user as part of the VM runtime.\
Below are the constants currently defined:
```
_file_  // At global (file or REPL) scope.
_func_  // At function scope.
```

## Identifiers

Identifiers in Choice can start with any letter or an underscore `_`, and thereafter can contain any letter, `_`, or digit. There is current work in progress to extend identifiers to support valid unicode characters, but it's more difficult than it may seem!

## Whitespace

Following C-like languages, whitespace in Choice is not relevant.\
Additionally, the interpreter will automatically remove any whitespace characters (with the exception of the newline `\n`, tab `\t`, or `' '`) from any input it receives. This allows for further compatibility on systems where line-breaks are done with a `\r\n` sequence.

## Blocks

...

## Precedence and Associativity

...