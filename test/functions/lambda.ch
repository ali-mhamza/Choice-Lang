make a = || {};

a(); // Expect:

make b = |x| {
    println!(x);
};

b(1); // Expect: 1

make c = |x, y| {
    println!(x + y);
};

c(1, 2); // Expect: 3

make d = || {
    println!(d);
};

d(); // Expect: <lambda>