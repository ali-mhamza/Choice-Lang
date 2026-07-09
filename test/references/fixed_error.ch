func change(a)  \
{               \
    a = 1;      \
}

fix x = 1;
change(*x);     // Error

fix y = [1, 2, 3];
change(*y[0]);  // Error

type Z { fix x = 1, y = 2 }
make z1 = Z();
change(*z1.x);  // Error

fix z2 = Z();
change(*z2.y);  // Error