// Variadic parameter followed by regular parameter.

func bad(values..., x)  \
{                       \
    println!(x);        \
} // Error (3)

// Variadic parameter followed by default parameter.

func bad(values..., x = 1)  \
{                           \
    println!(x);            \
} // Error (3)

// Variadic parameter followed by another variadic parameter.

func bad(a..., b...)    \
{                       \
    println!(a);        \
} // Error (3)