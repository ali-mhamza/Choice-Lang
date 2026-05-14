## Control-Flow Structures

Choice has (6) primary control-flow structures that can be used:
- `if-elif-else` statements.
- `if-elif-else` expressions.
- `while` loops.
- `for` loops.
- List comprehensions.
- `repeat-until` loops.
- `match-is` structures.

Below is a description of how each structure works.

#### Note
A common point between all the control structures below is that they only accept *statement* bodies, not declarations. This means that, without a brace-enclosed block, no variables or functions can be declared in the body of a control-flow structure.\
This is primarily to make the scope of these variables explicit and easy to deduce (for the user and implementer).

<hr>

#### `if-elif-else` statements

Similar to Python or C-like languages, this control-flow structure allows you to execute a statement (or block) based on a condition specified after the `if` keyword.\
You may also specify an alternative condition/case with the `elif` keyword (as many times as you wish), followed (optionally) by an alternative `else` case.\
No branch (besides perhaps the initial `if` branch) is necessary to add here.

```
make x = 1;

if (x % 2 == 0)
    println!("Even");

if (x == 1)
    println!(1);
elif (x == 2)
    println!(2);
else
{
    println!(x);
    quit(1);
}
```

Borrowing from C, conditions must be enclosed in parentheses, and braces are optional for single-statement bodies.

<hr>

#### `if-elif-else` expressions

This is Choice's alternative to the ternary operator (which tends to bring many difficulties for both language users *and* implementers).\
The structure of the expression is as below:
```
if ([COND]) { [EXPR] } [elif ([COND]) { [EXPR] } ...] else { [EXPR] }
```
Put in words, there must be a condition after the `if` keyword, followed by a brace-enclosed *expression* (not a statement). This can then be followed by any number of similar `elif` clause.\
Unlike their statement counterparts, these expression must end with an `else` clause (so that the expression can always evaluate to a specified value).

```
make x = 1;
make y = if (x == 1) { 1 } elif (x == 2) { 2 } elif (x == 3) { 3 } else { nil };
```

This is primarily an improvement on Python's "ternary" operator by reorganizing the conditions and expressions.

<hr>

#### `while` loops

The syntax here is identical to that of C, except that declarations (and any other statements) are not permitted within the condition of the loop:

```
make i = 0;
while (i < 10)
{
    println!(i);
    i++;
}

while (make j = 0) // Error!
{
    println!(j);
    j++;
}
```

<hr>

#### `for` loops

`for` loops in Choice work by defining a variable that iterates over some iterable object, taking the value of each "item" in that object (whatever type or form that item may have).\
Below is an example of a simple for-loop, iterating over the numbers from 1-10 (inclusive):
```
for (i in 1..10)
    println!(i);

make x = 0;
for (i in 1..10)
{
    x += i;
}
```

There are some shared aspects here with other languages:
- The 'in' keyword and iteration syntax, shared with Python.
- The necessary use of parentheses to enclose the loop condition, as in C.
- The optional use of braces to enclose its body, also from C.

A primary different from Python's for-loops however is that the loop variable is not injected into the surrounding scope. Instead, its scope is limited to the body of the loop:
```
for (i in 1..10)
{
    println!(i);
}

println!(i); // Error!
```

Additionally, a condition can be placed on the loop variable. If the condition is true, the body of the loop runs with that value of the variable. Otherwise, the iteration is skipped entirely.\
The syntax for this is as below:
```
for (i in 1..10 where i % 2 == 1)
    print!(i);  // Prints: 13579
```

As can be understood, the conditional is inserted by using the `where` keyword after the iterable object, followed by an expression.

#### Detailed note
Though only one variable is used internally throughout every iteration of the loop, the *object* that variable holds is entirely new.\
This is particularly useful for closures, since it means any closure constructed within the loop captures an entirely independent variable, unaffected by any following iterations of the loop.

```
make x = [for (i in 0..9): || { // Constructing an array of lambdas.
    print!(i);
}];

for (fn in x)
{
    fn(); // Full output: 0123456789
}
```

<hr>

#### List comprehensions

`for` loops, like `if-elif-else` statements, also have an expression counterpart, though in a more minor form. List comprehensions can be used to iteratively generate the lists of an element.\
The syntax for list comprehensions can be illustrated with this example:
```
make list = [for (i in 1..10): i]; // list = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
```

