****## Starting Point
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
- **Worked out afterwards: the span estimate was too short.** S ≈ 79^1.585
  assumes every split halves the problem exactly. DCQ's split tree doesn't:
  79 blocks split 39/40, and near the bottom the uneven splits leave 1 × 2
  and 2 × 1 pieces, which DCP2 runs in sequence (one-sided splits). Computing
  work and span on the actual split tree, with the same split rule and the
  thin edge leaves, in units of one full leaf (script `span.py`):

  | | work | span | W / S | bounds at 4 / 8 / 16 threads |
  |---|---|---|---|---|
  | hypothesis, cutoff 1 | 6,241 | 1,000 | 6.2 | 2.44–4.0 / 3.51–6.2 / 4.49–6.2 |
  | **exact, cutoff 1** | 6,104 | **1,334** | **4.58** | 2.13–4.0 / 2.91–4.58 / **3.56–4.58** |
  | exact, cutoff 2 | 6,104 | 1,412 | 4.32 | 2.08–4.0 / 2.81–4.32 / 3.40–4.32 |

  With the exact span, every measurement is inside its bounds, including
  4.24× at 16 threads. The cutoff difference is also explained: the exact
  spans differ by 6%, not 35%, and the measured gap is 3–4%. So the
  "below the lower bound" results above, and the scheduler explanation
  proposed for them in 3b, came from the span estimate, not from the
  scheduler.

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
  - **Hypothesis at the time:** a thread waiting in `taskwait` may only
    run its own direct children in GCC's runtime, so it has nothing to do
    until LL and HH finish, and the same happens at every level. A scheduler
    like that isn't greedy. That would explain both the results below the
    greedy lower bound and why cutoff 1 acts as if its span were longer.
    Experiment 3c tests this.
  - **Revisited after 3c:** the exact span (Experiment 3, Analysis) already
    accounts for both observations. Waiting parents do cost speedup, but
    mostly at low thread counts (Experiment 3c).

### Experiment 3c: the parent runs HH itself (DCP2HD, DCP2HD2)

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
- **Worked out afterwards (confirmed by DCP2HD2 below):** a `taskwait` waits for every
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
- **Next:** DCP2HD2 runs HH as an *undeferred* task,
  `#pragma omp task if(0)`. The current thread runs it immediately, as with a
  direct call, but it is a separate task, so its `taskwait`s wait only for
  HH's own children. The hypothesis above then applies unchanged. If the
  `taskwait` explanation is right, DCP2HD2 should also run at least as fast
  as DCP2 at 16 threads (0.160 s), not 2.5× slower.

#### DCP2HD2: Experiment Design

    #pragma omp task          dc(LL)
    #pragma omp task if(0)    dc(HH)    // runs now, on this thread, as its own task
    #pragma omp taskwait
    dc(LH)
    dc(HL)

Same checks, trace and sweep as planned for DCP2HD above.

#### DCP2HD2: Experiment Run

| check | result | file |
|---|---|---|
| DCP2 and DCP2HD binaries unchanged by the new switch value | byte-identical | — |
| gate: `.check`, `.check-par` at 1, 3, 16 threads | 42/42 oracle, 14/14 gold at each | `results/DCP2/diagnostic/gate_hd2_{seq,1,3,16}.txt` |
| `verify_timed` at 1, 3, 16 threads | 0 of 32 failed at each | `results/DCP2/diagnostic/verify_timed_hd2.txt` |
| race control (no `taskwait`), 16 threads | fails: oracle 4/42, gold 2/14, all at 20,000 × 20,000 | `results/DCP2/diagnostic/race_hd2_1_gate.txt` |

- **Trace:** `results/DCP2/diagnostic/trace_hd2_*.err`, 3 calls per process,
  call 2 reported.
- **Sweep:** on perch, an identical machine (i9-13900K, E-cores 16–31;
  `results/DCP2/diagnostic/machine_20261030_0925.txt`). Before starting and
  before each thread count, an idle check: CPUs 16–31 at most 3.1% busy, no
  other user's process above 2.0% CPU (`idle_20261030_0925.log`). Every rep
  is in `results/DCP2/sweep_raw_hd2_20261030_0925_perch.csv`.
- **A first sweep on the original machine was discarded.** Another user's
  process was running at about 530% CPU, and spreads reached 86%. It is kept
  in `results/DCP2/diagnostic/` and not used.

#### DCP2HD2: Results

Trace, 16 threads:

