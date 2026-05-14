println!("\b0");            // Expect: [bytes] \x00
println!("\b1");            // Expect: [bytes] \x01
println!("\b10");           // Expect: [bytes] \x02

println!("\b101010");       // Expect: [bytes] \x2A
println!("\b11111111");     // Expect: [bytes] \xFF
println!("\b101010101");    // Expect: [bytes] \xAA1

// Torture.

println!("\b1");                        // Expect: 
println!("\b1000001");                  // Expect: A
println!("\b01000001");                 // Expect: A
println!("X\b100001Y");                 // Expect: X!Y
println!("\b010000011");                // Expect: A1
println!("A\b100001\tB");               // Expect: [bytes] A!\tB
println!("Mix:\b1000001\n\b1000010");   // Expect: Mix:A
                                        // Expect: B