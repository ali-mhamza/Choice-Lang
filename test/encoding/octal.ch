println!("\o0");    // Expect: [bytes] \x00
println!("\o7");    // Expect: [bytes] \x07
println!("\o10");   // Expect: [bytes] \x08
println!("\o52");   // Expect: [bytes] \x2A
println("\o377");   // Expect: [bytes] \xFF
println("\o1234");  // Expect: [bytes] \x534

// Torture.

println!("\o1");                // Expect: 
println!("\o41");               // Expect: !
println!("\o101");              // Expect: A
println!("\o000");              // Expect: [bytes] \x00
println!("X\o101Y");            // Expect: XAY
println!("\o1011");             // Expect: A1
println!("A\o41\tB");           // Expect: [bytes] A!\tB
println!("Mix:\o101\n\o102");   // Expect: Mix:A
                                // Expect: B