| | DCP2 | DCP2HD | **DCP2HD2** |
|---|---|---|---|
| wall time | 0.159 s | 0.400 s | **0.153 s** |
| `single` thread: leaves | 121 | 1,380 | 1,388 |
| `single` thread: `taskwait`s | 79 | 444 | 444 |
| `single` thread: ms in `taskwait` (of which leaves) | 147.7 (2.8) | 260.7 (1.2) | **13.4 (2.1)** |
| other threads: average leaves | 408 | 324 | 324 |
| all threads: ms in `taskwait` without leaves | 503.1 | 413.8 | **20.4** |
| all threads: ms in leaves | 680.8 | 681.1 | 680 |

At 8 threads: 0.165 s against DCP2's 0.186. The `single` thread ran 1,469
leaves, against an average of 682 for the others, and waiting without leaves
summed to 30.0 ms (DCP2: 398.0).

Sweep (perch), speedup against DCQ's 0.6792 s:

| threads | DCP2 c1 | **DCP2HD2 c1** | DCP2 c2 | DCP2HD2 c2 | exact bounds, cutoff 1 | largest spread |
|---|---|---|---|---|---|---|
| 1 | 0.99 | 0.99 | 0.99 | 1.00 | — | 0.2% |
| 2 | 1.93 | 1.93 | 1.93 | 1.93 | 1.4–2.0 | 0.3% |
| 3 | 2.31 | 2.31 | 2.23 | 2.32 | — | 11.4% (DCP2 c1) |
| 4 | 2.41 | **3.15** | 2.39 | 3.11 | 2.13–4.0 | 8.7% (HD2 c1) |
| 6 | 3.37 | **3.68** | 3.22 | 3.63 | — | 8.2% |
| 8 | 3.69 | **4.06** | 3.63 | 3.98 | 2.91–4.58 | 5.9% |
| 10 | 3.95 | **4.27** | 3.88 | 4.09 | — | 4.0% |
| 12 | 4.09 | **4.32** | 3.98 | 4.16 | — | 4.4% |
| 14 | 4.18 | **4.37** | 4.07 | 4.20 | — | 2.1% |
| 16 | 4.24 | **4.40** | 4.08 | 4.21 | 3.56–4.58 | 4.5% |

At 16 threads DCP2HD2 runs at 129.6 GFLOP/s with cutoff 1. DCP2 on perch
matches the earlier sweep (4.24× at 16 threads both times). The largest
spreads are mostly in DCP2's cells; DCP2HD2's stay within about 3% except
at 4 threads. Every thread count is in the csv.

#### DCP2HD2: Analysis

- **The `taskwait` explanation for DCP2HD holds.** DCP2HD2 differs from
  DCP2HD only in running HH as its own task, with the same 444 `taskwait`s
  and about the same leaves on the `single` thread. It takes 0.153 s at
  16 threads, against DCP2HD's 0.400.
- **Against this experiment's hypothesis:**

  | prediction | measured | |
  |---|---|---|
  | `single` thread runs at least half the others' average | 1,388 against 324 (16 threads) | supported |
  | waiting without leaves at least halved (16 threads) | 503 → 20 ms | supported |
  | 16 threads, cutoff 1: 4.5–5.0× | 4.40× | not reached; exactly at the refutation threshold |
  | the cutoff matters more, toward 20–35% | 4.5% (4.40 against 4.21) | refuted |

- **Running HH directly helps most at low thread counts:** +31% at 4 threads
  (3.15× against 2.41×), +10% at 8, +4% at 16. With 4 threads, one parked
  parent is a quarter of the machine. At 16 threads both versions are close
  to their span.
- **Worked out afterwards: DCP2HD2 is span-bound.** One full leaf takes
  680 ms / 6,104 = 0.111 ms, so the exact span of 1,334 leaves is 148.6 ms.
  DCP2HD2 takes 154.3 ms at 16 threads, within 4% of it. With cutoff 2:
  157.3 ms predicted, 161.3 ms measured. DCP2 is 7–8% above its span. The
  waiting time that disappeared was mostly not on the critical path, which
  is why a 25-fold drop in waiting gave only a 4% gain at 16 threads.
- **Conclusion:** the waiting parent is a real cost, but only a large one at
  low thread counts. At 16 threads the limit is the span, as the work-and-span
  analysis says once the span is computed on the actual split tree. The
  remaining lever is the span itself, i.e. the leaf size (Future work,
  below).

### Experiment 3: conclusions

- **The best without a reduction is DCP2HD2:** 4.40× at 16 threads
  (129.6 GFLOP/s, 23.0% of the 16-core TPP), with cutoff 1 and L = 1,280.
  Leaf size was not scanned (Future work, below).
- **Work and span predict the result, once the span is exact.** Avoiding
  races without a reduction forces an ordering: LL ∥ HH, then LH, then HL. The
  span then grows like n^1.585. On the actual split tree it is 1,334 leaves,
  which limits the speedup to W/S = 4.58× at L = 1,280. DCP2HD2 runs within
  4% of that span, and every DCP2 and DCP2HD2 result is inside its bounds.
