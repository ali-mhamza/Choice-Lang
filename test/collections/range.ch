for (i in 0..9)
    print!(i);              // Expect: 0123456789

println!();

for (i in 9..0)
    print!(i);              // Expect: 9876543210

println!();

println!(0 in 0..9);        // Expect: true
println!(9 in 0..9);        // Expect: true
println!(0 in 9..0);        // Expect: true
println!(9 in 9..0);        // Expect: true
println!(10 in 0..9);       // Expect: false
println!(10 in 9..0);       // Expect: false

println!(len!(0..9));       // Expect: 10
println!(len!(9..0));       // Expect: 10