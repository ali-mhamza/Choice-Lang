// Mixed bitwise operators.
println!(5 & 3 | 1);    // Expect: 1
println!(5 | 3 & 1);    // Expect: 5
println!(5 ^ 3 & 1);    // Expect: 4
println!(5 & 3 ^ 1);    // Expect: 0

// With parentheses.
println!((5 & 3) | 1);  // Expect: 1
println!(5 & (3 | 1));  // Expect: 1
println!((5 ^ 3) & 1);  // Expect: 0

// Precedence with arithmetic.
println!(5 + 3 << 2);   // Expect: 32
println!(5 << 3 + 2);   // Expect: 160
println!(5 & 3 + 1);    // Expect: 4
println!(5 + 3 & 1);    // Expect: 0

// Precedence with comparison.
println!(5 & 3 == 1);   // Expect: true
println!(5 == 3 & 1);   // Expect: false

// Operator associativity.
println!(5 & 3 & 1);    // Expect: 1
println!(5 | 3 | 1);    // Expect: 7
println!(5 ^ 3 ^ 1);    // Expect: 7
println!(5 << 1 << 1);  // Expect: 20
println!(20 >> 2 >> 1); // Expect: 2