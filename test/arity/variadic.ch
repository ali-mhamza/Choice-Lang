func show(values...)
{
    println!(len!(values), values);
}

show();             // Expect: 0 []
show(1);            // Expect: 1 [1]
show(1, 2, 3);      // Expect: 3 [1, 2, 3]

func firstAndRest(first, rest...)
{
    print!(first);
    print!(",", len!(rest));
    println!(",", rest);
}

firstAndRest("a");              // Expect: a, 0, []
firstAndRest("a", "b", "c");    // Expect: a, 2, ['b', 'c']

func mutateRest(pos, rest...)
{
    rest[pos] *= 10;
    println!(rest);
}

mutateRest(0, 1, 2, 3); // Expect: [10, 2, 3]
mutateRest(1, 1, 2, 3); // Expect: [1, 20, 3]
mutateRest(2, 1, 2, 3); // Expect: [1, 2, 30]