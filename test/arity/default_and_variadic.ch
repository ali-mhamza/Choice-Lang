make count = 0;

func mark()
{
    return ++count;
}

// Default parameters may be followed by a variadic parameter.
func collect(prefix = mark(), values...)
{
    print!(prefix);
    print!(",", len!(values));
    println!(",", values);
}

collect();              // Expect: 1, 0, []
collect(10);            // Expect: 10, 0, []
collect(10, 20, 30);    // Expect: 10, 2, [20, 30]
println!(count);        // Expect: 1

// Multiple defaults may appear before a variadic parameter.
func collect2(a = mark(), b = mark(), values...)
{
    print!(a);
    print!(",", b);
    print!(",", len!(values));
    println!(",", values);
}

collect2();             // Expect: 2, 3, 0, []
collect2(40);           // Expect: 40, 4, 0, []
collect2(40, 50, 60);   // Expect: 40, 50, 1, [60]
println(count);         // Expect: 4