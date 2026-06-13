## Intro

To maintain data integrity as much as possible, Choice introduces useful - though complex - mechanisms to guarantee certain mutability constraints surrounding both variables and values.\
Since these mechanisms have intricate parts and details, a thorough explanation of them is necessary to avoid confusion or errors.

## Variable Mutability

Variable (im)mutability is controlled by the variable declaration syntax.\
To declare a mutable variable, use the `make` keyword:
```
make x = 1;
```
To declare an *im*mutable variable, use the `fix` keyword:
```
fix x = 1;
```
One of the two keywords must be used for regular variable declarations. Since the default for function parameters and loop variables is variable mutability, the `make` keyword does not have to be used:
```
func A(x, y) {}
for (i in [1, 2, 3]) {}
```
However, the `fix` keyword is still needed to enforce variable immutability in both cases:
```
func A(fix x, y) {}
for (fix i in [1, 2, 3]) {}
```

Attempting to reassign (through the `=` operator or any compound-assignment operators) or increment/decrements an immutable variable will result in either a:
- Compile error, if the operation is carried out on the variable explicitly.
- Runtime error, if the operation is carried out on the variable implicitly through a reference within a function.

It should be noted here that variable immutability does not *necessarily* guarantee the immutability of the value contained within the variable.\
To elaborate, variable immutability only guarantees that the variable may not be assigned to after declaration. Thus, the following would fail:
```
fix x = 1;
x = 2; // Error!
```
However, it does not necessarily guarantee that the value stored within the variable may not be internally mutated. Thus, this is allowed:
```
make x = [1, 2, 3];
fix y = x;      // Value in 'y' is mutable, while 'y' itself is immutable.
y[0] = 10;      // Compiles without errors.
println!(y);    // Prints: [10, 2, 3]
```


## Value Mutability

Value mutability controls the interior mutation that a particular value accepts or does not accept. It should be clear that the following, therefore, only applies to object types that can accept interior mutation (e.g., lists), and thus integers, Booleans, etc. are excluded from the below explanation.

The mutability of a value is primarily controlled in the language through the `mut` and `immut` keywords. The former makes the modified value mutable, while the latter makes it immutable.\
These operators have the *lowest* precedence to allow them to be applied to any expression. The only exception to this is multiple return values (`return [EXPR], [EXPR], ...;`), where they only apply to the immediately following expression (rather than the entire collection).

Any attempts to mutate an immutable variable will result in a runtime error:
```
make x = immut [1, 2, 3];
x[0] = 10; // Error!
```

An important point to note is that two separate copies of a single internal piece of data may be mutable and immutable across separate copies. To give an example:
```
make x = mut [1, 2, 3];
fix y = immut x;
x[0] = 10;  // Compiles and runs without errors.
y[0] = 10;  // Fails at runtime, but 'y' has already been mutated.
```
Since both the copies in 'x' and 'y' refer to the same list, any mutation to the value in 'x' is also reflected in 'y'. In this case, the purpose of making the value in 'y' immutable would be to have an immutable *view* into the list stored in 'x', though caution should be maintained to not assume the value in 'y' is entirely untouchable here.\
The following sections expand upon certain aspects of this.


## Default Value Mutability

The default mutability of a value depends on whether the value is being newly produced or if it already exists (e.g., in a variable).

### New Values

The mutability of a newly-produced value within the interpreter is determined by the following factors, in order of priority:
1. The `mut` and `immut` keywords.
   - These keywords will override any other mutability for the value determined from any other factor.
   - Both keywords can be used anywhere to modify a value, including outside a declaration, e.g., `call(mut [1, 2, 3])`.
2. Variable location (with the `make` and `fix` keywords).
   - The default for any newly-produced value stored in a mutable variable is that it is mutable, and immutable if it is stored in an immutable variable.
   - This applies to both declarations as well as assignments (to a mutable/immutable variable).
   - This is primarily for user convenience, to guarantee (im)mutability on both levels by default, which is what most users would expect. Without this guarantee, the following code would become required too often:
        ```
        make x = mut [1, 2, 3];
        fix y = immut [1, 2, 3];
        ```
3. The default mutability of the value type.
   - All value types are mutable by default, with the exception of string literals, which (to maintain internal consistency in stored literals) are immutable by default.

### Existing Values

The mutability of an existing value is more nuanced. An existing value, given the above three factors, will always have its own independent mutability. This mutability cannot be directly modified, though any copies of the object may acquire a modified mutability flag through the use of the `mut` and `immut` keywords (see the next section for more details on this).

To give some examples:
```
// List here is mutable.
make x = [1, 2, 3];

// Even though 'y' is an immutable variable,
// the list maintains its current mutable status.
fix y = x;

// Compiles and runs without errors.
y[0] = 10;

// Creating an immutable 'view' into the list in 'x'
// (original list in 'x' is still the same, i.e., mutable).
fix z = immut x;

// Results in a runtime error.
z[1] = 20;
```


## Changing Mutability

As other sections have alluded to, you cannot directly change the mutability status of an already-existing value. However, when making any copies of the value (e.g., to store in other variables, or pass as a function argument), you can apply a new mutability flag, once more using the `mut` and `immut` operators.

Applying `immut` to a mutable value, as mentioned in [Variable Mutability](#variable-mutability), does not make the original copy immutable, but rather only creates an immutable *view* or *handle* to that value. Thus, while the value may still be mutated through the original, it will not be mutated through this immutable copy, providing an additional guarantee of data protection.\
Accordingly, performing this transformation will result in a warning to make this (possibly surprising) behavior clear.

Applying `mut` to an immutable value, on the other hand, will result in a runtime error. This applies to both values that are inherently immutable, as well as immutable views or handles.\
This is primarily done to prevent a user from trying to modify an immutable value (this is mimicked in other languages by prohibiting casts from 'const' to non-'const').