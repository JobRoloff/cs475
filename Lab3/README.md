---
Lab: 3
---

## Tasks

A: Performance comparisons between Simple and MKL Matrix Multiplication algorithms
B:
C:

## Task A — naive matrix multiply vs. MKL `dgemm`

### Question

How much faster is Intel MKL's `dgemm` than a naive triple-nested-loop matrix
multiply for similarly sized inputs, and how does MKL's `dgemm` scale as the
thread count increases?

### Hypothesis

MKL `dgemm` will be far faster than the naive loop even on a single thread,
because MKL uses register/cache blocking (tiling) plus a hand-tuned vectorized
micro-kernel, whereas the naive `C[i][j] += A[i][k]*B[k][j]` loop walks column
`j` of `B` with stride `N`, thrashing the cache. Compiler optimization (`-O2`,
`-O3 -march=native`) should recover some vectorization for the naive loop but
cannot introduce blocking, so a large gap remains. MKL should scale close to
linearly up to the 8 physical P-cores, then sub-linearly once work spills onto
the E-cores / SMT siblings.

### Experiment — Constants

- **Machine:** Intel Core i9-13900K (8 P-core + 16 E-core = 32 hardware threads),
  62 GB RAM, machine idle (load average < 1 aside from the benchmark itself).
- **Compiler:** `gcc` 11.5.0 (the sample Makefile uses `gcc`, not `icx`).
- **MKL:** oneMKL 2023.0 Update 2, loaded via `module load compilers/icx`
  (`compilers/icc` from the lab handout no longer exists on this system —
  `module load compilers/icc` gives "Permission denied"). `MKL_VERBOSE=1`
  reports the **AVX2** code path — Raptor Lake has no AVX-512.
- **Primary workload:** C = A·B with A, B, C all 2000×2000 → 2·2000³ ≈ 1.6·10¹⁰
  FLOPs per multiply. `alpha = 1.0`, `beta = 0.0`.
- **Naive program** (`simple_matrix.c`, unmodified): `float`, one `clock()`-timed
  run, no warm-up, no internal repeat loop.
- **MKL program** (`matrix_multiplication/src/dgemm_with_timing.c`): `double`,
  one warm-up call + `LOOP_COUNT = 10` timed calls, reports the **average
  ms/call** measured with `dsecnd()`, buffers 64-byte aligned via `mkl_malloc`.
  The **only** source edit made: line 42 `m = p = n = 2000` (shipped as
  `2000 / 200 / 1000`) so the two programs do the same amount of work.
- **Run-to-run variation control:** MKL program already averages 10 calls after a
  warm-up; each thread count run 3×, median reported. Naive program run 5×, first
  run discarded (cold cache / page faults), median of runs 2–5 reported.
  `uptime` checked before/after each batch.

### Experiment — Steps

```bash
module load compilers/icx                       # provides MKL (icc module is gone)

# 1. MKL dgemm, matched to 2000^3 (one edit: dgemm_with_timing.c line 42 -> m=p=n=2000)
cd matrix_multiplication
make -B release/dgemm_with_timing
for t in 1 2 4 8 16 32; do
  for rep in 1 2 3; do
    MKL_NUM_THREADS=$t OMP_NUM_THREADS=$t ./release/dgemm_with_timing | grep milliseconds
  done
done

# 2. Naive program, three optimization levels, no source edit
gcc -O0               ../simple_matrix.c -o simple_O0
gcc -O2               ../simple_matrix.c -o simple_O2
gcc -O3 -march=native ../simple_matrix.c -o simple_O3
for b in simple_O0 simple_O2 simple_O3; do for i in 1 2 3 4 5; do ./$b | grep Time; done; done

# 3. Handout sanity commands
make ; make run_dgemm_example
./release/matrix_multiplication          # the sample's own triple-loop reference
```

### Observed Results

**Table 1 — Naive `simple_matrix.c` (single-threaded, 2000³ `float`), seconds**
(first run discarded, median of runs 2–5):

| Build | trial times (s) | median (s) | GFLOP/s |
| --- | --- | --- | --- |
| `gcc -O0` | 41.68 / 42.88 / 45.32 / 49.01 / 45.22 | **45.27** | 0.35 |
| `gcc -O2` | 11.19 / 11.72 / 11.73 / 11.65 / 11.58 | **11.69** | 1.37 |
| `gcc -O3 -march=native` | 11.51 / 12.04 / 11.50 / 11.82 / 12.14 | **11.93** | 1.34 |

