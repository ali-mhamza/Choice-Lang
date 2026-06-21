make a = 1;
println!(a);            // Expect: 1
println!(typeof!(a));   // Expect: Int

make b = -1;
println!(b);            // Expect: -1
println!(typeof!(b));   // Expect: Int

make c = 9223372036854775807;
println!(c);            // Expect: 9223372036854775807
println!(typeof!(c));   // Expect: Int
// Overflow. Not properly implemented.
println!(c + 1);        // Expect: -9223372036854775808