- **The span estimate must use the actual split tree.** The estimate from
  the recurrence (79^1.585 ≈ 1,000) assumed every split was even. It was 25%
  too short, and made the results look as if they were below the greedy lower
  bound. That led to a scheduler explanation (3b) which the exact span made
  unnecessary.
- **Both orderings are needed** (3a). Dropping either one makes the gate
  fail.
- **The kernel doesn't slow down; all of the loss is waiting** (3b). The
  leaf time summed over all threads stays at about 680 ms at every thread
  count. At 16 threads, each thread waits about 73% of the time.
- **Which thread waits matters most at low thread counts** (3c). A parent
  parked in `taskwait` costs a quarter of the machine at 4 threads: DCP2HD2
  is 31% faster than DCP2 there, but only 4% faster at 16 threads, where both
  are near their span.
- **A `taskwait` waits for every child of the current task**, not only those
  created at the same level. Running a child as a direct call (DCP2HD) made
  its `taskwait`s wait for unrelated work, and the result was 2.5× slower.
  Running it as an undeferred task (`task if(0)`, DCP2HD2) avoids this.
- **The cutoff barely matters:** 3–4%, matching the 6% difference between
  the exact spans of cutoffs 1 and 2.
- **Against Experiment 2:** the reduction gives 15.22× by spending one copy
  of `c` per thread (12.8 MB at 16 threads). Sharing `c` needs no extra
  memory, but the ordering limits the speedup to about 4.4× at this leaf
  size. The trade is memory against parallelism.
- **Open:**
  - the leaf size, the remaining lever on the span: predicted at about
    7–8× at L = 256–384, not measured (Future work, below);
  - large rep-to-rep spreads (5–12%) at 3–9 threads, not explained.

### Future work: leaf size for the taskwait version

Not run. At 16 threads DCP2HD2 is limited by its span, and the span depends
on the leaf size, so a smaller leaf is the remaining way to raise goal 2's
speedup.

- **Prediction:** W/S grows like (n/L)^0.415 as L shrinks, but the kernel
  runs slower at small L (PA3 Experiment 7). The exact span on the actual
  split tree (`span_L.py`, cutoff 1) gives these ceilings at 16 threads. The
  last column scales W/S by PA3's one-core rate at that leaf size against
  1,280:

  | L | blocks per side | W / S | ceiling × DCQ at 1,280 |
  |---|---|---|---|
  | 256 | 391 | 9.5 | 7.9 |
  | 384 | 261 | 8.8 | 8.0 |
  | 392 | **256** | 9.9 | (no PA3 rate) |
  | 512 | 196 | 7.1 | 6.7 |
  | 768 | 131 | 6.7 | 6.4 |
  | 784 | **128** | 7.4 | (no PA3 rate) |
  | 1,024 | 98 | 5.4 | 5.3 |
  | 1,280 | 79 | 4.6 | 4.6 |

  - The best leaf size should be about 256–384. If DCP2HD2 again reaches
    about 96% of its ceiling, that is about 7–8×, nearly double the result
    at 1,280, but still about half of Goal 3's 13.80×.
  - Block counts that are powers of two give a shorter span than their
    neighbours, since every split is even: 128 blocks (L = 784) at W/S 7.4
    against 131 blocks (L = 768) at 6.7.
- **Caveats:** at L = 256 with cutoff 1 there are about 150,000 leaf tasks,
  so task overhead may show, and cutoff 2 should do better. Experiment 4a
  also showed that a leaf's speed depends on per-row-group costs (heads,
  tails, visits), which grow as the leaf shrinks, so the real ceilings may
  be lower.
- **Design, if run:** DCP2HD2 at 16 threads; L = 256, 384, 392, 512, 768,
  784, 1,024 and 1,280; cutoffs 1 and 2; 5 reps each; idle check; DCQ rerun
  on the same machine for the speedups.

## Part C: Another strategy without a reduction

### Experiment 4: output-range recursion (Goal 3)

#### Question

Experiments 2 and 3 both divide the inputs (`a` × `b`), so subproblems
overlap in `c`: either each thread gets its own copy (a reduction), or the
overlapping subproblems wait for each other (`taskwait`). If we divide the
output instead, so that every task owns its own range of `c`, can we get
Experiment 2's speedup with no reduction, no extra copy of `c`, and no
ordering?

#### Hypothesis

Written before any DCP3 run.

