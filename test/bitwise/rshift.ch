println!(8 >> 1);   // Expect: 4
println!(5 >> 1);   // Expect: 2
println!(0 >> 5);   // Expect: 0
println!(5 >> 0);   // Expect: 5
println!(-1 >> 1);  // Expect: -1
println!(-5 >> 1);  // Expect: -3
println!(1 >> 63);  // Expect: 0