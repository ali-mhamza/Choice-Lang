make x = "Hello";

println!(x);                        // Expect: Hello
println!(typeof!(x));               // Expect: String

println!(x + ", world!");           // Expect: Hello, world!
println!(typeof!(x + ", world!"));  // Expect: String

make y = ", world!";

println!("Hello" + y);              // Expect: Hello, world!
println!(typeof!("Hello" + y));     // Expect: String

make name = "John";
make distance = 10;

println!("Hello, %(name) is %(distance) mins away.");       // Expect: Hello, John is 10 mins away.
println!("I am %(distance + 1) mins away.");                // Expect: I am 11 mins away.
println!("Contains %("a quote") and closes.");              // Expect: Contains a quote and closes.
println!("First level, %("second %("level")") and end.");   // Expect: First level, second level and end.
println!("Hello, my name is \%(name).");                    // Expect: Hello, my name is %(name).
println!(r"Hello, my name is %(name).");                    // Expect: Hello, my name is %(name).