Each task owns a contiguous range of outputs [k0, k1) and computes every
pair (i, j) with k0 ≤ i + j < k1. In the (i, j) rectangle, that range is a
band along the anti-diagonal. Two tasks never write the same element of
`c`, so no task waits for another; the only sync is the end of the parallel
region.

    c[0 .. 2n−1)                      split each range where it halves the
       /        \                     *work* (pairs), not the width
    [0, m)     [m, 2n−1)
     / \         / \
    ... ...     ... ...               stop at 128 pieces (depth 7)
     |   |       |   |
    piece piece piece piece           each piece: rows in blocks, a
                                      parallelogram of pairs per block

- **Work:** the same 99,999² ≈ 10¹⁰ multiply-adds. Each row group computes
  only the outputs in its piece where it has valid pairs, so the extra
  (zero-padded) FMAs are about the same as BKQ's.
- **Span:** one piece, 1/128 of the work (5.3 ms of DCQ's 679 ms), plus 7
  levels of splitting. W / S = 128.
- **Pieces are equal in work,** so they should be close to equal in time.
  For equal, independent pieces the greedy bound (14.2× at 16 threads) is
  too pessimistic. The exact ceiling is the number of pieces divided by the
  most pieces any one thread must run: pieces / ⌈pieces / P⌉.

  | threads | ceiling, 128 pieces | ceiling, 256 pieces | T3NG (Experiment 2) |
  |---|---|---|---|
  | 4 | 4.00 | 4.00 | 3.98 |
  | 6 | 5.82 | 5.95 | 5.93 |
  | 8 | 8.00 | 8.00 | 7.87 |
  | 10 | 9.85 | 9.85 | 9.77 |
  | 12 | 11.64 | 11.64 | 11.56 |
  | 14 | **12.80** | 13.47 | 13.48 |
  | 16 | 16.00 | 16.00 | 15.22 |

- **1 thread:** within 2% of DCQ. The inner loop is BKQ's padded kernel;
  only the loop bounds change (a parallelogram instead of a square).
- **16 threads, 128 pieces:** 15.4–15.8×, above T3NG's 15.22×. T3NG's 2.2 ms
  over linear included zeroing the private copies (0.8–0.9 ms) and the merge
  (0.2 ms), which this strategy doesn't have. What remains is setup
  (0.3–0.9 ms) and end-of-run imbalance (about 0.5 ms): 43.3–44.0 ms against
  42.5 ms for linear.
- **A staircase at thread counts that don't divide 128,** most visible at
  14 threads (at most 12.8×), and smaller with 256 pieces.
- **Memory:** no copy of `c`; only the padded copies of `b` that DCQ already
  makes, independent of the thread count.
- **Refuted if:**
  - the one-thread time is more than 2% slower than DCQ (the new leaf bounds
    cost speed, which then caps every count);
  - with the one-thread time within 2%, 16 threads fall below T3NG's 15.22×
    (an overhead the model leaves out);
  - 14 threads with 128 pieces exceed 12.8× by more than the reps' spread
    (the pieces are not equal in time).

#### Experiment Design

DCP3 is a new variant with the same padded leaf kernel (R = 8, U = 3, four
shifted copies of `b`), no reduction, no `taskwait` and no `taskgroup`:

    #pragma omp parallel
    #pragma omp single
        rec(0, 2n−1, 7)               // called directly; 2^7 = 128 pieces

    rec(k0, k1, depth):
        if depth == 0:  piece(k0, k1)
        else:
            m = point where pairs(k0, m) = pairs(k0, k1) / 2
            #pragma omp task  rec(k0, m, depth − 1)
            #pragma omp task  rec(m, k1, depth − 1)

- **pairs(k0, k1)** is the number of (i, j) with k0 ≤ i + j < k1, computed in
  closed form from the triangle shape.
- **Recursion by depth,** not by a threshold on pairs: halves are only
  nearly equal, so a threshold would split some halves again into unequal
  pieces.
- **piece(k0, k1):** in chunks of W outputs (default 1,280), so the chunk of
  `c` stays in L1. For each chunk, every contributing row group computes
  only the outputs in the chunk where it has valid pairs. Pieces near the
  ends of `c` are up to about 12,500 outputs wide, so without chunks their
  `c` window (about 50 KB) would not fit in L1.
- **Pieces:** a run-time option, a power of two, default 128.
- **Checks:** gate at 1, 3 and 16 threads; `verify_timed` at 1, 3 and
  16 threads; a race control that splits each piece's rows into two tasks.
  Every pair is still computed once, so it must pass at 1 thread and fail
  at 16.
- **Run:** first DCP3 at 1 thread against DCQ, back to back, with W = 1,280
  and with chunking off. Then 1–16
  threads, 128 and 256 pieces alternating at each thread count, 5 reps,
  99,999 × 99,999, with the idle check. Speedup against DCQ on the same
  machine.

#### Experiment Run: first version

These runs are of the first version of DCP3: splits at any output, so
pieces start at any alignment. Experiments 4a and 4b change the split rule;
the final sweep is in "Experiment 4: results".

| check | result | file |
|---|---|---|
| gate: `.check`, `.check-par` at 1, 3, 16 threads | 42/42 oracle, 14/14 gold | `results/DCP3/diagnostic/gate_*.txt` |
| trace, 2 / 8 / 16 threads, call 2 of 3 | 128 pieces in every call, pairs total = na × nb | `results/DCP3/diagnostic/trace_{2,8,16}.err` |

- **Piece sizes:** 77,993,847 to 78,260,647 pairs (0.34% apart). A split
  must fall on a whole output, and one output in the middle of `c` is about
  100,000 pairs (0.13% of a piece), so this is the rounding of 7 levels of
  splits. It costs about 0.2 ms at 16 threads.
- **Trace timings** (trace build, not idle-checked, load average 4.8; not
  used for rates):

  | threads | wall | sum of piece time | pieces per thread | piece time per thread |
  |---|---|---|---|---|
  | 2 | 0.374 s | 742 ms | 64, 64 | 370–372 ms |
  | 8 | 0.099 s | 759 ms | 16 each | 93–98 ms |
  | 16 | 0.054 s | 789 ms | 7–9 | 46.5–52.7 ms |

  The summed piece time is 9–16% above DCQ's 680 ms, and at 16 threads the
  pieces don't take equal time, so threads ran 7 to 9 pieces each.

- **One core** (barracuda, identical to the other two machines; idle checks
  and gate passed
  before each run; DCQ run before each DCP3 configuration; 5 reps each;
  `results/DCP3/onecore_20261031_1701.csv`):

  | configuration | median s | GFLOP/s | against DCQ | spread |
  |---|---|---|---|---|
  | DCQ (4 runs) | 0.6788–0.6790 | 29.46 | — | ≤ 0.11% |
  | DCP3, W = 1,280 | 0.7010 | 28.53 | **+3.2%** | 0.21% |
  | DCP3, W = 640 | 0.7429 | 26.92 | +9.4% | 0.13% |
  | DCP3, W = 320 | 0.8097 | 24.70 | +19.3% | 0.18% |
  | DCP3, chunking off | 0.7377 | 27.11 | +8.6% | 0.32% |

- **The one-thread prediction is refuted:** the best DCP3 is 3.2% slower
  than DCQ, against the 2% limit. At that rate, even a perfect 16 on
  16 threads gives 15.5× against DCQ, and the predicted overheads bring it
  to about 15.0×, below T3NG.
- **Smaller chunks are slower, and no chunking is slower too.** The cost
  grows roughly like 1/W (+3.2, +9.4, +19.3%), which points to a fixed cost
  for each (row group, chunk) pair rather than to cache misses. Without
  chunks, the wide pieces near the ends of `c` don't fit in L1. The best W
  is probably above 1,280.

### Experiment 4a: making DCP3's leaf as fast as DCQ's

- **Diagnosis (from the code and a model of the loop bounds, no runs;
  `DCP3/tools/visits.py`):** at W = 1,280, DCP3 makes 1.38M (row group, chunk)
  visits against DCQ's 0.99M (L = 1,280), and 1.0M of them pay a scalar head
  and a masked tail; DCQ pays no heads. The heads come from piece
  boundaries: 96 of the 128 pieces start at an output that isn't a multiple
  of 4, and every chunk of such a piece inherits that alignment.
  - A fit to the three chunked one-core points above, about 27 ns per extra visit and
    11 ns per head, splits W = 1,280's 22 ms gap about evenly between the
    two. It is two parameters fit to three points, so it is only a guide.
  - Chunking off has the fewest visits but is 59 ms slower; the fit
    explains only about 12 ms of that, so the rest is L1 misses on the wide
    end pieces.
  - A global chunk grid (multiples of W from output 0) would not help: the
    model gives 49% more visits at W = 1,280 and the same heads as chunking
    off.
- **Hypothesis:** rounding every split point to a multiple of 8 makes every
  piece and every chunk start aligned. This removes all heads and masked
  tails, at the cost of less equal pieces (1.5% apart in pairs instead of
  0.34%).
  - W = 1,280: about 1.6% over DCQ (11 ms), from the extra visits only.
  - Larger W cuts visits in the wide pieces (1.17M at 1,920 down to 1.09M at
    5,120), so the best W should be above 1,280, until the c and b windows
    overflow L1.
  - The middle pieces (about 780 outputs wide at 128 pieces) are one chunk
    whatever W is, so their visits stay. 256 pieces halve that width, so
    they should give a slower leaf: the 128 against 256 comparison trades
    balance against leaf rate.
  - **Model predictions** (fit, aligned, 128 pieces): 0.6896 s at
    W = 1,280, 0.6840 at 1,920, 0.6825 at 2,560, 0.6819 at 3,840, 0.6817 at
    5,120 (DCQ 0.6790). These leave out cache effects, which should show up
    at the larger W.
  - **256 pieces** (model): 392–8,848 outputs wide, 2.9% apart in pairs, and
    every middle piece narrower than any W, so about +4.7–4.9% over DCQ at
    every W, slower than the unaligned 128-piece leaf.
  - **Refuted if** aligned W = 1,280 is not at least 1% faster than the
    unaligned 0.7010 s.
- **Design:** split points rounded to the nearest multiple of 8 (clamped to
  the range); chunks still k0 + t·W. One core, back to back with DCQ:
  W = 1,280, 1,920, 2,560, 3,840 and 5,120, 5 reps each, idle check and gate
  before each.
- **Run:** barracuda, stamp 20261031_1734; idle check and gate passed before
  every run (11 and 10); DCQ before each W; 5 reps each
  (`results/DCP3/onecore_aligned_20261031_1734.csv`).
- **Results:**

  | W | DCQ just before (s) | aligned DCP3 (s) | GFLOP/s | against DCQ | model predicted | spread |
  |---|---|---|---|---|---|---|
  | 1,280 | 0.6791 | **0.6701** | **29.85** | **−1.3%** | 0.6896 (+1.6%) | 0.12% |
  | 1,920 | 0.6789 | 0.6930 | 28.86 | +2.1% | 0.6840 (+0.7%) | 0.14% |
  | 2,560 | 0.6788 | 0.7013 | 28.52 | +3.3% | 0.6825 (+0.5%) | 0.12% |
  | 3,840 | 0.6790 | 0.7025 | 28.47 | +3.5% | 0.6819 (+0.4%) | 0.13% |
  | 5,120 | 0.6788 | 0.7085 | 28.23 | +4.4% | 0.6817 (+0.4%) | 0.28% |

- **Analysis:**
  - **Supported:** aligned W = 1,280 is 4.4% faster than the unaligned
    version (0.7010 s), well past the 1% threshold.
  - **Better than predicted:** DCP3's leaf is now 1.3% *faster* than DCQ's
    (84.8% of the one-core TPP, against 83.7%). DCQ pays a 7-wide masked tail
    on every row group (each covers 1,287 outputs), and aligned DCP3 pays
    none.
  - **Larger W is slower, the opposite of the model:** the model has no
    cache term. At W = 1,280 a chunk's working set is about 25 KB (5 KB of
    `c`, plus a 1,280-float window of each of the 4 copies of `b`), inside
    the E-core's 32 KB L1. At 1,920 it is about 37.5 KB, past it. The loss
    from L1 misses outweighs the visits saved.
  - **The one-thread prediction of Experiment 4 now holds:** DCP3 is within
    2% of DCQ (1.3% faster), with W = 1,280 and aligned splits. Experiment
    4b and the final sweep build on this version.
