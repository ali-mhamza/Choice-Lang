func A() {}

A(1); // Error

func B(x, y)            \
{                       \
    println!(x + y);    \
}

B(1); // Error

func C(x, x) {} // Error

func D(fix x, fix y)    \
{                       \
    x = 1;              \
    y = 1;              \
} // Error (2)

func First()            \
{                       \
    Second();           \
}                       \
                        \
func Second()           \
{                       \
                        \
} // Error