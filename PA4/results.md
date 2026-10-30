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

A saved DCQ baseline at Part B's size (99,999 × 99,999, 5 reps, core 16) was
recorded later: median 0.6792 s, 29.45 GFLOP/s
(`results/DCP2/clamp_check_20261029_1440.csv`). Part B's speedups use it.

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

### Moving from a reduction to taskwait

Near-linear speedup of DCQ was attained by giving each thread its own private
copy of `c` (15.22× on 16 E-cores, T3NG). These copies were combined in
parallel once all tasks were complete.

One limitation of this design is its memory footprint, since each thread
allocates its own copy of `c` (12.8 MB at 16 threads here, and 256 MB at
1,000 × 4,000,000).

In the next set of experiments, we remove the reduction and have every task
write to a single shared copy of `c`. Races are avoided by ordering the
subproblems with `taskwait` instead. Before running anything, we analyze the
work and span of this ordering to predict how much speedup it can keep.

### Experiment 3: taskwait only (Goal 2)

#### Question

Without a reduction, every task writes the shared `c`, and races are avoided
only by ordering the subproblems with `taskwait`. How much speedup does that
ordering leave, and can the work and span of the recursion predict it?

#### Hypothesis

Written before any DCP2 run.

At every split, LL and HH write disjoint outputs and can run together. LH and
HL write the same outputs, and each overlaps LL and HH, so they run after LL
and HH, one after the other:

    stage 1:  LL ∥ HH      taskwait
    stage 2:  LH
    stage 3:  HL

- **Work:** W(n) = 4 W(n/2), 6,241 leaves (79 × 79 at L = 1,280).
- **Span:** S(n) = 3 S(n/2), which grows like n^1.585. That is about
  1,000 leaves with tasks down to single leaves (cutoff 1). With cutoff 2,
  each 2 × 2 block at the bottom runs its 4 leaves in sequence, so the span
  is about 1,350 leaves.
- **Parallelism:** W / S ≈ 6.2 (cutoff 1), or about 4.6 (cutoff 2).
- **Prediction (cutoff 1):** between the greedy lower bound W / (W/P + S) and
  the upper bound min(P, W/S): 4.5–6.2× at 16 threads (about 130–180 GFLOP/s).
  The curve should be close to linear for the first few threads, then
  flatten after about 6–8.
- **Cutoff 2:** 3.6–4.6× at 16 threads.
- **1 thread:** within 1% of DCQ.
- **Refuted if:** above 6.2× (a race, or a shorter span than the analysis
  says), or more than about 10% below 4.5× (a cost the model leaves out).

#### Experiment Design

DCP2 is a new variant with the same leaf kernel, setup and recursion as DCQ,
no reductions, and no `taskgroup`:

    #pragma omp parallel
    #pragma omp single
        dc(root)                      // called directly, not as a task

    dc(node), splitting both ranges, above the cutoff:
        #pragma omp task  dc(LL)
        #pragma omp task  dc(HH)
        #pragma omp taskwait
        dc(LH)                        // called directly
        dc(HL)                        // called directly, after LH returns

- **One-sided splits:** a node that splits only one range runs its two
  halves in sequence, since their outputs overlap. This makes lopsided shapes
  sequential, which is out of scope here.
- **Leaves write only their own outputs**, since `c` is shared:
  - leaf sizes with L % 8 ≠ 0 are rejected;
  - the last row group of a leaf is clamped so it writes no output past
    `ilast + j0 + len − 1`.
- **Cutoff:** `--task-threshold`, default 1 (tasks down to single leaves).
- **Run:** 99,999 × 99,999, L = 1,280, E-cores 16–31 with `run_sweep.sh`'s
  binding, 1–16 threads, 5 reps, cutoffs 1 and 2 alternating at each thread
  count.

#### Experiment Run

| check | result | file |
|---|---|---|
| gate: `.check`, `.check-par` at 1, 3, 16 threads | 42/42 oracle, 14/14 gold at each | `results/DCP2/diagnostic/gate_{seq,1,3,16}.txt` |
| `verify_timed`, 32 configurations at 1, 3, 16 threads | 0 failed at each | `results/DCP2/diagnostic/verify_timed.txt` |
| leaf clamp: DCP2 at 1 thread against DCQ, back to back | 0.6796 s against 0.6792 s (0.07% slower) | `results/DCP2/clamp_check_20261029_1440.csv` |
| race controls | both fail (Experiment 3a) | `results/DCP2/diagnostic/race{1,2}_gate.txt` |

