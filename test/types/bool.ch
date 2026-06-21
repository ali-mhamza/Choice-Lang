println!(true);             // Expect: true
println!(false);            // Expect: false

println!(typeof!(true));    // Expect: Bool
println!(typeof!(false));   // Expect: Bool

println!(1 == 0);           // Expect: false
println!(1 == 1);           // Expect: true

println!(typeof!(1 == 0));  // Expect: Bool
println!(typeof!(1 == 1));  // Expect: Bool

println!(!true);            // Expect: false
println!(!false);           // Expect: true
println!(!!true);           // Expect: true