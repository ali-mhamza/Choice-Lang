make hits = 0;

func bump()
{
    return ++hits;
}

func pair(a = bump(), b = bump())
{
    print!(a);
    println!(",", b);
}

// Defaults are not evaluated when the function is not called.
println!(hits);      // Expect: 0

// Defaults are not evaluated when explicit arguments are supplied.
pair(10, 20);       // Expect: 10, 20
println!(hits);     // Expect: 0

// Defaults are evaluated only when needed.
pair(30);           // Expect: 30, 1
println!(hits);     // Expect: 1

// Defaults are evaluated anew each call.
pair();             // Expect: 2, 3
pair();             // Expect: 4, 5
println!(hits);     // Expect: 5

func newMap(a, b, map = {})
{
    map[a] = b;
    println!(map);
}

newMap(1, 2);       // Expect: {(1, 2)}
newMap(3, 4);       // Expect: {(3, 4)}

// Assignment/increment-like expressions are valid default initializers.
func nextVal(x = (hits += 10))
{
    println!(x);
}

nextVal();          // Expect: 15
println!(hits);     // Expect: 15
nextVal(99);        // Expect: 99
println!(hits);     // Expect: 15