Sweep: `results/DCP2/sweep_raw_20261029_1447.csv` (160 rows, every rep).
Speedup is against DCQ's 0.6792 s at the same size.

#### Results

| threads | lower bound (cutoff 1) | **cutoff 1** | cutoff 2 | upper bound (cutoff 1) | rep spread (cutoff 1) |
|---|---|---|---|---|---|
| 1 | — | 1.00 | 1.00 | 1.0 | 0.2% |
| 2 | 1.51 | 1.93 | 1.93 | 2.0 | 0.4% |
| 3 | 2.03 | 2.31 | 2.30 | 3.0 | 11.4% |
| 4 | 2.44 | **2.41** | 2.38 | 4.0 | 8.0% |
| 6 | 3.06 | 3.22 | 3.24 | 6.0 | 5.4% |
| 8 | 3.51 | 3.73 | 3.64 | 6.2 | 5.8% |
| 10 | 3.84 | 3.87 | 3.87 | 6.2 | 1.8% |
| 12 | 4.11 | **4.08** | 4.00 | 6.2 | 3.8% |
| 14 | 4.32 | **4.17** | 4.07 | 6.2 | 1.2% |
| 16 | 4.49 | **4.24** | 4.09 | 6.2 | 2.1% |

At 16 threads: 124.9 GFLOP/s with cutoff 1 (22.2% of the 16-core TPP) and
120.5 with cutoff 2. Bold marks results below the lower bound. Every thread
count is in the csv. The rows not shown follow the same pattern: above the
lower bound at 5, 7, 9 and 11 threads, below it at 13 (4.12 against 4.22)
and 15 (4.22 against 4.41).

#### Analysis

- **1 thread:** within 0.1% of DCQ. Supported.
- **The ceiling:** supported. The speedup flattens after about 8 threads
  (3.73× at 8, 4.24× at 16), far below Experiment 2's 15.22×.
- **The lower bound:** not quite. At every thread count from 12 up (and at
  4), cutoff 1 runs 1–6% below the greedy lower bound: 4.24× at 16 against
  4.49×. That is
  inside the hypothesis's 10% tolerance, but it shouldn't happen at all if
  the scheduler were greedy. The bound assumes no thread is ever idle while
  a task is ready to run.
- **Cutoff 2:** inside its own range at 16 threads (4.09× against
  3.6–4.6×).
- **The cutoff matters much less than predicted:** 4.24× against 4.09× at
  16 threads (3%). A span of 1,000 leaves against 1,350 predicts a gap of
  about 20–35%. Cutoff 1 behaves almost as if its span were already as long
  as cutoff 2's (Experiment 3b).
- **Rep-to-rep spread is large from 3 to 9 threads** (5–12%), unlike
  Experiment 2's final builds (under 6%).

### Experiment 3a: are both orderings necessary? (race controls)

- **Design:** two builds that each drop one ordering, run through the
  16-thread gate:
  - RACE=1: no `taskwait`, so LH and HL start while LL and HH may still be
    running;
  - RACE=2: `taskwait` kept, but LH and HL run as two tasks together.
- **Result:**

  | control | oracle failed | gold failed |
  |---|---|---|
  | RACE=1 | 5 / 42 | 2 / 14 |
  | RACE=2 | 5 / 42 | 2 / 14 |

  Every failure was at 20,000 × 20,000. RACE=2 corrupted more outputs than
  RACE=1 in 6 of the 7 failing cases (e.g. constants: 13,560 against 2,656 of
  40,001).
- **Conclusion:** both orderings are needed. RACE=2 corrupts more because LH
  and HL write exactly the same range, while RACE=1's overlaps are partial
  and only last while LL and HH are still running.
- **Limit of the check:** the gate exercises concurrency at only one size.
  Smaller sizes create too few tasks, and lopsided shapes run sequentially in
  DCP2.

### Experiment 3b: where do the threads spend their time?

