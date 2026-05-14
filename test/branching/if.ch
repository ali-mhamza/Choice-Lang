make a = 1;

if (a == 1)
    println!(1);    // Expect: 1

if (a == 2)
    println!(2);
else
    println!(1);    // Expect: 1

if (a == 2)
{
    println!(2);
}
elif (a == 1)
{
    println!(1);    // Expect: 1
}
else
{
    println!(0);
}

make b = if (a == 1) {1} else {2};
println!(b);        // Expect: 1

make c = if (a == 2) {1} else {2};
println!(c);        // Expect: 2

make d = if (a == 2) {a} elif (b == 1) {b} else {c};
println!(d);        // Expect: 1