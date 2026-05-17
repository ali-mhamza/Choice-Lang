make a: Int = 1;
make b: Int? = 2;
make c: List[Int] = [1, 2];
make d: List[List[Int]] = [[1, 2], [3, 4]];
make e: <Int | Dec> = 4;

make f = |x: Int| -> String {};
make g = |x: Int| => x ** 2;

func f1(x: *Int) {}
func f2(x: Int) -> String {}
func f3(x: Int) -> <(Int, String) | String> {}
func f4(x: Int) -> (<Int | String>, String) {}

make h: Func(Int, Boolean) -> String = null;
make i = |x: <Int | String>| {};

for (i: <Int | String> in [1, 2, 3]) {}