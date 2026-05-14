println!("\x0");    // Expect: [bytes] \x00
// Test will split output awkwardly with bare
// new-lines, so we add a character before.
print!("A\xA");     // Expect: A
println!("\x2A");   // Expect: [bytes] \x2A
println!("\xFF");   // Expect: [bytes] \xFF
println!("\xABC");  // Expect: [bytes] \xABC
println!("\x1z");   // Expect: [bytes] \x01z

// Torture.

println!("\x1");            // Expect: 
println!("\x21");           // Expect: !
println!("\x41");           // Expect: A
println!("\x7a");           // Expect: z
println!("\x00");           // Expect: [bytes] \x00
println!("X\x41Y");         // Expect: XAY
println!("\x414");          // Expect: A4
println!("A\x21\tB");       // Expect: A!	B
println!("Mix:\x41\n\x42"); // Expect: Mix:A
                            // Expect: B