The `for` loop used in a list comprehension is identical to a regular `for` loop, with two exceptions:
1. The expression following the loop header is separated from the header by a `:`, rather than braces (or other delimiter characters).
2. The loop does not have a proper "body". Instead, the loop header is followed by a single expression, which is evaluated to produce a new element of the list with each iteration.

Similar to `for` loops, list comprehensions can also have `where` conditional clauses to exclude some elements or cases:
```
make list = [for (i in 1..10 where i % 2 == 0): i]; // list = [1, 3, 5, 7, 9]
```

<hr>

#### `repeat-until` loops

This loop construct largely borrows from C's `do-while` loops, executing the body at least once and continuing to iterate over it while the condition at the end is "met".

However, there are two primary differences from C here:
1. The loop *stops* when the condition evaluates to true (hence the use of `until` rather than `while`), rather than when it evaluates to false.
2. Unlike `while` loops and `for` loops, the body of a `repeat-until` loop must be enclosed in braces since it sits between two keywords.

An example of this type of loop:
```
make i = 0;
repeat {
    print!(i);
} until (i >= 10); // Full output: 0123456789
```

<hr>

#### `match-is` structures

This is Choice's analog for the well-known `switch-case` structure found in C (with a plethora of alternatives in other languages).\
The basic syntax for the structure is as follows:
```
make x = 1;
match (x)
{
    is 1:
        println!(1);
    is 2:
        println!(2);
    is ?:
        println!("Unexpected value.");
}
```

The main points to note from the above example:
- The value to be matched should follow the `match` keyword and, as with other control-flow structures, is to be contained between parentheses.
- Each case uses the keyword `is` followed by an expression, which is then followed by a colon `:`.
- The default case uses the syntax `is ?`, rather than a `default` (or similar) keyword.

There are some important divergences from C's `switch-case` which are also important to note here:
- A case within the structure can only be followed by a single statement (multiple statements must be enclosed in a block).

- There is *no default fallthrough behavior*. This is a particularly irritating feature of C's `switch-case`.\
  However, the alternative removal of fallthrough altogether (a la Python) often also leads to unfavorable repetition.\
  To settle for a convenient median position, these are Choice's fallthrough rules for `match-is` structures:
  1. Cases with empty bodies get default fallthrough. This allows for easy stacking or layering of multiple cases that all share the same body.
  2. Cases with non-empty bodies have fallthrough disabled by default. To get fallthrough behavior, a `fallthrough` statement (the `fallthrough` keyword followed by a semicolon `;`) can be added to the end of the case body (since it is an independent statement, braces may be necessary to add).
  3. A `fallthrough` statement must be at the end of the case body, which makes it clear where the point of execution will continue from to the next case.

- The way to prematurely end a particular case body's execution (or to end case fallthrough) is with an `end` statement (the `end` keyword followed by a semicolon `;`). It functions exactly like C's `break` statement in the `switch-case` structure.\
  Though this involves remembering another keyword besides `break` (which is instead reserved for loops), it allows you to use `break` to exit a loop from within a `match-is` structure, since the interpreter is not forced to apply the `break` to the structure instead of the loop.

Currently, `match-is` structures are really syntactic sugar for multiple structured comparisons. There is planning in progress to optimize them into a more efficient instruction format.

<hr>

## More on loops, `break` and `continue`

Borrowing from C-like languages, `break` and `continue` statements can be used to exit a loop or skip to the following iteration of a loop, respectively.\
However, similar to Rust and Java, loops also accept labels which can be specified by a `break` or `continue` statement to exit an outer loop or skip to the following iteration of an outer loop.

```
make i = 0;
while (i < 10) : outer
{
    make j = 0;
    while (j < 10)
    {
        if (j == 1)
            continue outer;
    }
}

for (i in 1..10) : outer
{
    for (j in 1..10)
    {
        if (j == 5)
            break outer;
    }
}

// Add repeat-until example.

make x = 0;
repeat: outer {
    println!(x);

    make y = 0;
    repeat {
        if (y == 5)
            break outer;
    } until (y == 10);

} until (x == 10);
```

Additionally, borrowing from Python, `while` and `for` (but not `repeat-until`) loops support an additional `else` clause which is executed if the loop exits normally (i.e., the condition evaluated to false or iteration was completed) rather than with the use of a `break` statement.\
This is particularly useful for searching operations, since it can allow you to easily write a handler for when a particular value/element/etc. is not found.

```
make x = 1;
for (i in 10..20)
{
    if (i == x)
        println!("Found a match!");
}
else
{
    println!("No match found!");
}
```