- **Trace at 16 threads, aligned, W = 1,280** (idle check passed; call 2 of
  3; `results/DCP3/diagnostic/trace_al_16.err`): 51.4 ms per call (about
  13.2× against DCQ), the same in all 3 calls. Pieces per thread were 7–9 in
  call 2 and exactly 8 in calls 1 and 3, with no change in the total, so the
  uneven count isn't what sets the time. **Summed piece time is 780 ms, 16%
  above aligned DCP3's one-core 670 ms**, the same 16% as the unaligned trace
  on a loaded machine. The leaf itself is fast on one core and slow when
  16 cores run it together.

### Experiment 4b: false sharing at piece boundaries

- **Question:** why does DCP3's piece time rise 16% at 16 threads (and about
  9% at 2), when T3NG's leaf time didn't rise at all?
- **Hypothesis:** neighbouring pieces share a cache line of `c`. Split points
  are multiples of 8 floats, but a cache line is 16 floats (64 B), so
  boundaries can fall inside a line. (Checked afterwards: the harness's
  `malloc` puts `c` 16 B past a line, so all 127 boundaries are inside a
  line.) Sibling pieces are created together and
  usually run at the same time on different cores. Every row group
  read-modify-writes the outputs at both ends of its chunk, so the boundary
  line moves between the two cores once per row group.
  - **Size:** a middle piece is one chunk with all 12,500 row groups. At
    about 150 cycles per contended line transfer, 2 boundaries × 12,500 ×
    150 cycles ≈ 0.9 ms per 5.2 ms piece, about 17%, matching the 16%.
  - **Fix:** round each split point to the nearest m with &c[m] on a 64-byte
    boundary, using `c`'s actual address (m ≡ 12 mod 16 here), so no two
    pieces share a line. Model: pieces 2.25% apart in pairs, no boundary
    inside a line, no heads; masked tails only in the first and last piece.
  - **Side effect:** this also moves most of `c`'s 32-byte vector accesses
    from straddling a line to starting on one, which may change the one-core
    time. So the 16-thread summed piece time is compared with the new
    one-core time, not with 0.670 s.
  - **Prediction:** one-core time unchanged (0.670 s, no other core to share
    with); summed piece time at 16 threads back to about 670–690 ms; about
    43–44 ms per call, about 15.4–15.8× against DCQ. Pieces up to about 3%
    apart in pairs.
  - **Refuted if:** summed piece time at 16 threads stays above 740 ms (the
    lines were not the cause).
