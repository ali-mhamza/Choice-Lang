func outer()
{
    make x = "Value";
    func middle()
    {
        func inner() { println!(x); }

        println!("Create inner closure.");
        return inner;
    }

    println!("Return from outer.");
    return middle;
}

make mid = outer(); // Expect: Return from outer.
make inner = mid(); // Expect: Create inner closure.
inner();            // Expect: Value