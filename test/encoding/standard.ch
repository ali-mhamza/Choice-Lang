println!("Hello\nworld!");	// Expect: Hello
                            // Expect: world!
println!("A\tB");           // Expect: [bytes] A\tB
// Test expects both words to be printed separately.
println!("Hello\rWorld!");  // Expect: Hello
                            // Expect: World!
println!("A\\B");           // Expect: A\B
println!("A\"B");           // Expect: A"B

// Torture.

print!("A\n");                  // Expect: A

println!("A\nB");               // Expect: A
                                // Expect: B

println!("Start\n\nEnd");       // Expect: Start
                                // Expect: [bytes] \n
                                // Expect: End

println!("\t");                 // Expect: [bytes] \t
println!("A\tB");               // Expect: [bytes] A\tB
println!("\tIndented");         // Expect: [bytes] \tIndented
println!("A\t\nB");             // Expect: [bytes] A\t	
                                // Expect: B

println!("A\rB");               // Expect: A
                                // Expect: B
println!("Hello\rX");           // Expect: Hello
                                // Expect: X
println!("12345\rAB");          // Expect: 12345
                                // Expect: AB

println!("\\");                 // Expect: \
println!("A\\B\\C");            // Expect: A\B\C
println!("\\\\");               // Expect: \\

println!("\"");                 // Expect: "
println!("He said: \"hi\".");   // Expect: He said: "hi".
println!("\\\"");               // Expect: \"