# Choice

Choice is a dynamically-typed, interpreted language that supports static type-checking for safe code practies.

The source code compiles to a single binary which statically links all the needed components, tools and dependencies, allowing for easy use and delivery. Fully portable across different popular systems and architectures (Linux, MacOS, Windows, etc.).

## Build Instructions

The following targets (among others) are available in the project Makefile:
```
make release        # Produces an optimized ./choice-release executable.
make debug          # Compiled with -g, -O0 flags + UB and address sanitizers; produces ./choice-debug.
make test           # Runs the available test suite.
make test-[dir]     # Runs the tests in tests/[dir].
```

## Project Layout
```
include/        # Project header files.
src/            # Project source files.
test/           # Test suite files.
docs/           # Documentation markdown files.
dependencies/   # Headers and source files for various dependencies.
```

## Documentation Guide
[internals](docs/internals/)    — Inside view on how (some of) the interpreter works.\
[syntax](docs/syntax/)          — A user-friendly guide on the language's rules, features and syntax.\
[tools](docs/tools/)            — Different tools built in to the interpreter for different uses cases or interests.