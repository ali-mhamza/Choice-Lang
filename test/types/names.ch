println!(typeof!(1));           // Expect: Int
println!(typeof!(1.1));         // Expect: Dec
println!(typeof!(true));        // Expect: Bool
println!(typeof!(null));        // Expect: Null
println!(typeof!("Word"));      // Expect: String
// Empty string.
println!(typeof!(""));          // Expect: String
println!(typeof!(1..10));       // Expect: Range
println!(typeof!([1, 2, 3]));   // Expect: List
// Empty list.
println!(typeof!([]));          // Expect: List

println!(typeof!(typeof!(1)));  // Expect: Builtin Type
println!(typeof!(println));     // Expect: Builtin Function

func A() {}
println!(typeof!(A)); // Expect: User Function

func B()
{
    make x = 1;
    func C() { x = 2; }

    println!(typeof!(C));
}

B(); // Expect: User Function

make D = || {};
println!(typeof!(D));     // Expect: Lambda
println!(typeof!(D()));   // Expect: Void