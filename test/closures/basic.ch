func A1()
{
    make x = 1;
    func B1()
    {
        println!(x);
    }

    return B1;
}

make x = A1();
x(); // Expect: 1

func A2(x)
{
    func B2()
    {
        println!(x);
    }

    return B2;
}

make y = A2("Hello, world!");
y(); // Expect: Hello, world!