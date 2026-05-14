println!("\u");                 // Error
println!("\u1234");             // Error
println!("test\u");             // Error
println!("before \u after");    // Error

println!("\u{}");               // Error
println!("test\u{}");           // Error

println!("\u{");                // Error
println!("test\u{");            // Error

println!("\u{1234567}");        // Error
println!("test\u{1234567}");    // Error

println!("\u{110000}");         // Error
println!("test\u{110000}");     // Error

println!("\u41}");              // Error
println!("\u{41");              // Error
println!("\u{XYZ}");            // Error
println!("\u{12G}");            // Error
println!("\u{-1}");             // Error
println!("\u{ 41}");            // Error
println!("\u{41 }");            // Error
println!("\u{1F60G}");          // Error
println!("\u{0000001}");        // Error
println!("\u{D800}");           // Error
println!("\u{DFFF}");           // Error