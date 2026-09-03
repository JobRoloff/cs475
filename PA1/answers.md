## Stage 1: Starting Point

```bash
(parallel-programming) bullhead:~/cs475$ cd Lab1
(parallel-programming) bullhead:~/cs475/Lab1$ make all
make: Nothing to be done for 'all'.
(parallel-programming) bullhead:~/cs475/Lab1$ OMP_NUM_THREADS=4 ./bin/Mandelbrot.check-par
The total number of errors is 0
```

## Stage 2: The Scientific Method, Unrolled

**Question**
How does switching from the default static schedule to dynamic scheduling (schedule(dynamic)) affect the speedup curve and load balance for the uneven Mandelbrot row density?

**Hypothesis**

Given an unbalanced workload, if the OpenMP scheduling is switched from static to dynamic, then parallel efficiency will  improve comapred to the static baseline across all thread counts.

This improvement occurs because increasing static binds threads to fixed row assignments, forcing fast threads to idel while bottleneck threads process computationally dense center rows. Under dynamic scheduling, idle threads dynamkcally pull remaining unassinged rows from a shared queue, reclaiming lost CPU cycles and balancing the overall workload.

More spefically,

StaticSpeedup(24) = T1 / T24  
    = 2.902048 / 2.84241 
    = 1.0210

StaticEfficiency(24) = StaticSpeedup(24) / 24 
    = 1.0210 / 24
    = .0424 (4.25%)


Predicted DynamicSpeedup(24) = StaticSpeedup(24) X ImbalanceRatio
    = 1.0210 X 4.1997
    = 4.2878

Predicted DynamicEfficiency(24) = DynamicSpeedup(24) / p 
    = 4.287816 / 24
    = .17865 (17.87%)



**Experiment Design**

* Code Modifications:
  Modified the primary inner loop in src/ directly by replacing the default 
  OpenMP loop construct with `#pragma omp parallel for schedule(dynamic)`.

* Benchmark Parameters:
  - OpenMP Schedule: dynamic (default chunk size of 1)
  - Thread Counts (p): Evaluated across the full sweep p ∈ {1, 2, 4, 8, 12, 16, 20, 24, 28, 32, 36}
  - Image Parameters: Executed via test script with arguments `-pmin 1 -pmax 36 -r 5 -o`
  - Number of Trials: 5 runs per thread count (averaging reported execution time across runs)

* Hardware & Compilation Setup:
  - Compiled using GCC with optimization flags (`-O3 -fopenmp`) via `make all`.
  - Executed on a physical machine with 24 cores and 2 threads per core to evaluate thread counts up to p = 36.


 <!-- 
 
 Using code to do the experiment.. 
 
 1. Change "src/Mandelbrot.c"
 2. Compile the C file
 3. Change lab1's main.py:
 4. Run lab1's main.py
 
 What to change in Mandelbrot.c?
 - OpenMP clause
 
 What to change in main.py?
 - the current main.py generates work_imbalance.csv. we need our main.py to calculate speedup for the given test iteration (first test being static vs dynamic scheduling type)
 - num threads to test
 - num threads to test
 - output file name
 
 Observation / thought: each iteration we change something and then measure before / after speed.

 Idea: Would it be useful to have a way to use cli to automate this through creating a "profiler"?

 Profiler Usage: 
 
 python3 profile.py <output_speedup_fil_name.md> <description(whats being changes. for our 1st hypothesis, this could be "static -> dynamic scheduling")> <>

 Profiler Output:

 - filename.md containing the following:

 - title
 - description of what changed
 - table: 
    - Headers: Trial 1 | ... | Trial n | 
    - Rows: 1 - 36 (number of threads in file execution (p))
 - analytics - for each row:
    - Static Speedup
    - Static Efficiency
    - Actual Dynamic Speedup
    - Actual Dynamic Efficiency
    - 
 -->




## (Omit From Report) Personal Notes

## Speedup

S(p) = T(baseline) / T(p)

speedup of p threads = time for baseline / time of p threads

// note: use max observed time for baseline
// note: use average time of p threads

A model to represent the impact of adding threads to a computational task

Additional variables such as how well work can be divided across threads impact speedup. For an example, if some tasks require more work than others, some threads will wait idle. 

### Schedule Clause Types

- schedule(static) = divides work into equal sized blocks statically before execution
- schedule(static, chunk_size) = round robin work assignment
- schedule(dynamic [, chunk_size]) = hands out work based on available threads
- schedule(guided [, chunk_size]) = chunk sizes decrease exponentially over time