- **Run:** splits rounded to &c[m] on a 64-byte line; gate, `verify_timed`
  and race control passed (`results/DCP3/diagnostic/gate_line_*.txt`,
  `verify_timed_line.txt`, `race1_line_gate_{seq,1,16}.txt`). One core, back
  to back with DCQ (barracuda, `results/DCP3/onecore_line_20261031_2112.csv`).
  Trace at 16 threads, 3 calls, idle check first
  (`results/DCP3/diagnostic/trace_line_16.err`).
- **Results:**

  | | multiple of 8 | line-aligned |
  |---|---|---|
  | one core | 0.6701 s | 0.6705 s (DCQ 0.6789, −1.2%) |
  | 16 threads: summed piece time (calls 1–3) | 780 ms (call 2) | 743 / 735 / 738 ms |
  | above the one-core time | +16% | **+9.6–10.9%** |
  | piece time per thread (call 2) | 45.5–50.3 ms | 43.5–47.1 ms |
  | pieces per thread | 7–9 | 8 each, all calls |
  | total call (trace build) | 51.4 ms | 48.2–50.7 ms |
  | piece spread (pairs) | 1.54% | 2.25% |

- **Analysis:**
  - **The side effect didn't appear:** the one-core time moved 0.06%,
    inside the spread.
  - **False sharing was real but explains only about 40% of the excess:**
    summed piece time dropped by about 45 ms, but is still about 65 ms
    (+10%) above one core. The prediction (670–690 ms) is not reached.
  - **Refutation line:** 735–743 ms against 740, so it sits at the line,
    inside call-to-call variation. Neither clearly supported nor refuted:
    the boundaries were one cause, not the only one.
  - **What's left is not explained.** The trace records per-thread totals
    only, so it can't say whether some pieces (e.g. the narrow middle ones,
    which stream all of `a` and `b`) slow down more than others, or whether
    cores sharing an L2 cluster slow each other.
  - About 48–51 ms per call in the trace build is about 13.4–14.1× against
    DCQ, so Experiment 4's 15.4–15.8× prediction is unlikely to hold.

