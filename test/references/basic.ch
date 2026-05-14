func A(x, y)
{
    x = 1;
    y = 1;
}

make x;
make y;

A(x, *y);

println!(x);    // Expect: null
println!(y);    // Expect: 1