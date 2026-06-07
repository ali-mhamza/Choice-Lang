# Choice

Choice is a dynamically-typed, interpreted language that supports static type-checking for safe code practies.

The source code compiles to a single binary which statically links all the needed components, tools and dependencies, allowing for easy use and delivery. Fully portable across different popular systems and architectures (Linux, MacOS, Windows, etc.).

## Build Instructions

The following targets (among others) are available in the project Makefile:
```
# Produces an optimized ./choice-release executable.
make release

# Compiles with -g, -O0 flags + UB and address sanitizers; produces ./choice-debug.
make debug

# Runs the available test suite.
make test

# Runs the tests in tests/[DIR].
make test-group GROUP=DIR
```

## Project Layout
```
include/        # Project header files.
src/            # Project source files.
test/           # Test suite files.
docs/           # Documentation markdown files.
dependencies/   # Headers and source files for various dependencies.
```

## Documentation

For a beginner-friendly guide to this project's documentation, feel free to go through [intro.md](docs/intro.md).\
It provides a thorough introduction to the various directories and documentation files for the project.