### Experiment 4: results

DCP3 with line-aligned splits and W = 1,280. Barracuda, idle check before
each thread count (17, all passed), gate before each (16, all passed), 5 reps
each, every rep in `results/DCP3/sweep_raw_20261031_2116.csv`. Speedup
against DCQ's 0.6789 s on the same machine
(`onecore_line_20261031_2112.csv`). Efficiency is against DCP3's own
1-thread time.

| threads | 128 pieces: s | GFLOP/s | **speedup** | efficiency | spread | 256 pieces: speedup |
|---|---|---|---|---|---|---|
| 1 | 0.6703 | 29.8 | 1.01 | — | 0.1% | 1.00 |
| 2 | 0.3490 | 57.3 | 1.95 | 96% | 8.1% | 1.87 |
| 3 | 0.2424 | 82.5 | 2.80 | 92% | 2.5% | 2.66 |
| 4 | 0.1874 | 106.7 | 3.62 | 89% | 3.7% | 3.40 |
| 6 | 0.1239 | 161.4 | 5.48 | 90% | 3.3% | 4.95 |
| 8 | 0.0935 | 213.9 | 7.26 | 90% | 3.0% | 6.48 |
| 10 | 0.0752 | 265.9 | 9.03 | 89% | 5.4% | 7.68 |
| 12 | 0.0652 | 306.7 | 10.41 | 86% | 4.3% | 9.45 |
| 14 | 0.0566 | 353.5 | 12.00 | 85% | 3.4% | 11.09 |
| 16 | 0.0492 | **406.6** | **13.80** | 85% | 4.8% | 12.48 |

