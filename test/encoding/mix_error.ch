println!("mix \b end");                 // Error
println!("mix \o end");                 // Error
println!("mix \x end");                 // Error

println!("mix \o400 still bad");        // Error
println!("prefix \o777 suffix");        // Error

println!("mix \u{} ok");                // Error
println!("mix \u{1234567} ok");         // Error
println!("mix \u{110000} ok");          // Error
println!("mix \u no braces");           // Error

println!("A\x41B\o400C");               // Error
println!("A\b1000001B\u{}C");           // Error
println!("A\u{41}B\u{110000}C");        // Error
println!("A\o101B\xC\u{1234567}");      // Error

println!("\b2\o101");                   // Error
println!("\o8\x41");                    // Error
println!("\xg\u{41}");                  // Error
println!("\u{XYZ}\n");                  // Error

println!("ok:\b1 bad:\b2");             // Error
println!("ok:\o101 bad:\o400");         // Error
println!("ok:\x41 bad:\xg");            // Error
println!("ok:\u{41} bad:\u{110000}");   // Error

println!("chain:\b1\o8");               // Error
println!("chain:\o101\xg");             // Error
println!("chain:\x41\u{XYZ}");          // Error
println!("chain:\u{41}\b9");            // Error