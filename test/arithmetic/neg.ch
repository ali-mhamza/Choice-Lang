println!(-1);               // Expect: -1
println!(-(-1));            // Expect: 1
println!(typeof!(-1));      // Expect: Int
println!(typeof!(-(-1)));   // Expect: Int

println!(-1.5);             // Expect: -1.5
println!(-(-1.5));          // Expect: 1.5
println!(typeof!(-1.5));    // Expect: Dec
println!(typeof!(-(-1.5))); // Expect: Dec