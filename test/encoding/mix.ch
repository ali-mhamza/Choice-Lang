print!("X\rY");             // Expect: X
println!();                 // Expect: Y
println!("X\\Y");           // Expect: X\Y
println!("X\"Y");           // Expect: X"Y
println!("A\b1000010Z");    // Expect: ABZ
println!("A\o101Z");        // Expect: AAZ
println!("A\x41Z");         // Expect: AAZ
println!("\b1\o7\xF");      // Expect: [bytes] \x01\x07\x0F

// Torture.

println!("A\\nB");      // Expect: A\nB
println!("A\\tB");      // Expect: A\tB
println!("A\\rB");      // Expect: A\rB
println!("A\\x41B");    // Expect: A\x41B
println!("A\\u{41}B");  // Expect: A\u{41}B

println!("combo:\x41\o102\b1000011\u{44}");         // Expect: combo:ABCD
println!("combo:\u{41}\n\x42\t\o103\\\b1000100\""); // Expect: combo:A
                                                    // Expect: [bytes] B\tC\\D\"

println!("\b1\o41\x42\u{43}");                      // Expect: !BC
println!("Z\b1000001\o102\x43\u{44}");              // Expect: ZABCD