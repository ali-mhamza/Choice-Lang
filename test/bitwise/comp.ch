println!(~0);                       // Expect: -1
println!(~1);                       // Expect: -2
println!(~5);                       // Expect: -6
println!(~12345);                   // Expect: -12346
println!(~-1);                      // Expect: 0
println!(~-2);                      // Expect: 1
println!(~9223372036854775807);     // Expect: -9223372036854775808
// println!(~-9223372036854775808);    Expect: 9223372036854775807
println!(~(5 & 3));                 // Expect: -2
println!(~(5 | 3));                 // Expect: -8
println!(~(5 ^ 3));                 // Expect: -7
println!(~((5 << 2)));              // Expect: -21
println!(~((5 >> 1)));              // Expect: -3