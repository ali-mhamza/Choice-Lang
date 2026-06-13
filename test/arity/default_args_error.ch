func f(a = fake)    \
{                   \
    println!(a);    \
} // Error (1)

func f(a = 1, b)        \
{                       \
    println!(a + b);    \
} // Error (4)