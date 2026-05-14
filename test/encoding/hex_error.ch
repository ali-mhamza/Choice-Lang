println!("\x");                 // Error
println!("test\x");             // Error
println!("before \x after");    // Error

println!("\xg");                // Error
println!("\xG");                // Error
println!("\xz");                // Error
println!("\xZ");                // Error
println!("\x_");                // Error
println!("\x-");                // Error
println!("\x ");                // Error
println!("\x\t");               // Error

println!("X\xg");               // Error
println!("X\xGY");              // Error
println!("X\x_Y");              // Error