At 16 threads: 406.6 GFLOP/s, 72.2% of the 16-core TPP (563.2). Every thread
count is in the csv. 256 pieces has spreads of 10% or more at 2, 8, 10 and
11 threads (largest 16.6%); 128 pieces' largest is 9.6% (15 threads).

### Experiment 4: analysis

| prediction | measured | |
|---|---|---|
| 1 thread within 2% of DCQ | 1.3% faster (after 4a) | supported |
| 16 threads, 128 pieces: 15.4–15.8×, above T3NG | 13.80× | **refuted** |
| a staircase, at most 12.8× at 14 threads (128 pieces) | 12.00×, no visible step | the bound holds, the step isn't visible |
| 256 pieces smaller staircase | slower at every count | other effects dominate |

- **Where the 16-thread time goes** (49.2 ms against 41.9 ms for 670.3 ms
  / 16, from the 4b trace): setup about 1 ms (the four copies of `b`, built
  serially); piece slowdown about 4 ms (summed piece time +10% at
  16 threads); end-of-run imbalance about 2 ms (per-thread piece time
  43.5–47.1 ms).
- **The loss appears by 4 threads, then stays flat:** efficiency falls from
  96% at 2 threads to 89% at 4, stays at about 90% to 10 threads, and then
  drops slowly to 85% at 16. If `run_sweep.sh` binds threads compactly
  (16–19 first, which is one E-core cluster sharing an L2), this is the
  pattern of contention inside a cluster: it grows until the first cluster
  is full, then each added cluster brings its own L2. This is not verified.
- **The staircase isn't visible** because the pieces don't take equal time:
  piece slowdown and imbalance are larger than the steps.
- **256 pieces is slower everywhere:** 1.5% on one core (narrower middle
  pieces mean more row-group visits, as predicted for 4a, though by less
  than the model's 4.9%), and 6–15% from 4 threads up, more than the leaf
  alone explains. Its reps also vary much more.

### Experiment 4: conclusions

- **Goal 3 is met by DCP3, output-range recursion:** 13.80× at 16 threads
  (406.6 GFLOP/s, 72.2% of TPP), with no reduction, no `taskwait`, no copy of
  `c`, and only the parallel region's barrier.
  - It is 3.1× faster than the best `taskwait` version (DCP2HD2, 4.40×).
  - It is 9% slower than the reduction (T3NG, 15.22×), which uses one copy of
    `c` per thread.
- **The leaf matters as much as the decomposition** (4a): the first DCP3
  leaf was 3.2% slower than DCQ's. Aligning split points removed every scalar
  head and masked tail, and the leaf became 1.3% faster than DCQ's. The
  visit-count model got the direction right at small W but missed the L1
  limit above W = 1,280.
- **Owning disjoint outputs is not the same as owning disjoint cache lines**
  (4b): the harness puts `c` 16 B past a line, so every split shared a line
  with its neighbour. Aligning splits to lines cut the 16-thread piece
  slowdown from 16% to 10%.
- **More pieces is not better here:** 256 pieces was slower at every thread
  count. Narrower pieces cost more in the leaf than they gain in balance.
- **Open:**
  - the remaining 10% piece slowdown at 16 threads; a possible cause is
    shared L2 contention within an E-core cluster (the efficiency pattern
    above), testable by comparing 4 threads in one cluster against 4 spread
    across clusters;
  - building the copies of `b` in parallel would remove about 1 ms (2%) at
    16 threads;
  - 256 pieces' large rep spreads are not explained.

## Summary: three ways to parallelize DCQ

| strategy | variant | best at 16 threads | GFLOP/s | extra memory | sync | what limits it |
|---|---|---|---|---|---|---|
| reduction (Goal 1) | T3NG | **15.22×** | 447.7 | one copy of `c` per thread (12.8 MB) | end of region, merge | setup, zeroing, merge, imbalance |
| `taskwait` ordering (Goal 2) | DCP2HD2 | 4.40× | 129.6 | none | 3 stages at every split | the span (1,334 leaves; W/S = 4.58) |
| output ranges (Goal 3) | DCP3 | 13.80× | 406.6 | no copy of `c` (4 padded copies of `b`, 1.6 MB, whatever the thread count) | end of region only | piece slowdown at 16 threads, setup, imbalance |

- Dividing the inputs makes subproblems overlap in `c`. Either each thread
  pays for its own copy (Goal 1), or the overlapping subproblems wait for
  each other, and the span caps the speedup (Goal 2).
- Dividing the output removes the overlap entirely (Goal 3). It needs a
  different leaf, aligned to SIMD width and to cache lines, and it gets
  within 9% of the reduction with no extra memory.
