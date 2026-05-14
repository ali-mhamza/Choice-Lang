make a = || {};

a(1); // Error

make b = |x, y| {       \
    println!(x + y);    \
};

b(1); // Error

make c = |x, x| {} // Error

make D = |fix x, fix y| {   \
    x = 1;                  \
    y = 1;                  \
}; // Error (2)