println!(5 & 3);                    // Expect: 1
println!(12 & 10);                  // Expect: 8
println!(0 & 12345);                // Expect: 0
println!(-1 & 12345);               // Expect: 12345
println!(9223372036854775807 & 1);  // Expect: 1
// Value is too large -> overflow.
// println!(-9223372036854775808 & 1); Expect: 0
println!(5 & -3);                   // Expect: 5