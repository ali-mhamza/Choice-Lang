## Variables

### Declarations

To declare a variable, use the keyword `make`:
```
make x = 1;
make y;
```
To declare a fixed-value variable, replace `make` with the keyword `fix`:
```
fix x = 1;
```

### Assignments

Assigning to a variable works the same as in other programming languages:
```
make x = 2;
x = 4;
```

Of course, since the language is dynamically typed, it is allowed to assign values of different types to a single variable:
```
make x = 1;
x = "Hello, world!";
x = true;
x = null;
```