make x = {};
println!(type!(x));         // Expect: Table

x[1] = 1;
x[2] = 2;
x["hello"] = 3;
println!(x);                // Expect: {(1, 1), (2, 2), ('hello', 3)}

println!(x[1]);             // Expect: 1
println!(x[2]);             // Expect: 2
println!(x["hello"]);       // Expect: 3
println!(x[3]);             // Expect: ()

println!(1 in x);           // Expect: true
println!(2 in x);           // Expect: true
println!("hello" in x);     // Expect: true

println!(0 in x);           // Expect: false
println!(false not in x);   // Expect: true

println!(len!(x));          // Expect: 3