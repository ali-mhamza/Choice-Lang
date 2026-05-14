make x = 1;

func A()
{
    func B()
    {
        func C(y)
        {
            y = 10;
        }

        C(*x);
    }

    B();
    println!(x);
}

A(); // Expect: 10