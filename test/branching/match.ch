make x = 1;

// Basic.

match (x)
{
    is 1:
        println!(1);                // Expect: 1
    is 2:
        println!(2);
}

// No matches.

match(x)
{
    is 2:
        println!(2);
    is 3:
        println!(3);                // Expect:
}

// Default case.

match (x)
{
    is 2:
        println!(2);
    is ?:
        println!("Default");        // Expect: Default
}

// Default fallthrough for empty cases.

match (x)
{
    is 1:
    is 2:
    is 3:
    is 4:
    {
        println!(4);                // Expect: 4
        println!("Fallthrough");    // Expect: Fallthrough
    }
}

// Opt-in fallthrough for non-empty cases.

match(x)
{
    is 1:
    {
        println!(1);                // Expect: 1
        fallthrough;
    }
    is 2:
        println!(2);                // Expect: 2
}

// Using 'end' to stop fallthrough (both kinds).

match (x)
{
    is 1:
    is 2:
    is 3:
        end;
    is 4:
    is 5:
    {
        println!(5);
        println!("Fallthrough");     // Expect:
    }
}

match(x)
{
    is 1:
    {
        println!(1);                // Expect: 1
        fallthrough;
    }
    is 2:
        println!(2);                // Expect: 2
    is 3:
        println!(3);                // Expect: 3
    is 4:
    {
        println!(4);                // Expect: 4
        end;
    }
    is 5:
        println!(5);                // Expect:
}

// Nested use of 'fallthrough' (doesn't "leak out").

make y = 2;

match (x)
{
    is 1:
    {
        match (y)
        {
            is 2:
            {
                println!(2);        // Expect: 2
                fallthrough;
            }
        }
    }
    is 2:
        println!(2);                // Expect:
}

// Nested use of 'fallthrough' (doesn't "leak in").

match (x)
{
    is 1:
    {
        println!(1);                // Expect: 1
        fallthrough;
    }
    is 2:
    {
        println!(2);                // Expect: 2
        match (y)
        {
            is 2:
                println!(2);        // Expect: 2
            is 3:
                println!(3);        // Expect:
        }
    }
}

// Nested use of 'end' (doesn't "leak out").

match (x)
{
    is 1:
    {
        println!(1);                // Expect: 1
        fallthrough;
    }
    is 2:
    {
        println!(2);                // Expect: 2
        match (y)
        {
            is 2:
            {
                println!(2);        // Expect: 2
                end;
            }
        }
    }
    is 3:
        println!(3);                // Expect: 3
}

// Nested use of 'end' (doesn't "leak in").
// Unfinished.