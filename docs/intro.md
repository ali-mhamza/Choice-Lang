#### Hello and welcome! This is a brief file to help you navigate the different documentation directories, listing what each directory and file contains or should help with.
#### Directories and files are listed in alphabetical order.

## [Examples](./examples/)
#### The entire directory contains source code examples for different features, as illustrated by each file's title and/or header comment.
#### This is a great directory to look through if you are a new user of the language. The examples are generally straightforward, easy demonstrations of the language's features, aimed at beginners.

<br>

## [Internals](./internals/)
#### This directory holds any documentation regarding the internal workings of the interpreter, primarily for contributors or language enthusiasts.
#### If you simply wish to use the interpreter without getting into how it works internally, feel free to skip this directory entirely.

#### [config_internal.md](./internals/config_internal.md)
- This file provides careful guidance on various configurations and flags throughout the project's source files, including:
  - Which constants can/should be changed.
  - Valid values said constants can take.
  - Flags that can be given certain values, and the impacts of changing them.
- If you want to play around with printing output or verbosity, settings, etc., this is the file to mainly look at.

#### [debugging.md](./internals/debugging.md)
- This file provides additional guidance on how to use the interpreter's built-in debugging features and flags to debug your own scripts or (if you suspect a bug) the interpreter itself.

#### [opcode_structure.md](./internals/opcode_structure.md)
- This file lists out the format of each instruction/opcode used between the compiler(s) and the VM.
- It lists out operands (including their length, format, encoding, etc.), their significance/meaning, and (for some opcodes) how this is possibly reflecting in disassembling output.
- This is a crucial file to look at to truly understand the VM mechanics for each instruction.

<br>

## [Syntax](./syntax/)
#### This directory holds several documentation files covering different parts of the language's syntax.
#### While it is primarily meant for new users trying to learn/inquire about the language, it does include detailed features and aspects, offering a more comprehensive view of what the language has to offer.

#### [control_flow.md](./syntax/control_flow.md)

#### [functions.md](./syntax/functions.md)

#### [modules.md](./syntax/modules.md)

#### [syntax.md](./syntax/syntax.md)

#### [values.md](./syntax/values.md)

#### [variables.md](./syntax/variables.md)

<br>

## [Tools](./tools/)
#### This directory provides details on how to make use of all the options/variations that the interpreter has to offer.
#### Unlike [Internals](./internals/), this directory does not include any instructions involving modification of the project's source code. Included configurations or options can be used directly without any such changes.

#### [config.md](./tools/config.md)

#### [interpreter_options.md](./tools/interpreter_options.md)