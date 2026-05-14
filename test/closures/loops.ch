make x = [for (i in 0..9): || {
    print!(i);
}];

for (fn in x)
    fn(); // Expect: 0123456789
println!();