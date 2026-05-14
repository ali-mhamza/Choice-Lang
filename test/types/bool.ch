println!(true);             // Expect: true
println!(false);            // Expect: false

println!(type!(true));      // Expect: Boolean
println!(type!(false));     // Expect: Boolean

println!(1 == 0);           // Expect: false
println!(1 == 1);           // Expect: true

println!(type!(1 == 0));    // Expect: Boolean
println!(type!(1 == 1));    // Expect: Boolean

println!(!true);            // Expect: false
println!(!false);           // Expect: true
println!(!!true);           // Expect: true