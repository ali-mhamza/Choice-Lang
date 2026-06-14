## Numbers

Choice supports two numeric types: 64-bit signed integers, and 64-bit floating-point values (doubles).

Integer numeric literals can be written in decimal, binary (with a `0b` prefix), octal (with a `0o` prefix), and hexadecimal (with a `0x` prefix). Any invalid digits for a given base will result in a compilation error.
```
make a = 100;       // a = 100
make b = 0b1100100; // b = 100
make c = 0o144;     // c = 100
make d = 0x64;      // d = 100

a = 100A;   // Error!
b = 0b02;   // Error!
c = 0o09;   // Error!
d = 0x0G;   // Error!
```

Floating-point values accept both fixed-point and scientific notation:
```
make x = 1.2;   // Fixed-point.
make y = 12e-1; // Scientific notation.
```

Any numeric literals using scientific notation have type `Dec` (i.e., floating-point value), even if they evaluate to an integer.
```
make x = 1.2e1; // x = 12, but is still a Dec object.
```

For convenience, a single apostrophe character (`'`) may be used anywhere in a numeric literal as a digit separator for both integer and floating-point literals, though it must be sandwiched between two digits (including letter digits for hexadecimal literals).\
Thus, among other possible places, it cannot be at the beginning of a literal, the very end, used consecutively, or placed directly before/after a decimal point.


## Strings

Strings in Choice have a wide variety of useful options and features.\
Regular strings are placed between double-quote marks:
```
make x = "Hello, world!";
```
Using single-quote marks is a syntax error. They aren't permitted for any string format:
```
make x = 'Hello, world!'; // Error!
```
Regular strings as displayed above must be on the same line.

### Multi-line Strings

To construct a multi-line string, back-tick characters are instead used as the string delimiters:
```
make x = `Hello,
world!`;
```

A single leading or trailing newline within a multi-line string will be removed.\
Thus, the following strings are equivalent.
```
make x = `Hello, world!`;
make y = `
Hello, world!
`;
```
This allows you to more easily write JSON (or JSON-like) strings without having to put part of the text next to a quote mark.

### Escape sequences.

As with other languages, a number of escape sequences are accepted in strings:
```
\n
\r
\t
\\
\"
\`
```
It's worthwhile to note that `\'` is not a supported escape sequence (it is left unmodified within the string), since the single-quote mark is not a significant string delimiter in Choice.

In addition to the above, strings in Choice also support encoding characters in binary, octal, hexadecimal, or unicode using certain escape sequences.\
For all the below escape sequences, a minimum of 1 digit is expected following the escape sequence character. Any less than that will result in a compilation error.\
For encodings other than unicode, extra digits or digits not fitting the accepted pattern after the first digit (e.g., '2' for binary) will not be consumed.

### Binary

For binary, the `\b` escape sequence is used, followed by 1-8 binary digits:
```
make x = "\b1000001"; // "A"
```

### Octal

For octal, the `\o` escape sequence is used, followed by 1-3 octal digits:
```
make x = "\o101"; // "A"
```
It is important to note that, even though 3 digits are allowed, the value cannot exceed the maximum for a byte (255). If the value exceeds this maximum, an error is reported:
```
make x = "\o777"; // Error!
```

### Hexadecimal

For hexadecimal, the `\x` escape sequenced is used, followed by 1-2 hexadecimal digits:
```
make x = "\x41"; // "A";
```

### Unicode

Since unicode codepoints tend to conventionally be written in different ways (and may not be as convenient to pad with 0s), the syntax instead follows Rust: the `\u` escape sequence is used, followed by 1-6 hexadecimal digits enclosed between braces `{}`:
```
make x = "\u{1F600}";  // 😀
```

Extra digits or any non-hexadecimal digits in this case are not ignored, and are instead compilation errors:
```
make x = "\u{1234567}"; // Error!
```
Additionally, any unicode values in the UTF-16 surrogate range (U+D800 - U+DFFF) are invalid and will result in compilation errors.

### Raw Strings

