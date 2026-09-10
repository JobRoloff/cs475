# Lab 3 — Task A: naive matrix multiply vs. MKL `dgemm` timing comparison

## Context

Task A asks: *"Compare the execution time for the simple matrix multiply code and the
dgemm code (MKL) for similar sized inputs. Report the approximate execution time while
avoiding any major run-to-run variations."*

As shipped the two programs are **not comparable** — different sizes and precision:

| Program | File | Size (as shipped) | Type | Timing built in? |
|---|---|---|---|---|
| Naive | `simple_matrix.c` | N = 2000 square (2·2000³ ≈ 1.6e10 FLOPs) | `float` | **No** — 1 run, `clock()`, no warm-up, no loop |
| MKL `dgemm` | `matrix_multiplication/src/dgemm_with_timing.c` | m=2000, p=200, n=1000 (8e8 FLOPs) | `double` | **Yes** — 1 warm-up call + `LOOP_COUNT=10` timed calls, reports **avg ms/call** via `dsecnd()` |

**No C files have been modified yet.** Files are pristine.

**Decisions:**
- Make sizes match with **one** edit: `dgemm_with_timing.c` → `m = p = n = 2000`.
- Leave `simple_matrix.c` completely untouched (stays `float`, N=2000).
- MKL: run sequential (1 thread) then a thread-count sweep.

Machine: i9-13900K, 32 threads (8 P-core×2 + 16 E-core), 62 GB RAM, idle (load < 0.3).
gcc 11.5.0. MKL via `module load compilers/icx` (lab's `compilers/icc` is gone from this
system).

## Approach

### 1. One code edit — match the MKL sample to 2000³

`matrix_multiplication/src/dgemm_with_timing.c`, line 42:

```c
m = 2000, p = 2000, n = 2000;      // was: m = 2000, p = 200, n = 1000;
```

Nothing else changes. Keep the warm-up run, `LOOP_COUNT`, `dsecnd()` averaging, and
64-byte `mkl_malloc` alignment — that is precisely the "avoid run-to-run variation"
machinery. Memory: 3 × 2000² × 8 B ≈ 96 MB.

If, after building, the program prints *"highly recommended to define LOOP_COUNT ... as
N"*, bump `#define LOOP_COUNT 10` to that N and rebuild (keeps total runtime ≈ 1 s for
a stable average).

Build from `matrix_multiplication/`:

```bash
module load compilers/icx
make -B release/dgemm_with_timing
```

(Makefile links `-lmkl_rt`; the module supplies headers/libs via `CPATH`/`LIBRARY_PATH` —
already verified building cleanly.)

### 2. Build the naive program (no source edit) — optimization-level sweep

All single-threaded; `-O` level only changes the single-thread constant factor
(vectorization, unrolling), not the algorithm or thread count.

```bash
cd matrix_multiplication      # keep binaries out of the repo root; or build in place
gcc -O0               ../simple_matrix.c -o simple_O0
gcc -O2               ../simple_matrix.c -o simple_O2
gcc -O3 -march=native ../simple_matrix.c -o simple_O3
```

Primary baseline for the comparison table = **`-O2`** (a normal build). `-O0` and
`-O3 -march=native` are run for context in the writeup (how much is "just the compiler").

### 3. Run + collect (this is where run-to-run variation is controlled)

**Naive** (`simple_matrix.c` has no internal loop):

```bash
for i in 1 2 3 4 5; do ./simple_O2; done      # prints "Time: <seconds>"
```

Discard run 1 (cold caches / page faults), report **median of runs 2–5**. Repeat for
`simple_O0` and `simple_O3`.

**MKL** (`dgemm_with_timing` already reports a 10-call average per invocation):

```bash
for t in 1 2 4 8 16 32; do
  for rep in 1 2 3; do
    MKL_NUM_THREADS=$t OMP_NUM_THREADS=$t ./release/dgemm_with_timing | grep milliseconds
  done
done
```

