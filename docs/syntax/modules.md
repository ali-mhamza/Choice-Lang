## Modules

To make the creation of larger projects easier, Choice supports multi-file source code with the use of "modules".\
A module object is created at runtime and contains any global-scope objects defined within another Choice source file.

When a user imports a module (see section below for instructions on how this is done), the source code within the associated source file is executed once, and any necessary objects within the module's global scope become accessible.\
Modules are automatically cached. Thus, any re-imports (whether for specific entries, or even with an alias) do not require executing the module's associated source file twice. This also entails that any modifications to the module's source file will not be reflected.

To access a particular entry within a module object, the scope operator `::` should be used:
```
println!(mod::x);
mod::fn();
```

Choice does not currently support modification of module entries. Thus, code like the below will fail at compile-time:
```
mod::x = 1;
```

## Importing Modules

Importing a module directly creates a module object at runtime, allowing a user to access the module's entries through it using the aforementioned scope operator `::`.

To import a module, the `use` keyword is to be used:
```
use module;
```
This imports the source code within the source file `module.ch` in the current directory. It should be noted here that the module name is not quoted, and the `.ch` extension is omitted.

Imports resolve by default to the current directory. To import a module within a particular directly, use a `from` clause after the import:
```
use module from "module_dir";
```
Unlike the module name, the directory name is placed within quotes. A terminating "/" for the directory name is allowed but not required.

To avoid name collisions, shorten module names, etc., Choice supports module aliases to give a module a different name when importing it:
```
use module as mod;
use module from "module_dir" as mod;
```
As can be noted, any alias must follow the `from` clause (if one is present).\
This works even if the module has been previously imported by the original name (both names simply resolve to the same module object internally).\
It should be noted that the module object referred to by the alias still holds its original name:
```
use time as t;
println!(t); // Prints: <module time>
```

## Importing Specific Entries from Modules

It is possible that a user may not want to import all of the objects within a particular module, but rather only particular entries. Choice supports this by allowing a user to specify particular module entries to import.\
The syntax to do so involves appending the scope operator `::` to the module name, followed by a brace-enclosed list of entry names:
```
use module1::{entry1};
use module2::{entry1, entry2};
```

When module entries are specified in this form, they can be used directly without scoping from the module:
```
use module::{entry};
entry(); // Not 'module::entry()'.
```
Attempting to use the module in this example would lead to a compilation error. This is because importing specific entries from a module does not introduce the module itself as a variable that the compiler can recognize. Importing a module and specific entries (to avoid scoping) must be done separately.

Similar to modules, module entries that are specified also support aliases. To give an entry an alias, use the following syntax:
```
use module::{entry1 as alias1, entry2 as alias2};
```

Additional notes:
1. Choice does not currently support importing all entries within a module without scoping, unlike other languages.
2. Any `from` clause used when specifying module entries should come *after* the entry list.
2. It is not valid to add an `as` alias clause for the module if entries are specified. This is because, as mentioned, the module itself is not introduced as a variable or object, let alone one with an alias.

## Notes on Module Entry Values

First, since module objects are constructed by first evaluating their respective source files, the values for module entries depend on the final value they have once said execution concluded:
```
// mod.ch

make x = 1;
x = 2;

// main.ch

use mod;
println!(mod::x); // Prints 2, not 1.
```

Second, while globals are not internally captured within closure objects by default, modules do perform such captures to ensure consistent behavior if objects or functions that rely on certain global variables are specified in imports.\
As an example, the below code works as expected without having to import `global_var`:
```
// mod.ch

make global_var = "Hello from module!":
func print_var()
{
    println!(global_var);
}

// main.ch

use mod::{print_var};
print_var(); // Prints: Hello from module!
```