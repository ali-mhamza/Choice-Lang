println!(len!([1, 2, 3]));          // Expect: 3
println!(len!([]));                 // Expect: 0
println!(len!("Hello, world!"));    // Expect: 13
println!(len!(""));                 // Expect: 0
println!(len!(1..10));              // Expect: 10
println!(len!(10..1));              // Expect: 10
println!(len!(1..1));               // Expect: 1