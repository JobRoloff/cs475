## Starting Point
  What DCQ is (recursion on a×b, split rule, leaves = BKQ's padded kernel on the L grid),
  machine/cores/TPP, gate, baselines carried over: OPQ 9.41, BKQ 29.27.

## Part A: Sequential DCQ

### Experiment 1: DCQ vs earlier sequential variants

#### Question

Does replacing BKQ's loops over tiles with a recursive divide and conquer change single-core performance?

#### Hypothesis

DCQ will be slightly slower than BKQ. The recursion adds overhead that the
loop version does not have: managing recursive calls on top of the same
leaf work.

#### Experiment Design

DCQ recurses on (a range of `a`) × (a range of `b`), adding into `c` at
offset `i0 + j0`. It splits the longer range in half, or both ranges when
they are within a factor of 2 of each other. Split points are rounded to
whole leaf blocks, so every leaf starts at a multiple of the leaf size L.
When both ranges are ≤ L, the leaf runs BKQ's final kernel from PA3
(padded tiles, R = 8, U = 3). The padded, shifted copies of `b` are built
once before the recursion starts.

    dc(i0, na, j0, nb):
        if na <= L and nb <= L:  leaf(i0, na, j0, nb)
        else: split, then dc(LL); dc(LH); dc(HL); dc(HH)

L = 1,280 (BKQ's best single-core block size in PA3), 100,000 × 100,000
(20 GFLOP), E-core 16, 5 reps: `./run_sweep.sh DCQ "1" 5 -dA 100000 -dB 100000`.

#### Experiment Run

Gated (2026-10-28): `.check` 42/42 oracle, 14/14 gold (terminal output, not
saved). `run_sweep.sh` keeps only the median, so the individual reps were not
kept. OPQ and BKQ are PA3's recorded baselines (2026-10-20). [BKQ rerun in
the same session: pending.]

#### Analysis

| variant | GFLOP/s | % of TPP (35.2) | × OPQ |
|---|---|---|---|
| OPQ (PA2/PA3) | 9.41 | 26.7% | 1.00 |
| BKQ (PA3, block 1,280) | 29.27 | 83.2% | 3.11 |
| DCQ (leaf 1,280) | **29.46** | **83.7%** | **3.13** |

DCQ: median 0.679 s over 5 reps, 29.46 GFLOP/s.

DCQ is not slower than BKQ: it is 0.6% faster. That difference is inside PA3's rep-to-rep spread (up to 0.8%), and BKQ was measured on a different
day, so the two are the same within measurement error.

## Part B: Parallel DCQ
DCQ splits the (i, j) rectangle into four quarters. Each quarter adds into
a range of `c` fixed by where it sits:

```
              b  →
          bL (j < n/2)   bH (j ≥ n/2)
        +--------------+--------------+
  aL    |      LL      |      LH      |
  (i<n/2)              |              |
  a     +--------------+--------------+
  ↓ aH  |      HL      |      HH      |
  (i≥n/2)              |              |
        +--------------+--------------+

  output k = i + j:

  c:  0          n/2          n           3n/2         2n
      |-----------------------|                           LL  [0,    n)
                  |-----------------------|               LH  [n/2,  3n/2)
                  |-----------------------|               HL  [n/2,  3n/2)
                              |-----------------------|   HH  [n,    2n)
```

- **LL and HH** never share an output. They can always run together.
- **LH and HL** write exactly the same range. They can never run together
  unless one writes somewhere else.
- **LH (or HL)** overlaps half of LL and half of HH.

### Setup for all of Part B

- **Size:** 99,999 × 99,999, the harness default (100,000 coefficients per
  operand). This differs from Experiment 1 by one degree; the work is
  2.0 × 10¹⁰ FLOP either way.
- **Leaves:** L = 1,280, so a 79 × 79 grid of 6,241 leaves (78 full blocks
  plus a 160-wide edge per side). Leaf sizes with L % 8 ≠ 0 are rejected,
  since a partial row group would write past its own outputs.
- **Task cutoff:** 2 leaves per side; below it the recursion runs
  sequentially, as in DCQ.
- **Binding:** `run_sweep.sh`'s: E-cores 16–31, `OMP_PROC_BIND=close`, places
  one per cluster first (16, 20, 24, 28, 17, …). libgomp defaults otherwise
  (`OMP_WAIT_POLICY=passive`, `GOMP_SPINCOUNT=300000`). GCC 11.5.0.
- **Checks before any timing:**
  - the gate (`.check`, `.check-par`: 42 oracle, 14 gold cases);
  - `verify_timed`: the timed build's intrinsics path against a
    double-precision reference, 32 configurations at 1, 3 and 16 threads.
    The gate alone only tests the scalar checking path. A planted off-by-one
    in the timed leaf failed 24 of 32 configurations (`diag/verify_timed_planted.txt`).
- **Diagnosis tool:** a TRACE build that records, per thread, its CPU,
  leaves run and time in leaves, which thread ran `single`, and the time
  of each phase of a call. It is never used for reported rates.

Paths below use `diag/` for `results/DCP1/diagnostic/`.

### Experiment 2: reduction (Goal 1)

#### Question

Can a reduction remove every ordering between the four children, and give
close to linear speedup on 16 E-cores?

#### Hypothesis

Spawning all four children of every split as tasks, inside a taskgroup with
`task_reduction`, removes every ordering between them: each thread adds into
a private copy of `c`, so subproblems that write the same outputs (LH and HL,
and their overlap with LL and HH) can run at the same time. The span drops
from about 1,000 leaves (the taskwait-only order) to about one leaf plus the
merge, so the speedup is limited only by overhead outside the leaves:

- 1 thread: within 1% of sequential DCQ (29.46 GFLOP/s); one private copy and
  the task creation cost a fraction of a millisecond.
- 16 threads: about 15.3–15.5× (450–457 GFLOP/s). Linear would be
  679 / 16 = 42.2 ms [sic: 42.4]; add about 1 ms for the merge (PA3's BKP1
  measured 1.06 ms) and about 0.4 ms of setup on one thread, giving about
  43.8 ms.
- The curve should be close to linear at every thread count, with no
  staircase, because leaves are dealt one at a time.

#### Experiment Design

    #pragma omp parallel
    #pragma omp single
    #pragma omp taskgroup task_reduction(+: c[0:out_len])
        task: dc(root)                 // the root is created as a task

    dc(node):  above the cutoff: each child as
               #pragma omp task in_reduction(+: c[0:out_len])
               (no taskwait)
               at or below the cutoff: sequential DCQ

Each task reads `c` inside the task region, so its leaves write the
executing thread's private copy.

**Race control:** the same build without the reduction clauses (`-DRACE`),
so all tasks write the shared `c`. It must fail the gate.

#### Experiment Run

- Gate (2026-10-29): `.check` and `.check-par` (16 threads) 42/42, 14/14.
- Race control at 16 threads: **fails**, oracle 8/42 and gold 2/14, all at
  sizes that create more than one task (`diag/race_gate.txt`).
- Sweep: `./run_sweep.sh DCP1 "1 … 16" 5` (medians only; reps not kept).

#### Results

| T | s | speedup | | T | s | speedup |
|---|---|---|---|---|---|---|
| 1 | 0.6802 | 1.00 | | 9 | 0.0898 | 7.58 |
| 2 | 0.6810 | **1.00** | | 10 | 0.0809 | 8.41 |
| 3 | 0.3427 | 1.98 | | 11 | 0.0729 | 9.33 |
| 4 | 0.2299 | 2.96 | | 12 | 0.0672 | 10.13 |
| 5 | 0.1740 | 3.91 | | 13 | 0.0625 | 10.89 |
| 6 | 0.1401 | 4.86 | | 14 | 0.0580 | 11.73 |
| 7 | 0.1012 | 6.72 | | 15 | 0.0546 | 12.47 |
| 8 | 0.1009 | 6.74 | | 16 | 0.0510 | **13.34** |

#### Analysis

- **1 thread:** 0.6802 s, within 0.2% of DCQ. Supported.
- **16 threads:** 13.34× (51.0 ms), against the predicted 15.3–15.5×
  (43.8 ms). Refuted.
- **Shape:** 2 threads are no faster than 1, and from 3 threads on the
  speedup trails the thread count by about one, i.e. close to T − 1.
  Refuted: one thread's worth of work is missing. Sub-experiments 2a–2g find
  out why.

### Experiment 2a: which thread is missing?

- **Question:** do two threads share a core, or does one thread do no work?
- **Design:** TRACE build, 1 call per process, at 1–16 threads; then a rerun
  that also records which thread ran `single`.
- **Result** (`diag/trace_*.err`):

  | T | wall (s) | idle thread (cpu) | leaves per other thread | ms in leaves per other thread |
  |---|---|---|---|---|
  | 2 | 0.6837 | 0 (16) | 6,241 | 679.6 |
  | 4 | 0.2309 | 0 (16) | 2,069–2,091 | 226.5–226.8 |
  | 8 | 0.1026 | 4 (17) | 883–903 | 97.2–97.4 |
  | 16 | 0.0529 | 9 (22) | 409–425 | 45.2–45.6 |

  In the rerun (`diag/trace2_*.err`), the idle thread was **the thread that
  ran `single`** in all 15 runs with T ≥ 2.
- **Conclusion:** every thread runs on its own core, so placement is ruled
  out. Exactly one thread runs 0 leaves, and it is always the `single`
  thread. The others are balanced within 1%, at about 0.109 ms per leaf.
  With 680 ms of leaves spread over T − 1 threads, this accounts for the
  speedup pattern exactly.

### Experiment 2b: where the rest of the time goes

- **Prediction (Experiment 2):** merge about 1 ms, setup about 0.4 ms.
- **Design:** the TRACE build's phase timing, 1 call per process.
- **Result** (ms, `diag/trace2_*.err`):

  | T | setup | start → first leaf | leaf span | merge | total |
  |---|---|---|---|---|---|
  | 1 | 2.11 | 0.71 | 681.16 | 0.15 | 684.21 |
  | 2 | 1.21 | 1.19 | 680.51 | 0.32 | 683.31 |
  | 8 | 1.34 | 2.22 | 97.88 | 0.78 | 102.31 |
  | 16 | 1.32 | 2.96 | 47.05 | 1.00 | 52.43 |

- **Conclusion:** the merge matches the prediction (1.0 ms at 16 threads).
  Setup is about 3× the prediction. Start → first leaf (up to 5.6 ms) was not
  predicted; most of it is the cost of a process's first call, since each
  trace ran only one call (Experiment 2g's 3-call traces show 0.005 ms on
  later calls). All of these are small next to the idle thread's cost of
  1/T of the leaf time.

### Experiment 2c: OpenMP reduction on the parallel region (TRED=2)

- **Hypothesis:** the `single` thread waits at the end of the taskgroup and
  runs no tasks there. Putting the reduction on the parallel region
  (`reduction(task, +: c)`) means no thread waits at a taskgroup end, so the
  idle thread disappears.
- **Design:** the reduction clause moves to `#pragma omp parallel`; tasks
  keep `in_reduction`. The `taskgroup` construct itself stayed, without its
  clause, which went unnoticed at the time (see 2g).
- **Result** (1 call per process, `diag/t2trace_*.err`): the `single` thread
  ran **0 leaves** at 2, 8 and 16 threads (wall 0.684, 0.103, 0.055 s).
- **Conclusion at the time:** refuted. Revisited in 2g: this build never
  removed the taskgroup wait.

### Experiment 2d: hand-written per-thread reduction (TRED=3)

- **Hypothesis:** if the idle thread comes from OpenMP's reduction machinery,
  a reduction with no OpenMP reduction clauses removes it.
- **Design:**
  - one copy of `c` per thread, allocated each call, zeroed by its own thread,
    then a barrier;
  - tasks with no reduction clauses; each sequential subtree writes the copy
    of the thread running it (`omp_get_thread_num()`);
  - after the `single`'s barrier, an `omp for` merge over cache-line slices
    of `c`.

  The checking build merges its Kahan compensation with
  total = Σ(copy − comp), c = (float)total, comp = c − total.
- **Result** (3 calls per process, `diag/t3trace_*.err`): the `single` thread
  was idle in **8 of 9 calls** at 2, 8 and 16 threads. The one call with no
  idle thread (16 threads) took 45.4 ms, about 15.0×:

  | phase (16 threads, ms) | setup | zeroing copies | leaves | merge | total |
  |---|---|---|---|---|---|
  | TRED=3 | 0.92 | 0.78 | 43.44 | 0.21 | 45.40 |

- **Conclusion at the time:** "the reduction is not the cause". Also:
  without an idle thread the design reaches the hypothesis's speed, and the
  hand-written merge (0.21 ms) is 5× cheaper than `task_reduction`'s (1.0 ms).
  The taskgroup was still present here too (see 2g).

### Experiment 2e: making the creating thread run its children (SPAWN=2, 3)

- **Hypothesis:** a thread waiting at a `taskwait` runs its own child tasks.
  SPAWN=3 (run the last child directly) should make an idle `single` thread
  impossible.
- **Design** (on TRED=3): SPAWN=2 adds a `taskwait` after creating the
  children; SPAWN=3 creates all but the last as tasks, runs the last
  directly, then waits.
- **Result** (`diag/t3s2trace_*`, `diag/t3s3trace_*`): an idle thread in
  **6 of 9 calls** for each. Both were slower than SPAWN=1 in 11 of their 12
  calls at 8 and 16 threads, with less even load.
- **Conclusion:** refuted.

### Experiment 2f: the root task and the wait policy

- **Finding from the code:** the `single` block creates the root as a task
  and then waits at the end of the taskgroup. So the `single` thread never
  runs the root, and SPAWN=3's premise did not hold.
- **Hypothesis:** the waiting thread is asleep and not woken when new tasks
  appear.
- **Design:** TRED=3 trace, 3 calls per process at 2, 8 and 16 threads, under
  the default settings, `OMP_WAIT_POLICY=active`, and
  `GOMP_SPINCOUNT=infinite`.
- **Result** (`diag/env_*`):

  | setting | calls with an idle thread |
  |---|---|
  | default | 7 / 9 |
  | `OMP_WAIT_POLICY=active` | 8 / 9 |
  | `GOMP_SPINCOUNT=infinite` | 8 / 9 |

  The root ran on a thread other than the `single` thread in all 27 calls.
- **Conclusion:** refuted. The waiting thread isn't asleep; it finds
  nothing it is allowed to run.

### Experiment 2g: removing the taskgroup (NOTG) or running the root directly (ROOTDIRECT)

- **Finding from the code:** `#pragma omp taskgroup` was present in every
  version. TRED=2 and TRED=3 only changed its clause, so 2c and 2d never
  tested what they were meant to.
- **Hypothesis:** the `single` thread waits at the taskgroup's end, and there
  it picks up almost none of the root's descendants, which are created inside
  other tasks. (This libgomp behaviour is not verified from its source.)
  Removing the taskgroup, or having the `single` thread run the root itself,
  removes the idle thread.
- **Design:** T3NG and T2NG are TRED=3 and TRED=2 with no taskgroup; the
  `single`'s barrier or the region's end waits instead. RD is TRED=1 with the
  `single` thread calling the root directly; RDS3 is RD plus SPAWN=3. 3 calls
  per process at 2, 8 and 16 threads.
- **Result** (`diag/tr_*`):

  | build | calls with an idle thread |
  |---|---|
  | T3NG (hand-written, no taskgroup) | **0 / 9** |
  | T2NG (OpenMP reduction, no taskgroup) | **0 / 9** |
  | RD | 2 / 9 (both the first call of their process) |
  | RDS3 | 0 / 9, but the `single` thread's share varied from 21 to 800 leaves |

- **Conclusion:** supported for NOTG. The first hypothesis (2c) was right;
  its test was not.

### Experiment 2: final results

All builds gated and checked with `verify_timed` (`diag/verify_timed_notg_rootdirect.txt`).
`DCP1/tools/sweep_raw.sh`: for each T the four builds ran in turn, 5 reps
each, every rep saved (`results/DCP1/sweep_raw_notg_rootdirect_20261029_0959.csv`).
Speedup is against DCP1's 1-thread median, 0.6802 s.

| T | DCP1 (taskgroup + `task_reduction`) | T2NG (OpenMP reduction, no taskgroup) | **T3NG (hand-written, no taskgroup)** | RDS3 |
|---|---|---|---|---|
| 1 | 1.00 | 1.00 | 1.00 | 1.00 |
| 2 | 1.00 | 1.99 | 2.00 | 1.98 |
| 4 | 2.96 | 3.94 | 3.98 | 3.89 |
| 6 | 5.81 | 5.85 | 5.93 | 5.22 |
| 8 | 7.63 | 7.70 | 7.87 | 6.90 |
| 10 | 8.48 | 9.51 | 9.77 | 8.45 |
| 12 | 10.15 | 11.27 | 11.56 | 9.82 |
| 14 | 11.74 | 12.83 | 13.48 | 11.03 |
| 16 | 13.30 | 14.34 | **15.22** | 12.20 |
| GFLOP/s at 16 | 391.0 | 421.8 | **447.7** | 358.8 |
| largest rep spread | 50% | 5.9% | 4.9% | 12.8% |

(Every thread count is in the csv.)

DCP1's reps fall into two groups. At 2 threads they were 0.68 s (three reps)
or 0.34 s (two). That fits the idle thread appearing in some calls and not
others, so DCP1's medians depend on how many reps were fast. This wasn't
verified by tracing those reps.

#### Conclusions

- **Goal 1 is met by T3NG:** 15.22× at 16 threads (447.7 GFLOP/s, 79.5% of
  the 16-core TPP of 563.2), and 0.6797 s on 1 thread (within 0.1% of DCQ).
  The curve is close to linear at every count (2.00, 2.99, 3.98, … ×) with no
  staircase. It falls just short of the predicted 15.3–15.5×: 44.7 ms
  against 43.8.
- **Where T3NG's 2.2 ms over linear goes** (42.5 ms; TRACE calls 2–3 at
  16 threads, `diag/tr_T3NG_16.err`):
  - setup on one thread: 0.3–0.9 ms;
  - zeroing the private copies: 0.8–0.9 ms (not in the prediction);
  - merge: 0.2 ms (the prediction was 1 ms);
  - end-of-run imbalance: 0.5–0.7 ms.

  The leaf work itself is not inflated: 680.6–681.0 ms summed over 16 threads,
  against 679.6–680.1 ms on 1 thread.
- **With OpenMP's own reduction** (T2NG), the best is 14.34×. The runtime
  spends about 3 ms before the first leaf even on later calls, and about
  0.9 ms merging.
- **DCP1 as first built** (taskgroup + `task_reduction`) loses one thread to
  the taskgroup wait in most calls: 13.30×.
- **Lesson:** the right explanation was the first one (2c), but the test of
  it didn't remove the taskgroup, so it looked refuted. Two more
  explanations (2d–2f) were tested before the code was read closely. Check
  that an experiment removes what it claims to remove.
- **Open:**
  - the libgomp mechanism behind the taskgroup wait is not verified;
  - RD's idle thread appeared only on the first call of a process;
  - the 3 ms before the first leaf in T2NG and RD is not explained.
