#==========#

for (i in 0..9)
    print!(i);  // Expect: 0123456789

println!();

#==========#

for (i in 0..9 where i % 2 == 1)
    print!(i);  // Expect: 13579

println!();

#==========#

make str = "Hello, world!";
for (c in str)
{
    str = "Not hello world!";
    print!(c);
} // Expect: Hello, world!

println!();
println!(str); // Expect: Not hello world!

#==========#

for (i in 0..9)
{
    if (i == 5) break;
        print!(i);
} // Expect: 01234
else
{
    println!();
    println!("No break occurred!");
}

println!();

for (i in 0..9)
    print!(i);  // Expect: 0123456789
else
{
    println!();
    println!("No break occurred!"); // Expect: No break occurred!
}