- **Design:** TRACE build (never used for rates), 3 calls per process, call 2
  reported. Per thread: leaves, time in leaves, and time with a `taskwait`
  open (nested waits counted once), split into leaf time and the rest.
- **Result** (`results/DCP2/diagnostic/trace_*.err`):

  | threads | wall (s) | `single` thread: leaves | `single` thread: ms in `taskwait` (of which leaves) | other threads: ms in leaves |
  |---|---|---|---|---|
  | 2 | 0.354 | 3,286 | 342.1 (340.5) | 328.9 |
  | 8 | 0.187 | **98** | **175.2 (0.4)** | 76.5–107.5 |
  | 16 | 0.160 | **121** | **147.7 (2.8)** | 24.9–51.8 |

  The leaf time summed over all threads stays at about 680 ms at every
  thread count.
- **Conclusion:**
  - **The kernel doesn't slow down.** All of the loss is waiting: at
    16 threads a thread runs leaves for about 42 ms of a 160 ms call, so it
    waits about 73% of the time.
  - **The `single` thread is nearly idle** at 8 and 16 threads, almost the
    whole call inside a `taskwait`. It runs the root, creates LL and HH as
    tasks, and waits. Other threads take both tasks straight away.
  - **Hypothesis (not yet tested):** a thread waiting in `taskwait` may only
    run its own direct children in GCC's runtime, so it has nothing to do
    until LL and HH finish, and the same happens at every level. A scheduler
    like that isn't greedy. That would explain both the results below the
    greedy lower bound and why cutoff 1 acts as if its span were longer.
    Experiment 3c tests this.

### Experiment 3c: the parent runs HH itself (DCP2HD)

#### Question

Is the shortfall below the greedy lower bound caused by threads that wait in
a `taskwait` while their children run elsewhere? If each parent runs one
child itself instead of waiting for both, do those threads go back to work?

#### Hypothesis

In DCP2 a parent creates LL and HH as tasks and waits. Other threads take
both at once, and the parent can only run its own children while it waits
(Experiment 3b), so it sits idle through stage 1. Several threads can be
parked like this at the same time, so fewer than P threads do the work.

If the parent creates only LL as a task and runs HH itself, it works through
stage 1 and waits only if LL is still running when HH finishes. The ordering
(LL ∥ HH, then LH, then HL) is unchanged, so the work, the span and the
ceiling of about 6.2× stay the same. Only the idle time changes:

- **The `single` thread runs leaves:** at 8 and 16 threads, at least half as
  many as the average of the other threads (it ran 98 and 121 in DCP2).
- **Time waiting without running leaves drops:** summed over all threads at
  16 threads, by at least half against DCP2 (Experiment 3b's trace).
- **16 threads, cutoff 1:** 4.5–5.0×, back above the greedy lower bound
  (DCP2: 4.24×), and never above the 6.2× ceiling.
- **The cutoff matters more:** cutoff 2 falls further behind cutoff 1 than
  DCP2's 3%, toward the 20–35% the span difference predicts.
- **1 thread:** within 1% of DCQ.
- **Refuted if:** the `single` thread still runs under a quarter of the
  others' average, or 16 threads stay below 4.4× (no gain beyond the reps'
  roughly 2% spread). Either would mean the waiting parent is not what holds
  DCP2 below the bound.

#### Experiment Design

DCP2HD is DCP2 with one change at every node that splits both ranges:

    DCP2                           DCP2HD
    #pragma omp task  dc(LL)       #pragma omp task  dc(LL)
    #pragma omp task  dc(HH)       dc(HH)              // run directly
    #pragma omp taskwait           #pragma omp taskwait
    dc(LH)                         dc(LH)
    dc(HL)                         dc(HL)

Everything else is identical: leaf, setup, clamp, cutoff, top level (the
`single` thread calls the root directly, no `taskgroup`).

- **Checks:** gate at 1, 3 and 16 threads; `verify_timed` at 1, 3 and 16
  threads; a race control that removes the `taskwait` (LH starts while LL may
  still run), which must fail.
- **Trace:** 2, 8 and 16 threads, 3 calls per process, call 2 reported, same
  columns as Experiment 3b.
- **Sweep:** DCP2 and DCP2HD, cutoffs 1 and 2, alternating at each thread
  count, 1–16 threads, 5 reps, 99,999 × 99,999, L = 1,280. Speedup against
  DCQ's 0.6792 s.

