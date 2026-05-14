func A() {}

A(); // Expect:

func B(x)
{
    println!(x);
}

B(1); // Expect: 1

func C(x, y)
{
    println!(x + y);
}

C(1, 2); // Expect: 3

func D()
{
    println!(D);
}

D(); // Expect: <func D>