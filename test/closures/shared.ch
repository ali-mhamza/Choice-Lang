make globalGet;
make globalSet;

func main()
{
    make a = "initial";

    func get() { println!(a); }
    func set() { a = "updated"; }

    globalGet = get;
    globalSet = set;
}

main();
globalSet();
globalGet(); // Expect: updated