## Functions

Most languages have a way to describe procedures or functions, which are blocks of code that can be invoked at different places within user code, possibly with modified parameters. Choice here is no different, though it combines a few features found in other languages.

Functions in Choice are first-class objects. This means, unlike languages like C or C++, functions are also variable or objects, just like integers, Booleans, etc. They can be stored in variables or collections, passed to other functions, and so forth.

*Note:* passing too many or too few arguments to a function results in a runtime error. This also applies to different callable objects, such as built-in functions, built-in and user-defined types, and so forth. The maximum number of parameters for a function or arguments for a function call is 255 (the largest value that fits in a byte). A larger number of either will lead to a compilation error.

## Declaring a Function

Functions are declared with the `func` keyword. An example:
```
func name(param1, param2, param3)
{
    println!(param1 + param2 + param3);
}
```
As can be observed, the `func` keyword should be followed by the function name, and thereafter the parameter list between parentheses. The function body should then follow, enclosed between braces.

It is fully allowed to declare functions within each other:
```
func A()
{
    func B() {}
    func C() {}
}
```

## Function Name

The name of a function (if it has one; see the [lambdas](#lambdas) section below) must fit within a single byte. Thus, the length of a function's name must be between 1 and 255 characters.

For various utility purposes, a local variable with the name `_func_` is defined within each function, with a string value holding the function's name (or "lambda" if it is an anonymous function).

## Returning from a Function

Any function in Choice that does not explicitly return a value will implicitly return a value of type `Void` (see [values.md](./values.md) for more on value types in Choice).\
Choice also supports early and/or explicit returns from a function via a `return` statement, which is the keyword `return` followed (optionally) by a sequence of values to return:
```
func A() {}
func B() { return; }
func C() { return 1; }
func D() { return 1, 2, 3; }
```
In functions `A` and `B`, no value is explicitly returned. Thus, they both return a value of type `Void`.\
Function `C` returns the value 1, and thus any calls to `C` will evaluate to 1.\
Function `D` returns a sequence of values. This is syntactic sugar for the following:
```
func D() { return [1, 2, 3]; }
```
Thus, `D` returns a list containing the returned values in order. These values can then be unpacked (if needed) at the function call-site.

## Closures

Since functions are first-class objects in Choice, and since some functions declared within a local scope (either a regular block scope or function scope) may reference other local variables, functions need some way to maintain access to these local variables outside this scope.

Take this example:
```
func A()
{
    make x = 1;
    func B()
    {
        println!(x);
    }

    return B;
}

make a = A();
a(); // Should print: 1.
```
For the above to work, the returned function object referenced by `B` must be able to maintain access to the local variable `x`, even after `x` has gone out of scope.

To support this, Choice has closures, which are still function objects, yet with a captured series of variables that persist even after going out of scope within the user's code. This allows the above example to run as expected.

It is important to note that closures captures *variables*, not *values*. Thus, any modification to a captured variable persists across function calls:
```
func makeCounter()
{
    make count = 0;
    func counter()
    {
        return ++count;
    }

    return counter;
}

make counter = makeCounter();
println!(counter()); // Prints: 1.
println!(counter()); // Prints: 2.
println!(counter()); // Prints: 3.
```
Likewise, a variable captured by multiple closures is shared between them. Any modification to the variable within one closure is thus reflected across the others:
```
func closure()
{
    make x = 1;

    func set()
    {
        x = 2;
    }

    func get()
    {
        println!(x);
    }

    return set, get;
}

make set, get = closure();
set();
get(); // Prints: 2, not 1.
```

## Lambdas

Despite functions being first-class objects in Choice, and thus often being stored in variables or passed around as regular values, function declarations are still statement-level constructs. This means they cannot be directly used as expression in assignments, function call arguments, initializers, etc.

To resolve this, Choice supports anonymous functions, often called lambdas. As their title suggests, they don't have a name (unlike regular functions), but otherwise operate just the same.\
Lambdas are created by placing the function parameter list between a pair of vertical bars `||`, followed by the brace-enclosed function body:
```
make x = || { println!("lambda!"); };
make y = |a, b| { println!(a + b); };
```
Note the terminating semicolons, since lambdas are still just expressions (like any other initializer here).

Lambdas can also be created and immediately called with arguments:
```
make x = (|a, b| { return a + b; })(1, 2); // x = 3
```

For concise lambdas that simply return an expression value without needing a body, users can replace the brace-enclosed body with the `=>` arrow, followed by the expression to be returned:
```
make x = |a, b| => a + b;
make y = (|a| => a ** 2)(2);
```
As can be noted from the second example, lambda expressions using this syntax should be enclosed in parentheses if they are to be immediately called to avoid the call operator being consumed as part of the returned expression.

## Default Arguments

Functions in Choice also support default values for parameters. If no argument is passed in place of that parameter, the default value is used instead. Default values are added by placing an `=` sign after the parameter name, followed by the default value:
```
func A(a = 1, b = 2)
{
    println!(a + b);
}

A();      // Prints: 3.
A(2);     // Prints: 4.
A(2, 3);  // Prints: 5.
```

Parameters with default values cannot be followed by regular parameters in a function declaration. Attempting to place them in this order leads to a compilation error:
```
func A(a = 1, b) {} // Error!
```
However, they can still be followed by variadic parameters (see section below).

Differing from Python, default values are not pre-computed for functions. While this is slightly less efficient (since default values have to evaluated each time they are used), it avoids surprising or unintuitive behavior when default values are computed without being used or needed (which may impact other data, if they contain mutating expressions), or when they are re-used across calls.

## Variadic Parameters

Expanding on default values, functions in Choice also support variadic parameters. A variadic parameter effectively "collects" all arguments passed to a function (that have not already been used to initialize regular parameters, including those with default values).

A variadic parameter is declared by appending an ellipsis (`...`) *after* the variadic parameter name:
```
func test(a, b, c...)
{
    println!(a, b);
    for (x in c)
        println!(x);
}

test(1, 2, 3, 4, 5); a = 1, b = 2, c = [3, 4, 5]
```
A variadic parameter, as can be seen, has a list value containing all the remaining arguments.

It is allowed for a variadic parameter to receive 0 arguments, or receive the maximum number of arguments (255). In the former case, the value of the parameter is an empty list:
```
func test(a...)
{
    println!(a);
}

test(); // Prints: [].
```

No other parameters can follow a variadic parameter, whether regular parameters or default parameters.