Strings by default obey the above formatting rules for escape sequences, encoding, etc. To disable such formatting, *raw strings* can be used by adding the `r` prefix to a string:
```
make x = r"Hello\nworld!"; // Hello\nworld!
```
They similarly apply to multi-line strings:
```
make x = r`Hello\n
world!`;
```
However, it should be noted that single-line raw strings still require that any double-quote marks in the middle of the string (or back-ticks, for multi-line strings) be escaped (though they are not formatted with the \ being removed).

Multi-line raw strings can allow a user to insert double-quote marks within a string, such as within JSON:
```
make x = `
    {
        name: "John",
        age: 12
    }
`;
```


## Booleans

Choice has two Boolean values: `true` and `false`. It should be noted that Booleans in Choice are not convertible to integers (e.g., 1 and 0 respectively), as in other languages.\
Thus, operations like below will fail due to a type mismatch:
```
println!(1 + true);
```


## Lists

Choice, like most higher-level languages, has its own list/array type, which happens to be a dynamically-sized, homogeneous collection type.

Lists can be created in three different ways:
1. List literals - A list can be constructed directly as a literal with a bracket-enclosed, comma-separated list of values:
    ```
    make x = [];        // Empty list.
    make y = [1];       // Single element.
    make z = [1, 2, 3]; // Multiple elements.
    ```
    See [control_flow.md](./control_flow.md) for more details on an alternative literal syntax: list comprehensions.
2. Variable argument lists - All arguments passed to a variadic parameter are automatically consumed and combined into a list named by that parameter:
    ```
    func A(a...) // 'a' is a variadic parameter.
    {
        for (i in a)
            println!(i);
    }

    A(1, 2, 3); // Parameter 'a' contains the list: [1, 2, 3].
    A(1);       // Parameter 'a' contains the list: [1].
    A();        // Paramater 'a' contains an empty list: [].
    ```
3. Multiple return values - To return multiple values from a function, you can add a comma between the different return values. This creates a list (without the needing for brackets) containing these values and returns it from the function.
    ```
    func A()
    {
        return 1, 2, 3;
    }

    make x = A(); // x = [1, 2, 3]
    ```

Lists have the regular index operator []:
```
make x = [1, 2, 3];
println!(x[0]); // Prints: 1
x[0] = 10;
println!(x);    // Prints: [10, 2, 3]
```
Currently, the operator only supports single indices, and thus one cannot access a range of elements. The index must also be positive.\
Attempting to access an index outside the range [0, list-length) will result in a runtime error.

