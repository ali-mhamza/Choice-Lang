make a = [1];
a[0] = a;
println!(a);        // Expect: [[...]]

make b = {};
b[b] = b;
println!(b);        // Expect: {({...}, {...})}

make c = [1];
make d = {(c, c)};
c[0] = d;
println!(c);        // Expect: [{([...], [...])}]

make e = {};
make f = [e];
e[f] = f;
println!(e);        // Expect: {([{...}], [{...}])}