Report the **median of the 3 invocations** per thread count (each invocation is itself a
10-call average).

Also capture the MKL build/version once: `MKL_VERBOSE=1 ./release/dgemm_with_timing 2>&1 | head -3`.
Check `uptime` before/after — rerun anything taken while load spiked.

### 4. Deliverable — fill in `README.md`

The `README.md` is already scaffolded (Question / Hypothesis / Constants / Steps /
Observed Results / results table). Populate:

- **Question:** how much faster is MKL `dgemm` than a naive triple-loop for a
  2000×2000×2000 double matrix multiply, and how does MKL scale with threads?
- **Hypothesis:** MKL single-thread ≫ naive (cache blocking + AVX-512 microkernel);
  MKL scales ~linearly with cores up to the P-core count, then sub-linearly on E-cores / HT.
- **Constants:** size 2000³ double (2·2000³ ≈ 1.6e10 FLOPs), same machine, machine idle,
  `alpha=1, beta=0`, gcc 11.5.0, MKL version string, first run discarded, median reported.
- **Steps:** the commands from §1–§3.
- **Observed Results:** the two tables below + 2–4 sentences of interpretation.

**Table 1 — Naive (single-threaded), seconds (median of 4):**

| Build | trial times (s) | median | GFLOP/s = 1.6e10 / median / 1e9 |
|---|---|---|---|
| `gcc -O0` | | | |
| `gcc -O2` | | | |
| `gcc -O3 -march=native` | | | |

**Table 2 — MKL `dgemm`, ms per multiply (median of 3):**

| MKL_NUM_THREADS | trial ms | median ms | GFLOP/s = 1.6e10 / (median/1000) / 1e9 | speedup vs 1 thread |
|---|---|---|---|---|
| 1 | | | | 1.00× |
| 2 | | | | |
| 4 | | | | |
| 8 | | | | |
| 16 | | | | |
| 32 | | | | |

**Headline numbers to state explicitly:** naive `-O2` seconds vs. MKL 1-thread ms vs.
MKL 32-thread ms, and the two ratios (naive→MKL-1-thread, naive→MKL-best).

**Interpretation points:**
- MKL 1-thread already beats naive by a large factor → cache/register blocking (tiling)
  + hand-written AVX-512 kernel; the naive loop's `B[k][j]` column walk thrashes cache.
- `-O3 -march=native` closes only part of the gap → compiler recovers vectorization but
  not blocking.
- MKL thread scaling: near-linear to ~8 (P-cores), then diminishing on E-cores / SMT.
- Precision caveat: naive is `float`, MKL is `double` — naive has half the memory traffic,
  so the real gap is slightly larger than the raw numbers show.

## Files

| File | Change |
|---|---|
| `matrix_multiplication/src/dgemm_with_timing.c` | line 42: `p`, `n` → `2000` (only source edit); maybe bump `LOOP_COUNT` if the program recommends it |
| `README.md` | fill in Task A sections + both results tables |
| `simple_matrix.c` | **unchanged** |
| build outputs (`simple_O0/O2/O3`, `release/*`) | not committed |

## Verification

1. `module load compilers/icx && cd matrix_multiplication && make -B release/dgemm_with_timing` builds clean.
2. `./release/dgemm_with_timing` prints `A(2000x2000)` / `B(2000x2000)` and a `== at <X> milliseconds ==` line.
3. `gcc -O2 ../simple_matrix.c -o simple_O2 && ./simple_O2` prints `The value of N is : 2000` and `Time: <sec>`.
4. Sanity ordering of GFLOP/s: naive `-O0` < naive `-O2` ≤ naive `-O3` < MKL 1-thread < MKL 32-thread; MKL best should be ≫10× naive `-O2`.
5. Per-config repeat runs cluster within ~10–15% before taking the median; if not, check `uptime` and rerun.
