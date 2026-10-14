# Lab: Tasks, Taskgroups, and Task Reductions

In this lab you count how many times the naive recursive Fibonacci function is called on each argument, first serially and then with OMP tasks. Many tasks updating one shared array is exactly the situation task reductions are for. You will need taskgroups and task reductions for the D&C and PolyMult assignments.

## The program

Download and untar the provided `fibcount.tar` file. It contains the serial program `Fib_count.c`, the timer, a `Makefile` and `run_sweep.sh`.

`Fib_count.c` computes `fib(n) = fib(n-1) + fib(n-2)`, with `fib(0) = 0` and `fib(1) = 1`, with the naive double recursion. It keeps a global array `long result[MAXN+1]`: every time a call `fib(x)` returns, it increments `result[x]`. The increment is done in the caller, right after the call returns, and `main` does it for the top-level call `fib(n)`. At the end, `result[x]` is the number of times `fib` was called with argument `x`. The debug build prints the whole array.

You know in advance what the array must contain. With F(k) the k-th Fibonacci number:

- for x >= 1, `result[x]` = F(n-x+1): read from x = n down to x = 1, the array is the Fibonacci sequence 1, 1, 2, 3, 5, 8, ...
- `result[0]` = F(n-1) is the exception;
- the total number of calls is 2F(n+1) - 1;
- `result[1]` = fib(n), which is a free sanity check.

For example, for n = 6, `result[6]` down to `result[0]` is `1 1 2 3 5 8 5`, a total of 25 calls.

Every program takes n as its first command line argument (default 30). Put each version in its own file, add it to the `Makefile` (with `FLAGS`, i.e. `-O3 -fopenmp`, for the parallel versions), and keep `Fib_count.c` as the serial reference. Time on a quiet fish machine. `./run_sweep.sh ./Fib_count "1" 5 40` gives the median of 5 runs with n = 40; any arguments after the repetition count are passed on to the program.

## Part 1: serial

1. Run `make`, then `./Fib_count_debug 10`, and compare the array with the pattern above.
2. Add a check function to `Fib_count.c` that compares `result[]` against the formulas above (and the total against 2F(n+1) - 1) and prints `OK` or `WRONG`. Use it in every version. In the debug build, also print the expected values next to the array. Use `long` for the Fibonacci numbers and for the total: for n = 44 the total is 2F(45) − 1 ≈ 2.27 billion, which overflows an `int`.
3. Verify it for n = 10 printed in full, and for a larger n checked automatically.
4. Why does the pattern hold? Why is `result[0]` different?

## Part 2: naive tasks

Copy `Fib_count.c` into `Fib_count_race.c`. Parallelize it with `#pragma omp parallel` and `single` in `main`. Make each recursive call a `#pragma omp task` with `shared(x)` (resp. `shared(y)`), and `taskwait` before summing. Put each increment inside the task, right after the call: `{ x = fib(n-1); result[n-1]++; }`.

Keep `result[...]++` a plain increment on the shared array.

Run it with n = 30 and 4 to 8 threads, at least 5 times (e.g. `for i in 1 2 3 4 5; do OMP_NUM_THREADS=8 ./Fib_count_race 30; done`). What happens to the counts, and to fib(n)? Do the counts change from run to run? The race is nondeterministic, so an individual run can pass; report how many of your runs failed.

Explain what you see.

## Part 3: atomic

Copy `Fib_count_race.c` into `Fib_count_atomic.c` and fix the race with `#pragma omp atomic` on each increment.

Verify that the counts are now correct, and time it against the serial version for n = 25 and 30 and several thread counts.

## Part 4: taskgroup and task reduction

Copy your Part 2 code, `Fib_count_race.c`, into `Fib_count_red.c`. In `main`, wrap the top-level call in

```c
#pragma omp taskgroup task_reduction(+: result)
```

and add `in_reduction(+: result)` to every task that touches `result`:

```c
#pragma omp task shared(x) in_reduction(+: result)
{ x = fib(n-1); result[n-1]++; }
```

Each participating task now increments a private copy of the array. The private copies are added into the global `result` at the end of the taskgroup, and only then is the global array complete. So the increment of `result[n]` for the top-level call goes in `main`, after the taskgroup.

Keep each increment written inside the task's braces. The private copy replaces `result` only in the code written inside the task construct. An increment at the top of `fib`, or in any function called from the task, updates the shared global array, i.e. it races again, even though the task has `in_reduction`. (This is also why the increments are done by the caller.)

Verify the counts for several thread counts, and time it for n = 25 and 30.

*Optional:* drop `x`, `y` and the `taskwait` altogether, and only count. Since `result[1]` = fib(n), the reduction alone gives you the answer after the taskgroup. Why is this correct without any `taskwait`?

## Part 5: performance and cutoff

Copy `Fib_count_red.c` into `Fib_count_red_cutoff.c` and add a cutoff: a task whose argument is below `CUTOFF` calls a serial function instead of `fib`, so it spawns no more tasks. Read `CUTOFF` from the second command line argument, so that `./run_sweep.sh ./Fib_count_red_cutoff "1 2 4 8" 5 40 25` runs n = 40 with cutoff 25.

The serial function still has to count. By the rule in Part 4, if it increments the global `result`, the counts are wrong. Pass it the array to count into instead. Inside the task construct, `result` names the task's private copy, so passing it passes the private copy:

```c
#pragma omp task shared(x) in_reduction(+: result)
{ x = (n-1 < CUTOFF) ? fib_serial(n-1, result) : fib(n-1);
  result[n-1]++; }
```

Do the same for the atomic version, in `Fib_count_atomic_cutoff.c`, with atomic increments in its serial function.

Time the serial version and the reduction version with cutoff for n = 35, 40 and 44, for 1 to 8 threads and a few cutoff values. The atomic version with cutoff is much slower, so run it for n = 35 and 40 only.

Make a timing table and a speedup curve for your best cutoff.

## Deliverables

- Code for each version, and your `Makefile`.
- Verified count output: n = 10 printed in full, and a larger n checked automatically, for each version (for Part 2, show the failing runs and how many of your runs failed).
- A timing table and a speedup curve.
- Short answers to the questions below.

Submit everything as a single `.zip` or `.tar` file.

## Questions

1. Why does `result[x]` = F(n-x+1) hold, and why is `result[0]` different?
2. In Part 2, why are the counts wrong while fib(n) is right?
3. `taskwait` waits only for the direct children of the current task; a taskgroup waits for all tasks created in it and all of their descendants. Why does the reduction need a taskgroup rather than a `taskwait`?
4. Why is the reduction faster than atomic, and why does the difference show up much more clearly once you add the cutoff?
5. What does the cutoff do to performance, and why do tiny tasks hurt so much?

> **Note:** task reductions need OpenMP 5.0 (GCC 9 or later). Here `result` is a fixed-size array, so it can be reduced as a whole. If it were a pointer to a heap array, you would name an array section instead, e.g. `task_reduction(+: result[0:n+1])` with the same section in every `in_reduction`.
