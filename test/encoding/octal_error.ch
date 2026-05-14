println!("\o");                 // Error
println!("test\o");             // Error
println!("before \o after");    // Error

println!("\o8");                // Error
println!("\o9");                // Error
println!("\oa");                // Error
println!("\oA");                // Error
println!("\o_");                // Error
println!("\o-");                // Error
println!("\o ");                // Error
println!("\o\t");               // Error

println!("X\o8");               // Error
println!("X\o9Y");              // Error
println!("X\oaY");              // Error
println!("X\o_Y");              // Error

println!("\o400");              // Error
println!("\o777");              // Error
println!("\o999");              // Error