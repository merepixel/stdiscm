# Problem Set 1 - Threaded Prime Number Search

### Compilation
Each variant is a separate program. Use the following commands to compile with C++20:

```bash
clang++ -std=c++20 v1.cpp -o v1
clang++ -std=c++20 v2.cpp -o v2
clang++ -std=c++20 v3.cpp -o v3
clang++ -std=c++20 v4.cpp -o v4
```

### Running
After compilation, execute each variant with:
```bash
./v1
./v2
./v3
./v4
```

All variants read their configuration from config.txt.

### Configuration (config.txt)
The configuration file must contain the following parameters:
- threads (integer, ≥ 1): Number of worker threads to use.
- limit (integer, ≥ 2) - Highest number to test for primality.

The program checks numbers in the range [2 .. limit].

```bash
threads=4
limit=100000
```

### Variants
Each program implements a different strategy for task division and result printing.

* Variant 1 - Range-split, immediate print
    * The search range is divided evenly among threads.
	* Each thread prints primes as soon as they are found.
	* Output includes thread ID and a timestamp.
* Variant 2 - Range-split, deferred print
	* The search range is divided evenly among threads.
	* Each thread stores its results.
	* Printing happens only after all threads finish, ensuring cleaner output.
* Variant 3 - Per-number parallel divisibility test, immediate print
	* Threads cooperate on testing a single number’s primality (each checks a subset of divisors).
	* If the number is prime, it is printed immediately.
	* Output shows the thread ID and timestamp of the last tester thread that confirmed primality.
* Variant 4 - Per-number parallel divisibility test, deferred print
	* Threads cooperate per number as in Variant 3.
	* All results are collected first.
	* Primes are printed only after the computation finishes.