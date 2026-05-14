make x = "Hello, world!";
println!(x);            // Expect: Hello, world!

for (i in x)
    print!(i);          // Expect: Hello, world!

println!();

println!("H" in x);     // Expect: true
println!("!" in x);     // Expect: true
println!("f" not in x); // Expect: true
println!("f" in x);     // Expect: false

println!(len(x));       // Expect: 13