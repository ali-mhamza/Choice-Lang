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

println!((0..9)[0]);        // Expect: 0
println!((0..9)[4]);        // Expect: 4
println!((0..9)[9]);        // Expect: 9
println!((9..0)[0]);        // Expect: 9
println!((9..0)[4]);        // Expect: 5
println!((9..0)[9]);        // Expect: 0

println!((-3..3)[0]);       // Expect: -3
println!((-3..3)[3]);       // Expect: 0
println!((-3..3)[6]);       // Expect: 3
println!((3..-3)[0]);       // Expect: 3
println!((3..-3)[3]);       // Expect: 0
println!((3..-3)[6]);       // Expect: -3