**Table 2 — MKL `dgemm` (2000³ `double`), ms per multiply**
(each invocation is itself a 10-call average; median of 3 invocations):

| MKL_NUM_THREADS | trial ms | median ms | GFLOP/s | speedup vs 1 thread |
| --- | --- | --- | --- | --- |
| 1  | 348.92 / 374.50 / 349.22 | **349.22** | 45.8  | 1.00× |
| 2  | 174.22 / 173.55 / 174.58 | **174.22** | 91.8  | 2.00× |
| 4  | 88.59 / 88.65 / 88.47    | **88.59**  | 180.6 | 3.94× |
| 8  | 56.95 / 50.16 / 51.74    | **51.74**  | 309.2 | 6.75× |
| 16 | 64.52 / 63.52 / 65.08    | **64.52**  | 248.0 | 5.41× |
| 32 | 47.10 / 46.71 / 46.89    | **46.89**  | 341.2 | 7.45× |

**Table 3 — Cross-check, identical Intel timing harness, shipped sizes
2000×200×1000 `double` (8·10⁸ FLOPs)** — the sample's own
`matrix_multiplication` (triple loop) vs a copy of `dgemm_with_timing` rebuilt at
those shipped sizes; both use the same warm-up + 10-call `dsecnd()` average,
median of 5 invocations:

| Program | median ms | GFLOP/s | vs. triple loop |
| --- | --- | --- | --- |
| `matrix_multiplication` (triple nested loop) | 1094.7 | 0.73 | 1× |
| MKL `dgemm`, 1 thread  | 17.34 | 46.1  | **63×** |
| MKL `dgemm`, 32 threads | 2.64  | 303   | **414×** |

### Headline numbers

- Naive `simple_matrix.c` at `-O2`: **≈ 11.7 s** per 2000×2000×2000 multiply.
- MKL `dgemm`, **1 thread**: **≈ 349 ms** → **≈ 33× faster** than naive `-O2`.
- MKL `dgemm`, **32 threads** (best): **≈ 47 ms** → **≈ 250× faster** than naive `-O2`.
- Same-harness cross-check (2000×200×1000): MKL is **63×** faster single-threaded
  and **414×** faster at 32 threads than the equivalent triple loop.

### Interpretation

- **Single-thread MKL already crushes the naive loop** (33× at 2000³, 63× in the
  same-harness test). Almost all of that is algorithmic layout, not threads: MKL
  tiles the matrices so the working set stays in L1/L2 and runs an AVX2
  micro-kernel with FMA, reaching ~46 GFLOP/s. The naive loop's inner `B[k][j]`
  access strides by `N` floats (8 KB), so nearly every iteration misses cache;
  ~1.4 GFLOP/s is the result.
- **`-O3 -march=native` barely helps** (11.9 s vs 11.7 s at `-O2`, essentially a
  wash). The compiler cannot vectorize the reduction across the strided `B`
  column and cannot invent cache blocking, so the memory bottleneck is
  unchanged. `-O0` is ~4× slower still (45 s) — that difference is pure
  scalar-code overhead.
- **MKL thread scaling is near-linear to 4 threads** (2.00×, 3.94×) and still
  good at 8 (6.75×, one thread per P-core). At **16 threads it gets *worse*
  (5.41×)** — the scheduler mixes fast P-cores with slow E-cores and the fast
  threads stall at the join barrier waiting for the slow ones. At **32 threads
  it recovers to 7.45×** (46.9 ms) because every core is now busy; this size is
  small enough that ~7.5× is the practical ceiling.
- **Precision caveat:** the naive program is `float` and MKL is `double`, so the
  naive loop moves half the bytes MKL does. The genuine algorithmic gap is
  therefore *larger* than the raw time ratio suggests — a `double` naive loop
  would be slower than the numbers above.

| Algo Type | trial 1 time | trial 2 time | trial 3 time |
| --------- | ------------ | ------------ | ------------ |
| Simple (`simple_matrix.c`, `-O2`, 2000³) | 11.72 s | 11.73 s | 11.65 s |
| MKL `dgemm` (1 thread, 2000³) | 348.9 ms | 374.5 ms | 349.2 ms |
| MKL `dgemm` (32 threads, 2000³) | 47.1 ms | 46.7 ms | 46.9 ms |
