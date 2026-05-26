make x = [for (i in 0..9): i];
println!(x);            // Expect: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

for (i in x)
    print!(i);          // Expect: 0123456789

println!();

for (i in 0..9)
    print!(x[i]);       // Expect: 0123456789

println!();

println!(0 in x);       // Expect: true
println!(9 in x);       // Expect: true
println!(10 not in x);  // Expect: true
println!(10 in x);      // Expect: false

println!(len!(x));      // Expect: 10