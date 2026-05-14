println!("\u{41}");     // Expect: A
println!("\u{7A}");     // Expect: z
println!("\u{30}");     // Expect: 0
println!("A\u{20}B");   // Expect: A B

println!("\u{A9}");     // Expect: ©
println!("\u{AE}");     // Expect: ®
println!("\u{B0}");     // Expect: °

println!("\u{3A9}");    // Expect: Ω
println!("\u{3C0}");    // Expect: π

println!("\u{20AC}");   // Expect: €
println!("\u{2192}");   // Expect: →
println!("\u{2665}");   // Expect: ♥
println!("\u{4E2D}");   // Expect: 中
println!("\u{6587}");   // Expect: 文

println!("\u{1F600}");  // Expect: 😀
println!("\u{1F602}");  // Expect: 😂
println!("\u{1F680}");  // Expect: 🚀
println!("\u{10000}");  // Expect: 𐀀

// Torture.

println!("\u{41}");     // Expect: A
println!("\u{7a}");     // Expect: z
println!("\u{A9}");     // Expect: ©
println!("\u{3A9}");    // Expect: Ω
println!("\u{20AC}");   // Expect: €
println!("\u{4E2D}");   // Expect: 中
println!("\u{1F600}");  // Expect: 😀
println!("\u{10FFFF}"); // Expect: 􏿿

println!("Greek: \u{3A9}\u{3C0}");                          // Expect: Greek: Ωπ
println!("Arabic: \u{645}\u{631}\u{62D}\u{628}\u{627}");    // Expect: Arabic: مرحبا
println!("Emoji: \u{1F600}\u{1F680}");                      // Expect: Emoji: 😀🚀
println!("Mix: A\n\t\\\"\u{41}\x42\o103\b1000100");         // Expect: Mix: A
                                                            // Expect: [bytes] \t\\\"ABCD