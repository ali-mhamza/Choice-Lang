// Range values are indexable for reading, but their elements are not assignable.

range!(0, 9)[0] = 42;       // Error
range!(9, 0)[4] = 42;       // Error
range!(-3, 3)[3] = 42;      // Error
range!(0, 10, 2)[2] = 42;   // Error