//================== Zero boundaries ======================//

for (i in range!(0, 0))
    print!(i);                      // Expect: 0

println!();

println!(0 in range!(0, 0));        // Expect: true
println!(1 in range!(0, 0));        // Expect: false
println!(-1 in range!(0, 0));       // Expect: false

println!(len!(range!(0, 0)));       // Expect: 1


//================== Negative only ======================//

// No step specified.

for (i in range!(-5, -1))
    print!(i);                      // Expect: -5-4-3-2-1

println!();

for (i in range!(-1, -5))
    print!(i);                      // Expect: -1-2-3-4-5

println!();

println!(-5 in range!(-5, -1));     // Expect: true
println!(-1 in range!(-5, -1));     // Expect: true
println!(-3 in range!(-5, -1));     // Expect: true
println!(0 in range!(-5, -1));      // Expect: false

println!(len!(range!(-5, -1)));     // Expect: 5
println!(len!(range!(-1, -5)));     // Expect: 5


// Step = 2.

for (i in range!(-6, -1, 2))
    print!(i);                      // Expect: -6-4-2

println!();

for (i in range!(-1, -6, 2))
    print!(i);                      // Expect: -1-3-5

println!();

println!(-6 in range!(-6, -1, 2));  // Expect: true
println!(-5 in range!(-6, -1, 2));  // Expect: false
println!(-1 in range!(-6, -1, 2));  // Expect: false

println!(len!(range!(-6, -1, 2)));  // Expect: 3
println!(len!(range!(-1, -6, 2)));  // Expect: 3


//================== Crossing zero ======================//

// No step specified.

for (i in range!(-3, 3))
    print!(i);                      // Expect: -3-2-10123

println!();

for (i in range!(3, -3))
    print!(i);                      // Expect: 3210-1-2-3

println!();

println!(0 in range!(-3, 3));       // Expect: true
println!(3 in range!(-3, 3));       // Expect: true
println!(-3 in range!(-3, 3));      // Expect: true
println!(4 in range!(-3, 3));       // Expect: false

println!(len!(range!(-3, 3)));      // Expect: 7
println!(len!(range!(3, -3)));      // Expect: 7


// Step = 2.

for (i in range!(-4, 4, 2))
    print!(i);                      // Expect: -4-2024

println!();

for (i in range!(4, -4, 2))
    print!(i);                      // Expect: 420-2-4

println!();

println!(0 in range!(-4, 4, 2));    // Expect: true
println!(2 in range!(-4, 4, 2));    // Expect: true
println!(3 in range!(-4, 4, 2));    // Expect: false

println!(len!(range!(-4, 4, 2)));   // Expect: 5
println!(len!(range!(4, -4, 2)));   // Expect: 5


//================== Mixed edge cases ======================//

// Step larger than range.

for (i in range!(1, 3, 5))
    print!(i);                      // Expect: 1

println!();

for (i in range!(3, 1, 5))
    print!(i);                      // Expect: 3

println!();

println!(1 in range!(1, 3, 5));     // Expect: true
println!(3 in range!(1, 3, 5));     // Expect: false

println!(len!(range!(1, 3, 5)));    // Expect: 1
println!(len!(range!(3, 1, 5)));    // Expect: 1


// Step = exact jump to end.

for (i in range!(2, 10, 8))
    print!(i);                      // Expect: 210

println!();

println!(10 in range!(2, 10, 8));   // Expect: true
println!(6 in range!(2, 10, 8));    // Expect: false

println!(len!(range!(2, 10, 8)));   // Expect: 2