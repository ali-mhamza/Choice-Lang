#### The interpreter executable built with this project's source has some very useful command-line options which are worth introducing here in detail.
**Command-Line Instruction Format:**
```
EXECUTABLE_NAME FLAG [FILE]
```
**Note:** Attempting to use a particular option marked "No" in the `Applies to` section will result in an error.

<hr>

### Default (Execute)
- Flags - N/A
- Applies to:
  1. REPL - Yes
  2. Source files - Yes
  3. Bytecode files - No

This is the default option when the interpreter is run, whether in the REPL or with a named source file.\
As expected, the interpreter will simply execute the provided code, executing statements from top to bottom and reporting errors to the user (if any).

<hr>

### Display Tokens
- Flags: `-t`, `-token`
- Applies to:
  1. REPL - Yes
  2. Source files - Yes
  3. Bytecode files - No

This option will break the given code down into its individual tokens (the same that would be forwarded on to later phases in the interpreter pipeline), displaying them with informative data (position, content, type, etc.) on the screen.

<hr>

### Display Bytecode
- Flags: `-b`, `-bytecode`
- Applies to:
  1. REPL - Yes
  2. Source files - Yes
  3. Bytecode files - No

This option will fully compile the given code (REPL input or source code from a given file), display the resultant bytecode that would be executed by the interpreter VM with additional information (operands, values of constants or literals, nested functions, etc.).

<hr>

### Cache Bytecode
- Flags: `-c`, `-cache`
- Applies to:
  1. REPL - No
  2. Source files - Yes
  3. Bytecode files - No

This option will fully compile the given source file, saving the resultant bytecode in a file of the same name (in the current directory), but with a ".chbc" extension instead of a ".ch" extension.\
No output is shown on the screen for this option.\
The resultant bytecode cache files may be used to reduce compile times for unchanged source files, in combination with the two options below.

This option has three sub-options, which can be specified by using the corresponding flag after the main option flag. The three options specify how debug metadata (used for error reporting) is stored with the encoded bytecode:

1. Combined (`-c`/`-combined`) - Default.\
This option tells the interpreter to store generated debug metadata in the same file as the encoded bytecode. This produces a single bytecode file, though with a considerable size increase.\
This is the default option if no option is specified with this flag.

2. Separate (`-s`/`-separate`).\
This option tells the interpreter to generate two files in the same directory: one containing the encoded bytecode (with a ".chbc" extension), and another containing the debug metadata (with a ".chdbg" extension).\
Both files are then loaded for disassembly or execution, just as in the `Combined` option.\
***Note:*** Only the bytecode file needs to be specified on the command-line when using this option. The interpreter will automatically fetch the metadata file contents (if the file is found in the same directory).\
This is a useful option to have slimmed-down bytecode files that still support detailed runtime error information.

3. Stripped (`-n`/`-nodebug`).
This option is very simple. No debug metadata is generated here. This allows for very minimal bytecode files, but will lead to very generic runtime error information and stack traces.\
This option is best suited for already-checked code that should (reasonably) not require any detailed error-reporting.

<hr>

### Disassemble Bytecode
- Flags: `-d`, `-dis`
- Applies to:
  1. REPL - No
  2. Source files - No
  3. Bytecode files - Yes

This option will load the cached bytecode from the given ".chbc" file, reconstruct the serialized bytecode and constant pool into a bytecode object, and thereafter disassemble the bytecode with an identical format to option `Display Bytecode`.

<hr>

### Load Bytecode
- Flags: `-l`, `-load`
- Applies to:
  1. REPL - No
  2. Source files - No
  3. Bytecode files - Yes

This option will load the cached bytecode from the given ".chbc" file, reconstruct the serialized bytecode and constant pool into a bytecode object, and thereafter disassemble the bytecode with an identical format to option `Display Bytecode`, passing it forward to the VM to be executed.

<hr>

### Explain Error
- Flags: `-e`, `-explain`
- Applies to:
  1. REPL - No
  2. Source files - No
  3. Bytecode files - No

This option accepts a single error or warning code argument (an 'E' or 'W', respectively, followed immediately by the four-digit code) on the command-line, and displays further detail about the diagnostic to aid the user.\
This option, as can be observed, does not interact with any files, and is instead to help the user understand foreign diagnostic messages or issues.\
The option is currently a work-in-progress, providing only basic information about the provided diagnostic.

<hr>

**Optimization Note:**\
To reduce the need for redundant recompilation of source files that have not changed, the interpreter will automatically check for the presence of bytecode cache files matching a given source file in the following options:
<!-- no toc -->
- [Default (Execute)](#default-execute)
- [Display Tokens](#display-tokens)
- [Display Bytecode](#display-bytecode)

If such a cache file is found, and the source file's modification time is not later than that of the cache file, compilation is skipped entirely.\
If the chosen option was to cache the source file's compile bytecode (and the cache file follows the same debug metadata encoding being requested by the user), the interpreter does nothing and exits (as the bytecode is already up to date).\
Otherwise, the bytecode object is reconstructed from the cache and executed or disassembled, depending on the chosen option.