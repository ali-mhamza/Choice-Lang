## Types

As with most languages, Choice also supports user-defined types, declared using the `type` keyword:
```
type CustomType;
```
The above syntax can be used to concisely declare a type with no fields or methods.\
To declare fields and/or methods, place any fields, separated by commas, followed by any methods. Methods should still be declared with the `func` keyword.
```
type A {
    x, y

    func add() {}
    func print() {}
}
```
Some points to note with this example:
1. Fields (if any) must be declared *before* any methods.
2. Types declared with braces do not require a `;` after their declaration.
3. Types declared with braces do not need to contain any fields or methods. 
4. No comma should follow the last field.

To construct an instance of a user-defined type, the type's name should be called like a function:
```
type A;
make a = A();
println!(typeof!(a)); // Prints: <type A>.
```

Instance fields and methods are accessed using the `.` operator:
```
type A {
    x

    func print() {}
}

make a = A();
println!(a.x);
a.print();
```

Currently, Choice does not support flexible user-defined types. This means no new fields or methods may be added to a type instance at runtime. Thus, code like the below fails at runtime:
```
type A { x }

make a = A();
a.y = 1; // Error!
```

## Constructors

Choice supports four ways to initialize an instance of a particular type.\
The first, and simplest, way is with a user-defined constructor. The constructor must have the name `Self` (with this exact capitalization) to be recognized as the type's constructor:
```
type A {
    x

    func Self(a)
    {
        self.x = a;
    }
}
```

If no user-defined constructor is found, Choice supports two additional constructors, implemented by the language itself: a default constructor, and a "complete" constructor.\
The former takes no arguments. Any fields are initialized to `null` or use their default initializers (see section below for more detail on this):
```
type A { x }

make a = A();
println!(a.x); // 
```
The second ("complete") constructor accepts an argument for each field (even if it has a default initializer), assigning the passed arguments to each value in their declaration order within the type:
```
type A { x, y, z }

make a = A(1, 2, 3);
println!(a.x, a.z, a.y); Prints: 1 2 3.
```

The fourth and final initialization method supported for type instances is a memberwise initializer. This is constructed by using the type's name, followed by a brace-enclosed list of field-value pairs, as below:
```
type A { x, y, z }

make a = A {
    z = 2,
    y = 1
};
```
As can be observed from the example, fields can be omitted, and can be specified in any order, even if it does not match the original type declaration order.\
Any fields not initialized here will be initialized to `null` or use any specified default initializers.

*Note:* it is not allowed to place a `return` statement anywhere within a custom constructor, even to return an instance of the type. The constructor automatically returns the instance it initializes.

## Default Initializers

Type fields, if not initialized with a constructor or memberwise initializer, are automatically set to `null`.\
However, this can be changed by providing a field with a default initializer:
```
type A { x }      // 'x' is initialized to `null` if no initializer is given.
type B { x = 1 }  // 'x' is initialized to 1 if no initializer is given.
```
The initializer is only executed if the field has not been initialized in another manner. It is not computed or stored in advance.

Unfortunately, Choice does not currently support any way to use previous fields in a later field's default initializer. Thus, the below code would fail to compile:
```
type A { x = 1, y = x }
```

## Mutability

To declare an immutable field, place the `fix` keyword before the field name within the type declaration:
```
type A { fix x = 1, fix y = 2}
```
Similar to other immutable variable declarations (see [variables.md](./variables.md) and [mutability.md](./mutability.md)), a default initializer is required, and the initializer value is immutable by default, unless otherwise specified with `mut` keyword.

Any attempt to modify an immutable field will fail at runtime with an error.

## Methods

As mentioned above, methods can be declared after all fields (if any) have been declared. Method calls implicitly pass the instance they are called upon as the first argument. However, no additional parameter should be declared for the called-upon instance. The instance object is accessed through the pre-defined variable `self`:
```
type A {
    x, y

    func print() // Defined with zero parameters.
    {
        println!(self.x, self.y);   
    }
}

make a = A(1, 2);
a.print(); // Called with zero arguments.
```

*Note:* the `self` parameter has variable immutability (see [mutability.md](./mutability.md) for more details on this concept in Choice). This means it cannot be reassigned within a method, even to a different instance of the same type:
```
type A {
    func try()
    {
        self = 1; // Fails to compile.
    }
}
```