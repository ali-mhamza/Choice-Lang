println!(type!(1));         // Expect: Int
println!(type!(1.1));       // Expect: Dec
println!(type!(true));      // Expect: Boolean
println!(type!(null));      // Expect: Null
println!(type!("Word"));    // Expect: String
// Empty string.
println!(type!(""));        // Expect: String
println!(type!(1..10));     // Expect: Range
println!(type!([1, 2, 3])); // Expect: List
// Empty list.
println!(type!([]));        // Expect: List

println!(type!(type!(1)));  // Expect: Type
println!(type!(println));   // Expect: Builtin

func A() {}
println!(type!(A)); // Expect: Function

func B()
{
    make x = 1;
    func C() { x = 2; }

    println!(type!(C));
}

B(); // Expect: Function

make D = || {};
println!(type!(D));     // Expect: Lambda
println!(type!(D()));   // Expect: Void