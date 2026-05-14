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