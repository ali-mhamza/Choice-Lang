make i = 2;
repeat {
    i--;
    if (i == 0) continue;
    print!(i);
} until (i == 0);
println!(); // Expect: 1