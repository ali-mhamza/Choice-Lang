println!(-1);               // Expect: -1
println!(-(-1));            // Expect: 1
println!(type!(-1));        // Expect: Int
println!(type!(-(-1)));     // Expect: Int

println!(-1.5);             // Expect: -1.5
println!(-(-1.5));          // Expect: 1.5
println!(type!(-1.5));      // Expect: Dec
println!(type!(-(-1.5)));   // Expect: Dec