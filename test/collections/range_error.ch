// Range values are indexable for reading, but their elements are not assignable.

(0..9)[0] = 42;             // Error
(9..0)[4] = 42;             // Error
(-3..3)[3] = 42;            // Error
(3..-3)[3] = 42;            // Error