Like strings and ranges (see [Ranges](#ranges)), lists are also iterable, and thus can be looped over within a for-loop:
```
make x = [1, 2, 3];
for (i in x)
    println!(i); // Prints 1, then 2, then 3.
```


## Tables

Choice also has its own associative array/hash table type, which are concisely called "tables". As is common between languages with this feature, tables allow a person to associate a key object with a value object.

Tables can be created using table literals, which involve a brace-enclosed series of values, each between parentheses and separated by a comma:
```
make x = {};                // Empty table.
make y = {(1, 2)};          // Single pair.
make z = {(1, 2), (2, 4)};  // Multiple pairs.
```

As with lists, tables also have a defined [] operator:
```
make x = {(1, 2)};
println!(x[1]); // Prints: 2.
x[1] = 3;
println!(x[1]); // Prints: 3.
```
Similar to lists, attempting to get the value at a non-existent key will result in a runtime error:
```
make x = {(1, 2)};
println!(x[2]); // Error!
```
However, unlike lists, assigning to a non-existent key will simply add the key and value as a new pair to the table:
```
make x = {};
x[1] = 2; // Compiles and runs without errors.
println!(x); // Prints: {(1, 2)}.
```

Currently, the table supports the use of any object types as both keys and values. While this allows for more flexibility, it also weakens the guarantees given by the table regarding its keys, since they may be externally modified:
```
make x = [1, 2, 3];
make map = {(x, 1)};
x[0] = 10;

// This now fails since the key is actually [10, 2, 3].
println!(map[[1, 2, 3]]);
```
Some work towards resolving this issue is in plans.


## Ranges

Choice offers the Range type as well, which is a series of integer values with a defined start and end, as well as (possibly) a custom step size between them. Ranges are **inclusive** for both ends.

Ranges can be constructed in two ways:
1. Range literals - The '..' operator can be used on two integers to create a range object:
    ```
    make x = 1..10;
    ```
    The '..' operator has low precedence (only above comparison operators) to allow users to place very flexible expressions on each side of the operator.\
    Since range literals are constructed from only two integer values, both are used (respectively) as the start and end values, with a default value of 1 or -1, depending on the direction of the range (see below).
2. The `range!()` built-in function - This function accepts 2 arguments (the start and end values) as well as an optional 3rd argument for the step size, and constructs a range object from all three:
    ```
    make x = range!(1, 10);
    make y = range!(1, 10, 2);
    ```

The sequence of values represented by a Range object can be ascending as well as descending order, as determined by the start and end values. For convenience, when no step size is specified, a step size of 1 is assumed if start <= end, and -1 otherwise.

Similar to other collection types, ranges support iteration, indexing, and member checks:
```
make x = 1..10;

for (i in x)
    println!(i);    // Prints numbers 1-10, inclusive.

println!(x[0]);     // Prints: 1.
println!(2 in x);   // Prints: true.
```
Like lists, accessing an index which involves stepping outside the sequence of values that the Range object represents will result in a runtime error.\
Currently, the [] operator only supports single indices, and thus one cannot access a subsequence of numbers from the Range object. The index must also be positive.


## Null

Null is a very simple value type. It is specifically represented by, and created with, the `null` keyword/literal.\
`null` is the default value for an uninitialized variable, and represents the lack of a value within an object.


## Types

To facilitate type-checking in the interpreter (e.g., for branching code depending on an object's type), Choice provides the built-in `type!()` function.\
Since Choice currently lacks 'class' objects (or user-defined types altogether), this function instead returns a value of a unique type: Type.\
Internally, this is simply a numeric value representing the type of an object. However, it does not behave like an integer object, and cannot be used as such.

Examples:
```
make x = type!(1);
println!(x);        // Prints: Int.
println!(type!(x)); // Prints: Type.
```


## References

Since Choice uses a pass-by-sharing model (i.e., shallow copying any heap-allocated data, including strings, lists, etc.), passing a copy of an object is sufficient to mutate it within a function:
```
func swap_first_two(list)
{
    if (len!(list) < 2) return;

    make temp = list[0];
    list[0] = list[1];
    list[1] = temp;
}

make x = [1, 2, 3];
swap_first_two(x);
println!(x); // Prints: [2, 1, 3]
```

However, if the user wishes to reassign the variable itself to another value within a function, and have that change be reflected after the function exits, this will not be sufficient.\
To resolve this, a user can instead create a *reference* to a variable, and pass that to the function instead.

References follow three rules:
1. They are created with the '*' operator preceding the referenced variable.
2. References may only reference variables, not bare values. There is work in progress to expand references to be able to refer to collection elements or (once user-defined types are supported) fields as well, though both are currently unsupported.
3. References may only be created in function calls.

Example:
```
func A(x)
{
    x = 2;
}

make x = 1;
A(*x);
println!(x); // Prints: 2.
```
As can be observed from the example, references do not need to be "dereferenced". They are interacted with the same way as with a regular variable.

Attempting to use references to modify an immutable variable will result in a runtime error:
```
func A(x)
{
    x = 2;
}

fix x = 1;
A(*x); // Error!
```
See [mutability.md](./mutability.md) for more on immutable variables.


## Void

The Void type is specifically reserved for function calls that did not return a value ('calls' are specified here, since a single function is allowed to return different numbers or types of values), and prints as an empty pair of parentheses:
```
func A() {}
println!(type!(A())); // Prints: Void.
```

A function returning null is still considered to be returning a value, and thus does not return a value of type Void:
```
func A()
{
    return null;
}

println!(type!(A())); // Prints: null.
```