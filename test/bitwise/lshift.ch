println!(1 << 1);   // Expect: 2
println!(1 << 5);   // Expect: 32
println!(5 << 2);   // Expect: 20
println!(0 << 10);  // Expect: 0
println!(5 << 0);   // Expect: 5
println!(-5 << 1);  // Expect: -10
println!(1 << 63);  // Expect: -9223372036854775808