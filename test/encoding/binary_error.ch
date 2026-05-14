println!("\b");                 // Error
println!("test\b");             // Error
println!("before \b after");    // Error

println!("\b2");                // Error
println!("\b9");                // Error
println!("\ba");                // Error
println!("\bA");                // Error
println!("\b_");                // Error
println!("\b-");                // Error
println!("\b ");                // Error
println!("\b\t");               // Error

println!("X\b2");               // Error
println!("X\b9Y");              // Error
println!("X\baY");              // Error
println!("X\b_Y");              // Error