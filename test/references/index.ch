func change(a...)
{
    for (i in a)
        i = 3;
}

make x = [1, 2, 3];
change(*x[0], *x[1]);
println!(x); // Expect: [3, 3, 3]

make y = [[1, 2, 3]];
change(*y[0][0], *y[0][1]);
println!(y); // Expect: [[3, 3, 3]]