#### Experiment Run

| check | result | file |
|---|---|---|
| DCP2's default binaries unchanged by the switch | identical disassembly and bytes | — |
| gate: `.check`, `.check-par` at 1, 3, 16 threads | 42/42 oracle, 14/14 gold at each | `results/DCP2/diagnostic/gate_hd_{seq,1,3,16}.txt` |
| `verify_timed` at 1, 3, 16 threads | 0 of 32 failed at each | `results/DCP2/diagnostic/verify_timed_hd.txt` |
| race control (no `taskwait`), 16 threads | fails: oracle 4/42, gold 2/14, all at 20,000 × 20,000 | `results/DCP2/diagnostic/race_hd1_gate.txt` |

The trace ran (`results/DCP2/diagnostic/trace_hd_*.err`, 3 calls per
process, call 2 reported). The sweep was not run (see Results).

#### Results

| 16 threads (trace build) | DCP2 | DCP2HD |
|---|---|---|
| wall time | 0.160 s | **0.401 s** |
| `single` thread: leaves | 121 | 1,380 |
| `single` thread: `taskwait`s | 79 | 444 |
| `single` thread: ms in `taskwait` (of which leaves) | 147.7 (2.8) | **260.7 (1.2)** |
| other threads: ms in leaves | 24.9–51.8 | 26.9–66.2 |
| other threads: ms in `taskwait` | 21–68 | 1.9–57 |

Wall time at the other thread counts: 0.681 s at 1 thread (DCP2 0.681),
0.526 s at 2 (0.354), 0.404 s at 8 (0.187). The leaf time summed over all
threads stays at about 680 ms, and setup at about 1 ms.

#### Analysis

- **Refuted as built:** DCP2HD is 2.5× slower than DCP2 at 16 threads, not
  faster. The `single` thread now runs the most leaves, but spends 261 ms in
  `taskwait` with almost no leaves inside it. The other threads are mostly
  idle *outside* `taskwait`: with 680 ms of leaves in a 401 ms call, only
  about 1.7 threads run leaves at a time on average.
- **Worked out afterwards (not yet tested):** a `taskwait` waits for every
  child task of the *task it is in*, not only for the tasks created at the
  same level of the recursion. In DCP2HD, HH runs directly inside the
  parent's task, so every `taskwait` inside HH's recursion belongs to that
  same task. The innermost one waits for the parent's LL, which at the root
  is a quarter of the whole problem, before HH can go on to its own LH and
  HL:

      parent task:
        create LL            ← child 1: running on another thread
        HH, run directly:
           create LL'        ← child 2
           HH', run directly:
              ...
              taskwait       ← waits for LL'' and also LL' and the outer LL

  This is still correct (waiting longer never races, so the gate passes), but
  it serializes the HH chain, and the chain stops creating new tasks for the
  other threads. DCP2 doesn't have this problem, even though LH and HL run
  directly there too: by the time LH runs, the node's `taskwait` has already
  finished all of the node's children.
- **So DCP2HD does not test this experiment's hypothesis.** It changed two
  things at once: the parent runs HH (intended), and HH's `taskwait`s wait on
  unrelated work (not intended).
- **Next (pending):** DCP2HD2 runs HH as an *undeferred* task,
  `#pragma omp task if(0)`. The current thread runs it immediately, as with a
  direct call, but it is a separate task, so its `taskwait`s wait only for
  HH's own children. The hypothesis above then applies unchanged. If the
  `taskwait` explanation is right, DCP2HD2 should also run at least as fast
  as DCP2 at 16 threads (0.160 s), not 2.5× slower.

### Experiment 3d (pending): best leaf size at 16 threads

- **Question:** which leaf size gives the best speedup? This is the "best
  speedup you can get" part of goal 2.
- **Hypothesis:** a smaller leaf adds parallelism, since W/S grows like
  (n/L)^0.415, but the kernel runs slower (PA3 Experiment 7). The best leaf
  size at 16 threads should be below 1,280, around 256–512 (at least about
  5.4–5.6× against DCQ at 1,280).
- **Design:** after 3c, on whichever of DCP2 and DCP2HD2 is faster.
