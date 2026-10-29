# PA4 progress record (lab notes, not the report)

Written 2026-10-29. Covers the sessions of 2026-10-27 (BKQ kernel), 2026-10-28
(sequential DCQ) and 2026-10-29 (parallel DCP1). No experiments were run and
no code was changed to write this.

**Sources.** Each table names the file it comes from. Paths are relative to
`PA4/`, and `diag/` is short for `results/DCP1/diagnostic/`. Two kinds of
numbers have no file behind them:

- **"(from session notes, no file)"**: a number seen only in a session
  transcript. Most of these come from `run_sweep.sh`, which prints medians
  and keeps nothing on disk.
- **"(computed)"**: arithmetic on file numbers, with the inputs named.

Explanations found after a run are marked **worked out afterwards**.

---

## 1. Setup

### Machine
From `reference/Procinfo1.md`:
- **CPU:** Intel Core i9-13900K, with 8 P-cores and 16 E-cores.
- **E-cores:** they come in 4 clusters of 4 cores, and each cluster shares one 4 MB L2. Base clock is 2.20 GHz. Each core issues one 256-bit FMA per cycle, i.e. 16 FLOP/cycle.
- **Peak:** 35.2 GFLOP/s per E-core (TPP_per_core), and 563.2 GFLOP/s for all 16.
- **Turbo Boost is disabled** on the fish machines this semester.

### Cores and binding
All timed and gated runs use `run_sweep.sh`'s binding (`run_sweep.sh:31–49`). `DCP1/tools/sweep_raw.sh:33–41` and `DCP1/tools/verify_timed.sh:25–30` copy it.

| Setting | Value | Source |
|---|---|---|
| Parallel runs | `OMP_PROC_BIND=close OMP_PLACES="$BPM_OMP_PLACES" taskset -c 16-31` | `run_sweep.sh:49` |
| `BPM_OMP_PLACES` | `{16},{20},{24},{28},{17},{21},{25},{29},{18},{22},{26},{30},{19},{23},{27},{31}` | `run_sweep.sh:36` |
| Sequential runs (thread list exactly `"1"`) | `taskset -c 16`, using the non-OpenMP `.time` binary | `run_sweep.sh:32,48,60–61` |
| Thread count | `omp_set_num_threads(threads)`; nothing sets `OMP_NUM_THREADS` | `shared/harness/time_main.c:98` |

- **Where the threads land:** `DESIGN.md` calls the place list "cluster-cyclic". Thread *t* is pinned to place *t*, so 2 threads use cores 16 and 20, and 4 threads use 16, 20, 24 and 28.
  - Every TRACE run confirmed that each thread's `sched_getcpu()` matched its place (`diag/trace_*.err`, "TRACE start" lines).
  - **Not confirmed from any file:** that the 4-core clusters are {16–19}, {20–23}, {24–27} and {28–31}.
- **verify_timed** sets the thread count through `OMP_NUM_THREADS` instead (`verify_timed.sh:61`).

### libgomp settings and GCC version
**These come from session notes; no file holds them.** I read them with `OMP_DISPLAY_ENV=verbose` under the sweep binding, with 16 threads, on `bin/PolyMult_DCP1T3.time-par`.

| ICV | Value | Where it comes from |
|---|---|---|
| `OMP_PROC_BIND` | `CLOSE` | `run_sweep.sh` |
| `OMP_PLACES` | the list above | `run_sweep.sh` |
| `OMP_NUM_THREADS` | 16 | `omp_set_num_threads` |
| `OMP_WAIT_POLICY` | `PASSIVE` | libgomp default |
| `GOMP_SPINCOUNT` | 300000 | libgomp default |
| `OMP_DYNAMIC` | `FALSE` | libgomp default |
| `OMP_NESTED` | `FALSE` | libgomp default |
| `OMP_MAX_ACTIVE_LEVELS` | 1 | libgomp default |
| `OMP_SCHEDULE` | `DYNAMIC` | libgomp default |
| `OMP_STACKSIZE`, `GOMP_STACKSIZE` | 0 | libgomp default |
| `OMP_THREAD_LIMIT` | 4294967295 | libgomp default |
| `GOMP_CPU_AFFINITY` | `''` | libgomp default |
| `_OPENMP` | 201511 | compiler |

- **The shell sets none of these.** `env` showed no `OMP_*`, `GOMP_*` or `KMP_*` variables.
- **GCC version:** `gcc (GCC) 11.5.0 20240719 (Red Hat 11.5.0-14)`, linked against the system `/lib64/libgomp.so.1`.

### Problem sizes and the leaf grid
Leaf size is L = 1,280 floats, BKQ's best single-core block size in PA3 (`results.md`, Experiment 1).

| Runs | Size (degrees) | Coefficients per operand | Blocks per side | Leaves |
|---|---|---|---|---|
| DCQ sequential baseline | 100,000 × 100,000, passed as `-dA 100000 -dB 100000` (`results.md`) | 100,001 | 78 full + one 161-wide edge = 79 | 79 × 79 = 6,241 |
| Every DCP1 run: sweeps, traces, `run_sweep.sh` | 99,999 × 99,999, which is the harness default (`shared/harness/args.c:62`) | 100,000 | 78 full + one **160**-wide edge = 79 | 79 × 79 = 6,241 |

- **Your "161-wide edge" matches DCQ's size only.** For the DCP1 size the edge is 160 wide (computed: 100,000 − 78 × 1,280 = 160).
- **Every trace confirms 6,241 leaves in every call** (all `diag/*.err` files).
- **Why the two sizes differ:**
  - The first DCP1 run was `./run_sweep.sh DCP1 "1 16" 5` with no size arguments, so it used the 99,999 default (from session notes, no file).
  - Every later DCP1 run kept 99,999 so the results stayed comparable. `sweep_raw.sh` was given `-dA 99999 -dB 99999` explicitly (the `dA`/`dB` columns of the CSV).
  - The DCQ baseline was never rerun at 99,999, and DCP1 was never run at 100,000.
- **Work:** 2 × 100,000² = 2.0 × 10¹⁰ FLOP at 99,999 (computed). That is consistent with the CSV, e.g. 0.6802 s gives 29.4 GFLOP/s.

---

## 2. Sequential DCQ (`PolyMult_DCQ.c`)

### Design
From `PolyMult_DCQ.c` and `results.md` Experiment 1.
- **Recursion:** `dc(i0, na, j0, nb)` multiplies `a[i0 .. i0+na)` by `b[j0 .. j0+nb)` and adds the result into `c` at offset `i0 + j0` (`PolyMult_DCQ.c:170–200`).
  - It splits the longer range, or both ranges when neither is more than twice the other.
  - `half()` rounds each split to whole L-blocks (first half = ⌊blocks/2⌋ · L), so every subproblem starts at a multiple of L. A 3-block range splits 1 + 2.
- **Leaves:** when both ranges are ≤ L, the leaf runs BKQ's final kernel: padded tiles, R = 8 rows fused, U = 3 interleaved vectors (24 outputs per pass).
  - Rows past the end of `a` are zero-padded, so the last row group runs the same vector code.
  - The c-alignment offset is `h = (4 − ((i0 + j0) & 3)) & 3`, and the leaf uses `__builtin_assume_aligned(c, 16)` (`PolyMult_DCQ.c:38–60`).
- **Padded b copies are built once, before recursing:** NCOPY = 4 copies of each zero-padded L-block, shifted by 0–3 floats, each starting on a 64-byte boundary (`PolyMult_DCQ.c:215–236`).
- **Setup is serial:** zeroing `c` and building the copies run on one thread before the recursion.
- **`kernel_build_params()`** returns `"r=8;u=3"`.

### Review findings
These are recorded as you listed them. I found no written record of the review in the project or in the three session transcripts.
1. **Partial row groups write `c[k] += 0` past their own range when L % 8 ≠ 0.** That is harmless when sequential and a race in parallel.
   - DCP1 rejects any leaf size with `L % 8 ≠ 0` (`PolyMult_DCP1.c:545–548`). DCQ does not.
   - With L % 8 = 0, the last block of `a` can still contain a partial row group when dA + 1 isn't a multiple of 8 (comment at `PolyMult_DCP1.c:545–546`). **Not checked:** whether that remaining case can race in DCP1.
2. **Serial setup.** In the parallel versions this shows up as the "setup" phase of the traces (§3b, §3d): about 1.2–1.4 ms for TRED=1 and 0.3–1.0 ms for TRED=3.

### Baseline
| Variant | GFLOP/s | % of TPP (35.2) | × OPQ | Source |
|---|---|---|---|---|
| OPQ (PA2/PA3) | 9.41 | 26.7% | 1.00 | `PA3-report.html`, `results.md` |
| BKQ (PA3, block 1,280) | 29.27 | 83.2% | 3.11 | `PA3-report.html`, `results.md` |
| DCQ (leaf 1,280) | **29.46** | 83.7% | 3.13 | `results.md` (median 0.679 s over 5 reps) |

- **DCQ baseline run:** `./run_sweep.sh DCQ "1" 5 -dA 100000 -dB 100000` on 2026-10-28, pinned to core 16. It printed a median of 0.678863120 s, 29.462 GFLOP/s (from session notes, no file).
- **Hypothesis before the run** (`results.md`): "DCQ will be slightly slower than BKQ", because of the recursion's overhead.
  - **Refuted:** DCQ came out 0.6% faster.
  - `results.md` attributes the difference to noise: it is inside PA3's rep-to-rep spread of up to 0.8%, and BKQ was measured on a different day.
- **Gate (2026-10-28):** `.check` passed 42/42 oracle and 14/14 gold (from session notes, no file). Only L = 1,280 was gated; the oracle uses its own tune settings.
- **Related run, not a same-session rerun:** on 2026-10-27 the R = 8 / U = 3 / BS = 1,280 kernel was written in this directory as `PolyMult_OPQ.c`.
  - That run, `run_sweep.sh … "1" 5 -dA 100000 -dB 100000` on core 16, measured 0.669685989 s and 29.865 GFLOP/s (from session notes, no file).
  - `PolyMult_OPQ.c` has since been deleted (git status), and a `PolyMult_BKQ.c` exists with `build_params` "bs=1280;r=8;u=3".
  - **Not checked:** whether that file is the same kernel. There are no BKQ binaries in `bin/`.

### Still missing for DCQ
- **A BKQ rerun in the same session as DCQ.** `results.md` has it as "pending".
- **A leaf-size scan.** `TUNING.md` asks for one (coarse, then fine), and none has run.
- **The DCQ baseline's individual reps.** `run_sweep.sh` keeps only the median, and no raw-rep file exists for DCQ.
- **`results.md` Experiment 1 still has placeholders:** "Gated: [yes/no …]. Results: [csv path]".

---

## 3. Goal 1: parallel DCQ with a reduction (DCP1)

### 3.0 DCP1 as first built (TRED=1)
- **Structure:** setup as in DCQ, then `parallel` → `single` → `taskgroup task_reduction(+: c[0:out_len])`, and the root is created as a task.
  - Above the task cutoff, each task creates its up to 4 children as `task in_reduction(+: c[0:out_len])`, with no `taskwait`.
  - At or below the cutoff, the recursion runs sequentially, as in DCQ.
- **Cutoff:** `--task-threshold`, in leaves per side, default 2.
- **Private copies:** each task reads `c` inside the task region, so leaves get the private copy.
- **Alignment:** `h` comes from the actual address of `c`; there is no `assume_aligned` on `c`.
- **Checking build:** it also reduces `comp`.
- **build_params:** `"r=8;u=3"`.

**Hypothesis**, verbatim from `results.md` Experiment 2, written before the run:
> Spawning all four children of every split as tasks, inside a taskgroup with task_reduction(), removes every ordering between them … The span drops from about 1000 leaves (the taskawait-only order) to about one leaf plus the merge, so the speedup is limited only by overhead outside the leaves:
> - 1 thread: within 1% of sequential DCQ (29.46 GFLOP/s) …
> - 16 threads: aqbout 15.3-15.5x (450-457 GFLOP/s). Linear would be 679 / 16 = 42.2 ms; add about 1 ms for the merge (PA3-BKP1 measured 1.06 ms) about .4 ms of setup on one thread, giving about 43.8 ms
> - the shape might be closer to linear at every thread count, with no staircase becase leaves are dealt one at a time.

**First runs.** Gate on 2026-10-29: `.check` and `.check-par` (16 threads) both passed 42/42 and 14/14 (from session notes, no file).

`./run_sweep.sh DCP1 "1 16" 5`, at 99,999 × 99,999, all on `.time-par` (from session notes, no file):

| Threads | Median (s) | GFLOP/s | vs DCQ |
|---|---|---|---|
| 1 | 0.6805 | 29.39 | 0.24% slower |
| 16 | 0.0511 | 391.1 | 13.3× |

- **1 thread:** consistent with the hypothesis (within 1%).
- **16 threads: refuted.** Measured 13.3× against the predicted 15.3–15.5×, and 51.1 ms against the predicted 43.8 ms.

**First 1–16 sweep:** `./run_sweep.sh DCP1 "1 … 16" 5`, with speedup against this sweep's own 1-thread time (from session notes, no file):

| T | s | GFLOP/s | Speedup | | T | s | GFLOP/s | Speedup |
|---|---|---|---|---|---|---|---|---|
| 1 | 0.6802 | 29.40 | 1.00 | | 9 | 0.0898 | 222.84 | 7.58 |
| 2 | 0.6810 | 29.37 | 1.00 | | 10 | 0.0809 | 247.15 | 8.41 |
| 3 | 0.3427 | 58.36 | 1.98 | | 11 | 0.0729 | 274.19 | 9.33 |
| 4 | 0.2299 | 87.00 | 2.96 | | 12 | 0.0672 | 297.75 | 10.13 |
| 5 | 0.1740 | 114.91 | 3.91 | | 13 | 0.0625 | 320.21 | 10.89 |
| 6 | 0.1401 | 142.76 | 4.86 | | 14 | 0.0580 | 344.92 | 11.73 |
| 7 | 0.1012 | 197.55 | 6.72 | | 15 | 0.0546 | 366.53 | 12.47 |
| 8 | 0.1009 | 198.20 | 6.74 | | 16 | 0.0510 | 392.13 | 13.34 |

- **"Close to linear, no staircase": refuted.**
  - 2 threads were no faster than 1.
  - 8 threads were no faster than 7 in this sweep.
  - Elsewhere the speedup trailed the thread count by about 1.

**RACE control.** The RACE build is `-DRACE`: no `task_reduction` or `in_reduction`, so plain tasks write the shared `c`. It is a separate binary, `bin/PolyMult_DCP1.check-par-race`, run at 16 threads with the sweep binding (`diag/race_gate.txt`).
- **The requirement was "it must fail". It failed, with exit 1.**
- **Oracle: 8 of 42 failed** (`race_gate.txt:87`).
  - 100000×3: constants.
  - 3×100000: deterministic-pattern, single-random-operand, alternating-ramp.
  - 20000×20000: constants (20,491 of 40,001 outputs wrong), deterministic-pattern, alternating-unit, alternating-ramp.
- **Gold: 2 of 14 failed,** both at 20000×20000 (`race_gate.txt:122`).
- **Every size that creates only one task passed:** 500×500, 0×500, 500×0 and 0×0.

**Control sweep:** the normal build, `./run_sweep.sh --debug DCP1 "1 … 16" 5`. The gate passed 42/42 and 14/14, and all of the following is from session notes, no file:

| T | s | | T | s | | T | s | | T | s |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0.6803 | | 5 | 0.1736 | | 9 | 0.0893 | | 13 | 0.0618 |
| 2 | 0.6813 | | 6 | 0.1399 | | 10 | 0.0806 | | 14 | 0.0577 |
| 3 | 0.3425 | | 7 | **0.1150** | | 11 | 0.0727 | | 15 | 0.0539 |
| 4 | 0.2301 | | 8 | 0.1005 | | 12 | 0.0670 | | 16 | 0.0511 |

Every row is within about 1% of the first sweep except 7 threads (0.1150 against 0.1012). So the 7 ≈ 8 flat spot did not repeat.

### 3a. Trace: one idle thread, and it is the `single` thread
No hypothesis was written down before this run. The request was for a thread / cpu / leaves / ms-in-leaves table.

The TRACE build (`-DTRACE`) ran 1 call per process, with the sweep binding (`diag/trace_{1..16}.err` and `.csv`).

| T | Wall (s) | Idle tid (cpu) | Other threads: leaves | Other threads: ms in leaves |
|---|---|---|---|---|
| 1 | 0.6841 | — | 6241 | 680.0 |
| 2 | 0.6837 | 0 (16) | 6241 | 679.6 |
| 3 | 0.3447 | 0 (16) | 3115–3126 | 339.7–340.1 |
| 4 | 0.2309 | 0 (16) | 2069–2091 | 226.5–226.8 |
| 5 | 0.1763 | 2 (24) | 1538–1575 | 171.0–171.3 |
| 6 | 0.1414 | 0 (16) | 1235–1264 | 135.9–136.3 |
| 7 | 0.1184 | 4 (17) | 1024–1054 | 113.2–113.6 |
| 8 | 0.1026 | 4 (17) | 883–903 | 97.2–97.4 |
| 9 | 0.0915 | 0 (16) | 777–786 | 84.9–85.4 |
| 10 | 0.0814 | 4 (17) | 682–711 | 75.5–75.9 |
| 11 | 0.0741 | 4 (17) | 618–640 | 67.8–68.4 |
| 12 | 0.0693 | 5 (21) | 563–572 | 61.6–62.2 |
| 13 | 0.0637 | 0 (16) | 515–528 | 56.5–57.0 |
| 14 | 0.0616 | 12 (19) | 471–489 | 53.0–53.7 |
| 15 | 0.0560 | 6 (25) | 439–456 | 48.4–49.0 |
| 16 | 0.0529 | 9 (22) | 409–425 | 45.2–45.6 |

- **Exactly one thread ran 0 leaves in every run with 2 or more threads.** The other threads were within 1% of each other.
- **Placement is ruled out:**
  - Every thread ran on its own place.
  - The idle thread moved between runs: tids 0, 2, 4, 5, 6, 9 and 12, on 7 different CPUs.
- **Time per leaf stayed at about 0.109 ms** at every T (computed from this file, e.g. 45.4 ms / 415 leaves at T = 16).
- **This first trace did not record which thread ran `single`.**

**Rerun recording the `single` thread** (`diag/trace2_{1..16}.err`):
- **In all 15 runs with T ≥ 2, the idle thread was the `single` thread.** It was tid 0 in 10 runs, and tid 7, 1, 5, 4 and 6 at T = 8, 9, 12, 15 and 16.
- **Unexplained:** at T = 4 this rerun took 0.2644 s, against 0.2309 s before. Thread 1 ran only 1,467 leaves in 260.4 ms (about 0.177 ms per leaf), while threads 2 and 3 ran 2,385 and 2,389. It was a single rep and was never repeated.

**Worked out afterwards (computed):**
- With one of T threads idle, the leaf work of about 680 ms is spread over T − 1 threads. That gives 2 threads = 1 thread exactly, and at 16 threads 680 / 15 = 45.3 ms of leaves per thread.
- Measured at T = 16: 45.0–46.0 ms in leaves (`trace2_16.err`), with a leaf span of 47.0 ms.

### 3b. Phase timing (TRED=1)
**Prediction** (`results.md`): about 1 ms of merge (PA3's BKP1 measured 1.06 ms, `PA3-report.html`) and about 0.4 ms of serial setup.

Measured from `diag/trace2_{1..16}.err`, 1 call per process, all in ms. "Region start" is just before `#pragma omp parallel`.

| T | Setup | Start → first leaf | Leaf span | Last leaf → taskgroup end (merge) | Taskgroup end → region end | free | Total |
|---|---|---|---|---|---|---|---|
| 1 | 2.113 | 0.708 | 681.163 | 0.147 | 0.008 | 0.073 | 684.212 |
| 2 | 1.214 | 1.187 | 680.508 | 0.319 | 0.009 | 0.074 | 683.310 |
| 3 | 1.244 | 1.787 | 340.835 | 0.511 | 0.009 | 0.080 | 344.466 |
| 4 | 1.320 | 1.445 | 260.993 | 0.496 | 0.010 | 0.078 | 264.342 |
| 5 | 1.339 | 1.695 | 170.536 | 0.398 | 0.009 | 0.075 | 174.053 |
| 6 | 1.340 | 2.063 | 136.705 | 0.550 | 0.011 | 0.076 | 140.745 |
| 7 | 1.349 | 2.666 | 114.127 | 0.564 | 0.011 | 0.075 | 118.791 |
| 8 | 1.337 | 2.224 | 97.875 | 0.779 | 0.011 | 0.084 | 102.310 |
| 9 | 1.349 | 1.990 | 85.852 | 0.721 | 0.022 | 0.089 | 90.023 |
| 10 | 1.353 | 2.263 | 76.472 | 0.848 | 0.014 | 0.080 | 81.030 |
| 11 | 1.318 | 2.034 | 68.818 | 0.809 | 0.014 | 0.075 | 73.068 |
| 12 | 1.352 | 3.865 | 62.582 | 1.016 | 0.015 | 0.103 | 68.933 |
| 13 | 1.364 | 3.000 | 57.625 | 1.010 | 0.015 | 0.097 | 63.111 |
| 14 | 1.375 | 4.306 | 56.379 | 1.122 | 0.018 | 0.087 | 63.287 |
| 15 | 1.358 | 5.615 | 50.259 | 1.225 | 0.015 | 0.090 | 58.563 |
| 16 | 1.315 | 2.963 | 47.045 | 1.000 | 0.017 | 0.088 | 52.429 |

- **Merge: consistent with the prediction.** It grows from 0.15 to about 1.0–1.2 ms.
- **Setup: above the prediction.** About 1.3 ms against the predicted 0.4 ms.
- **Start → first leaf was not in the prediction.** It runs from 0.7 to 5.6 ms. These are first-call numbers; see §5.4.
- **The phases outside the leaves add up to about 2–8 ms per call.** The idle thread costs more than that at T ≥ 3: about 1/T of the leaf time.

### 3c. TRED=2: `parallel reduction(task, +: c)` — still idle
- **What changed:** the reduction clause moved to the `parallel` directive. Tasks keep `in_reduction`.
- **The `taskgroup` stayed, without its `task_reduction` clause.** That was stated in the report at the time (from session notes, no file).
- **Hypothesis:** none was stated. The request was conditional ("if TRED=2 has no idle thread: 1–16 thread sweep"), with one question: "whether those ~3 ms are thread start-up or private-copy init".
- **Gate:** `check-par-tred2` passed 42/42 and 14/14 at 1 and 16 threads (from session notes, no file).

Trace from `diag/t2trace_{1,2,8,16}.err`, 1 call per process, in ms:

| T | Wall (s) | `single` tid: leaves | Setup | Start → all arrived | All arrived → first leaf | Leaf span | Last leaf → taskgroup end | Taskgroup end → region end | Total |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 0.6838 | 0: 6241 | 2.067 | 0.455 | 0.019 | 680.863 | 0.001 | 0.223 | 683.734 |
| 2 | 0.6837 | 0: **0** | 1.356 | 0.965 | 0.217 | 680.645 | 0.107 | 0.302 | 683.674 |
| 8 | 0.1028 | 0: **0** | 1.276 | 2.194 | 0.273 | 97.997 | 0.046 | 0.768 | 102.637 |
| 16 | 0.0552 | 10: **0** | 1.341 | **3.465** | 0.263 | 48.774 | 0.047 | **1.079** | 55.091 |

- **Still idle:** the `single` thread ran 0 leaves at T = 2, 8 and 16, so the sweep was not run.
- **Most of start → first leaf is "start → all arrived"** (3.47 of 3.73 ms at 16).
- **The merge moved to the end of the region** (1.08 ms at 16).
- **The question was not fully answered.** Reading the compiled binary (`objdump`, from session notes, no file) showed:
  - no zeroing of a private copy in a thread's own code before its arrival timestamp;
  - a `GOMP_parallel_reductions` call inside the "start → all arrived" window.
  - **Not checked from libgomp's source:** what that call does.
  - Start → all arrived was still 0.455 ms at 1 thread, so it isn't purely thread start-up.
- **Later evidence, worked out afterwards from files (§3h):** with TRED=2's clause and calls 2–3 of a process (so no first-call cost), start → all arrived at 16 threads is still 2.31 and 2.83 ms (`diag/tr_T2NG_16.err`). TRED=3 has no reduction clause, and there it is 0.005 ms (`diag/tr_T3NG_16.err`).
- **TRED=2's `build_params` at that time:** they didn't yet include `tred`, so the rows in `t2trace_*.csv` say `DCP1,"r=8;u=3"` even though the binary is TRED=2.

### 3d. TRED=3: hand-written per-thread copies — still idle
Specified in `implementation.md`:
- **No OpenMP reduction clauses.**
- **Copies:** `omp_get_max_threads()` copies of `c` are allocated with `aligned_alloc(64)` on every call. Each thread zeroes its own copy, then hits a barrier.
- **Leaves:** a task reads `omp_get_thread_num()` once per sequential subtree and writes that thread's copy.
- **Merge:** after the `single`'s barrier, an `omp for schedule(static)` over cache-line slices of `c`.
- **`c` is not zeroed in setup.**
- **Hypothesis:** none was stated. The step 4 sweep was conditional on "no thread idle".
- **Gate:** `DCP1T3` `.check` passed, and `.check-par` passed at 1, 3 and 16 threads, each 42/42 and 14/14 (from session notes, no file). verify_timed: see §4.

Trace from `diag/t3trace_{1,2,8,16}.err`, 3 calls per process:

| T | Call 1: `single` tid / its leaves | Call 2 | Call 3 | Wall s (calls 1–3) |
|---|---|---|---|---|
| 1 | 0 / 6241 | 0 / 6241 | 0 / 6241 | 0.6835, 0.6822, 0.6820 |
| 2 | 1 / **0** | 0 / **0** | 1 / **0** | 0.6831, 0.6823, 0.6821 |
| 8 | 4 / **0** | 6 / **0** | 2 / **0** | 0.1009, 0.0996, 0.0996 |
| 16 | 4 / **0** | 4 / 405 | 11 / **0** | 0.0498, 0.0455, 0.0480 |

**Still idle:** in 8 of the 9 calls with T ≥ 2, the `single` thread ran 0 leaves. So the sweep was not run. The one call with no idle thread, call 2 at T = 16, was the fastest of its three.

Call 2 phases (ms, `t3trace_*.err`):

| T | Setup | Start → all arrived | Zeroing | Barrier → first leaf | Leaf span | Last leaf → `single`'s barrier | Merge | Total |
|---|---|---|---|---|---|---|---|---|
| 1 | 0.889 | 0.002 | 0.347 | 0.007 | 680.623 | 0.002 | 0.312 | 682.184 |
| 2 | 0.958 | 0.005 | 0.448 | 0.170 | 680.407 | 0.079 | 0.207 | 682.276 |
| 8 | 0.959 | 0.010 | 0.521 | 0.034 | 97.768 | 0.022 | 0.202 | 99.518 |
| 16 | 0.921 | 0.005 | 0.776 | 0.046 | 43.441 | 0.001 | 0.210 | 45.402 |

**Conclusion drawn at the time, quoted from your next request:** "TRED=3 rules out the reduction as the cause of the idle thread." §3g revisits this.

### 3e. SPAWN=2 / SPAWN=3 (TRED=3 only) — still idle
**Hypothesis**, from your request, before the run:
> A thread waiting at a taskwait runs its own child tasks while it waits. That's a different path from waiting at a barrier, and it's the reliable one.

- **SPAWN=2:** after creating its children, a node does a `taskwait`.
- **SPAWN=3:** it creates all children but the last as tasks, runs the last directly, then does a `taskwait`.

**Gate:** both passed `.check`, and `.check-par` at 1, 3 and 16 threads, each 42/42 and 14/14 (from session notes, no file). verify_timed passed 32/32 at 1, 3 and 16 threads, worst 2.02 (`diag/verify_timed_spawn.txt`).

Traces from `diag/t3s2trace_{2,8,16}.err` and `diag/t3s3trace_{2,8,16}.err`. Each cell is the `single` thread's leaves [idle thread count]; wall times in s:

| Build | T | Call 1 | Call 2 | Call 3 | Wall s (calls 1–3) |
|---|---|---|---|---|---|
| SPAWN=2 | 2 | **0** [1] | 3088 [0] | **0** [1] | 0.6874, 0.3442, 0.6847 |
| | 8 | **0** [**2**: tids 0, 1] | **0** [1] | **0** [1] | 0.1200, 0.1047, 0.1037 |
| | 16 | **0** [**2**: tids 6, 7] | **0** [1] | 457 [0] | 0.0625, 0.0580, 0.0520 |
| SPAWN=3 | 2 | **0** [1] | **0** [1] | 3088 [0] | 0.6858, 0.6843, 0.3435 |
| | 8 | **0** [1] | **0** [1] | 853 [0] | 0.1139, 0.1213, 0.0961 |
| | 16 | **0** [1] | 461 [0] | **0** [1] | 0.0577, 0.0530, 0.0562 |

- **The hypothesis was refuted for this design.** Each build had an idle thread in 6 of its 9 calls, so the sweep was not run.
- **At T = 8 and 16, 11 of their 12 calls were slower than every SPAWN=1 call** (compare §3d: 0.0996–0.1009 s at 8, 0.0455–0.0498 s at 16). The exception is SPAWN=3 call 3 at T = 8, 0.0961 s.
- **Load among the working threads was much less even than with SPAWN=1.** For example, SPAWN=2 call 1 at T = 16: 117–571 leaves and 13.0–58.5 ms in leaves. Compare 383–405 leaves under SPAWN=1 (`t3trace_16.err`, call 2).
- **When no thread was idle at T = 2, the call took about half the time** (0.344 s).

### 3f. The root is a task; wait policy and spin count have no effect
**Premise**, from your request, before the run:
> SPAWN=3 should make an idle single thread impossible (the thread running the root always runs its last child directly, down to a leaf).

**Found from the code (refuting the premise):** the `single` block was `#pragma omp taskgroup TG_RED` followed by `dc_task(&ctx, 0, dA+1, 0, dB+1)`. So:
- the root is created as a task;
- the `single` thread never runs `dc_node()` on the root itself, and goes straight to wait at the end of the `taskgroup`;
- "run the last child directly" only applies to whichever thread takes the root task.

A root-tid trace line was added, the only code change in this step.

Rerun of TRED=3 SPAWN=1 under three environments, 3 calls per process (`diag/env_{asis,active,spininf}_{2,8,16}.err`). Each cell is `single` tid / its leaves / root tid / idle count:

| Setting | T | Call 1 | Call 2 | Call 3 | Wall s (calls 1–3) |
|---|---|---|---|---|---|
| As is | 2 | 0/**0**/1/1 | 0/**0**/1/1 | 1/**0**/0/1 | 0.6855, 0.6825, 0.6817 |
| | 8 | 3/**0**/1/1 | 1/775/4/0 | 4/804/3/0 | 0.1008, 0.0876, 0.0867 |
| | 16 | 6/**0**/9/1 | 12/**0**/1/1 | 8/**0**/4/1 | 0.0492, 0.0482, 0.0481 |
| `OMP_WAIT_POLICY=active` | 2 | 0/**0**/1/1 | 0/**0**/1/1 | 1/**0**/0/1 | 0.6831, 0.6824, 0.6817 |
| | 8 | 4/**0**/6/1 | 4/788/5/0 | 3/**0**/2/1 | 0.1006, 0.0873, 0.0990 |
| | 16 | 12/**0**/8/1 | 12/**0**/3/1 | 9/**0**/10/1 | 0.0495, 0.0483, 0.0502 |
| `GOMP_SPINCOUNT=infinite` | 2 | 0/**0**/1/1 | 0/**0**/1/1 | 1/**0**/0/1 | 0.6833, 0.6823, 0.6814 |
| | 8 | 4/**0**/2/1 | 1/778/7/0 | 3/**0**/5/1 | 0.1005, 0.0877, 0.0991 |
| | 16 | 7/**0**/8/1 | 12/**0**/9/1 | 12/**0**/11/1 | 0.0502, 0.0486, 0.0474 |

- **Idle calls:** 7/9 as is, 8/9 with `active` and 8/9 with `infinite`.
- **The root ran on a thread other than the `single` thread in all 27 calls.**
- **At T = 8, calls with no idle thread took 87–88 ms; calls with one took 99–101 ms.**
- **libgomp did pick up both settings** (from session notes, no file):
  - `active` gave `OMP_WAIT_POLICY='ACTIVE'` with `GOMP_SPINCOUNT='30000000000'`;
  - `infinite` gave a spin count of 18446744073709551615 with `PASSIVE`.
- **Not checked:** libgomp's source for what a thread waiting at the end of a `taskgroup` does.

### 3g. Your finding: the taskgroup was in every TRED mode
From your next request: "The single thread waits at `#pragma omp taskgroup`, which is present in every TRED mode (TG_RED only changes the clause)."
- TRED=2 and TRED=3 both still had the `taskgroup`: §3c, and the code shown in §3f.
- **So TRED=2 and TRED=3 (§3c, §3d) only removed the reduction clause.** They never removed the place where the `single` thread waits.
- "TRED=3 rules out the reduction as the cause" therefore says nothing about the taskgroup wait, which was the remaining candidate. TRED=2/3 never tested what they were meant to.

### 3h. NOTG and ROOTDIRECT — NOTG removes the idle thread
- **NOTG=1** (TRED=2 or 3 only): no `taskgroup`. TRED=3 waits at the `single`'s implicit barrier; TRED=2 waits at the end of the region.
- **ROOTDIRECT=1** (any TRED): the `single` thread calls `dc_node()` on the root itself.
- **Builds tested:** T3NG, T2NG, RD (TRED=1) and RDS3 (TRED=1, SPAWN=3).
- **The plan:** each build gets 3 calls per process at T = 2, 8 and 16, and a sweep only if it had no idle thread in all 9 calls.
- **Gate:** all four passed `.check`, and `.check-par` at 1, 3 and 16 threads, each 42/42 and 14/14 (from session notes, no file). verify_timed passed 32/32 for all four at 1, 3 and 16 threads, worst 2.02 (`diag/verify_timed_notg_rootdirect.txt`).

Traces from `diag/tr_{T3NG,T2NG,RD,RDS3}_{2,8,16}.err`. Each cell is `single` tid / its leaves / root tid / idle count:

| Build | T | Call 1 | Call 2 | Call 3 | Wall s (calls 1–3) |
|---|---|---|---|---|---|
| **T3NG** | 2 | 1/3095/0/0 | 0/3130/0/0 | 0/3119/1/0 | 0.3432, 0.3421, 0.3419 |
| | 8 | 4/770/1/0 | 6/772/1/0 | 4/783/2/0 | 0.0882, 0.0875, 0.0865 |
| | 16 | 5/394/10/0 | 2/387/1/0 | 8/389/12/0 | 0.0467, 0.0453, 0.0446 |
| **T2NG** | 2 | 0/3123/1/0 | 1/3096/0/0 | 1/3113/1/0 | 0.3437, 0.3425, 0.3422 |
| | 8 | 1/776/4/0 | 4/780/1/0 | 3/783/5/0 | 0.0903, 0.0885, 0.0876 |
| | 16 | 1/387/15/0 | 13/402/3/0 | 3/396/1/0 | 0.0499, 0.0483, 0.0475 |
| RD | 2 | 0/3106/0/0 | 1/3121/1/0 | 1/3129/1/0 | 0.3443, 0.3433, 0.3433 |
| | 8 | 0/**0**/0/**1** | 4/778/4/0 | 7/785/7/0 | **0.1023**, 0.0891, 0.0891 |
| | 16 | 0/**0**/0/**1** | 12/413/12/0 | 13/406/13/0 | **0.0543**, 0.0505, 0.0506 |
| **RDS3** | 2 | 0/3160/0/0 | 1/3160/1/0 | 1/3160/1/0 | 0.3473, 0.3434, 0.3427 |
| | 8 | 0/200/0/0 | 4/800/4/0 | 7/800/7/0 | 0.1094, 0.0901, 0.0900 |
| | 16 | 1/21/1/0 | 4/325/4/0 | 12/200/12/0 | 0.0621, 0.0544, 0.0573 |

- **NOTG (both TRED=2 and TRED=3): no idle thread in any of 18 calls.** The root still usually ran on some other thread.
- **RD alone:** the `single` thread ran the root in all 9 calls. Even so, in call 1 at T = 8 and T = 16 it ran 0 leaves and was the only idle thread.
  - Call 1 is the first call in the process.
  - **Not explained.**
- **RDS3:** no idle thread, but the `single` thread's leaves varied from 21 to 800. Working threads ranged from 21 to 506 leaves in call 1 at T = 16, so the load was uneven.
- **TRACE phase marker caveat:** in T2NG, the "taskgroup end" mark is recorded right after the root task is created, so `last_leaf_to_tg_end` comes out negative. Only setup, the arrival split, the leaf span and the total are meaningful there; see §5.6.

**Phases at T = 16** (ms; calls 2 and 3, which carry no first-call costs):

| Build, call | Setup | Start → all arrived | Zeroing / all arrived → first leaf | Leaf span | Merge (TRED=3) / last leaf → region end | Total |
|---|---|---|---|---|---|---|
| T3NG c2 | 0.914 | 0.005 | zero 0.790, barrier → leaf 0.050 | 43.254 | 0.207 | 45.221 |
| T3NG c3 | 0.276 | 0.005 | zero 0.877, barrier → leaf 0.096 | 43.069 | 0.208 | 44.533 |
| T2NG c2 | 0.938 | **2.310** | 0.714 | 43.326 | 0.927 (computed: −44.037 + 44.964) | 48.215 |
| T2NG c3 | 0.247 | **2.825** | 0.184 | 43.156 | 0.972 (computed) | 47.386 |
| RD c2 | 0.965 | 0.005 | **2.979** | 43.349 | 1.034 | 48.332 |
| RD c3 | 0.279 | 0.006 | **2.867** | 43.157 | 1.024 | 47.334 |

- **Only the builds that use OpenMP reduction clauses (T2NG, RD) spend 2.9–3.0 ms between region start and the first leaf at 16 threads, even on calls 2–3** (`start_to_first_leaf_ms`: T2NG 3.024 and 3.009, RD 2.984 and 2.873). For T2NG most of it falls before arrival; for RD (taskgroup reduction) it falls after arrival. T3NG's equivalent is zeroing plus barrier → leaf, 0.84–0.97 ms.
- **Not checked:** what libgomp does in that time.
- **Unexplained:** setup varies between calls in the same process, from about 0.9–1.2 ms down to 0.22–0.37 ms in some third calls (e.g. T3NG and T2NG at T = 8 and 16, `t3trace` at 1, 8 and 16), but not in all of them (e.g. every build at T = 2, RDS3 at T = 8 and 16).

### 3i. Final sweep
`DCP1/tools/sweep_raw.sh`, 2026-10-29. For each T, the four builds ran in turn, 5 reps each (one process per build per T), at `-dA 99999 -dB 99999`. The wrappers were touched first, and all four passed the gate first (gate output not saved).

**File:** `results/DCP1/sweep_raw_notg_rootdirect_20261029_0959.csv`, 320 rows. Medians and spreads below are computed from it.

Each cell is median s / GFLOP/s / speedup against DCP1's 1-thread median, 0.6802 s:

| T | DCP1 (TRED=1) | T3NG | T2NG | RDS3 |
|---|---|---|---|---|
| 1 | 0.6802 / 29.4 / 1.00 | 0.6797 / 29.4 / 1.00 | 0.6798 / 29.4 / 1.00 | 0.6800 / 29.4 / 1.00 |
| 2 | 0.6804 / 29.4 / 1.00 | 0.3405 / 58.7 / 2.00 | 0.3411 / 58.6 / 1.99 | 0.3427 / 58.4 / 1.98 |
| 3 | 0.3425 / 58.4 / 1.99 | 0.2276 / 87.9 / 2.99 | 0.2286 / 87.5 / 2.98 | 0.2600 / 76.9 / 2.62 |
| 4 | 0.2299 / 87.0 / 2.96 | 0.1711 / 116.9 / 3.98 | 0.1724 / 116.0 / 3.94 | 0.1750 / 114.3 / 3.89 |
| 5 | 0.1734 / 115.3 / 3.92 | 0.1370 / 145.9 / 4.96 | 0.1381 / 144.8 / 4.92 | 0.1592 / 125.6 / 4.27 |
| 6 | 0.1171 / 170.7 / 5.81 | 0.1148 / 174.2 / 5.93 | 0.1164 / 171.9 / 5.85 | 0.1304 / 153.4 / 5.22 |
| 7 | 0.1172 / 170.6 / 5.80 | 0.0985 / 203.0 / 6.90 | 0.0996 / 200.7 / 6.83 | 0.1158 / 172.8 / 5.88 |
| 8 | 0.0891 / 224.5 / 7.63 | 0.0864 / 231.5 / 7.87 | 0.0883 / 226.5 / 7.70 | 0.0986 / 202.9 / 6.90 |
| 9 | 0.0890 / 224.8 / 7.64 | 0.0771 / 259.4 / 8.82 | 0.0790 / 253.3 / 8.62 | 0.0894 / 223.8 / 7.61 |
| 10 | 0.0802 / 249.3 / 8.48 | 0.0697 / 287.1 / 9.77 | 0.0715 / 279.5 / 9.51 | 0.0805 / 248.3 / 8.45 |
| 11 | 0.0726 / 275.5 / 9.37 | 0.0637 / 314.2 / 10.69 | 0.0656 / 304.9 / 10.37 | 0.0757 / 264.3 / 8.99 |
| 12 | 0.0670 / 298.4 / 10.15 | 0.0588 / 339.9 / 11.56 | 0.0604 / 331.3 / 11.27 | 0.0693 / 288.7 / 9.82 |
| 13 | 0.0626 / 319.4 / 10.86 | 0.0544 / 367.4 / 12.50 | 0.0565 / 353.9 / 12.04 | 0.0674 / 296.6 / 10.09 |
| 14 | 0.0580 / 345.1 / 11.74 | 0.0505 / 396.3 / 13.48 | 0.0530 / 377.2 / 12.83 | 0.0617 / 324.3 / 11.03 |
| 15 | 0.0543 / 368.3 / 12.53 | 0.0481 / 416.1 / 14.15 | 0.0498 / 401.3 / 13.65 | 0.0591 / 338.3 / 11.50 |
| 16 | 0.0511 / 391.0 / 13.30 | 0.0447 / 447.7 / 15.22 | 0.0474 / 421.8 / 14.34 | 0.0557 / 358.8 / 12.20 |

- **`build_params` in the CSV:**

  | Build | `build_params` |
  |---|---|
  | DCP1 | `r=8;u=3` |
  | T3NG | `r=8;u=3;tred=3;notg=1` |
  | T2NG | `r=8;u=3;tred=2;notg=1` |
  | RDS3 | `r=8;u=3;spawn=3;rootdirect=1` |

- **Baseline choice:** the speedups use DCP1's 1-thread `.time-par` median. Against DCQ's 0.679 s (`results.md`) every speedup would be about 0.2% lower (computed).
- **Largest rep spread, (max − min) / median, at any T:**

  | Build | Spread | At T |
  |---|---|---|
  | DCP1 | 50% | 2 |
  | T3NG | 4.9% | 15 |
  | T2NG | 5.9% | 15 |
  | RDS3 | 12.8% | 16 |

**DCP1's reps fall into two groups.** These are every thread count where its spread was over 4%, with the faster group in bold:

| T | Rep times (s) |
|---|---|
| 2 | 0.6817, 0.6809, 0.6804, **0.3423**, **0.3419** |
| 4 | 0.2312, 0.2300, 0.2299, **0.1721**, 0.2293 |
| 5 | 0.1736, **0.1398**, 0.1741, 0.1730, 0.1734 |
| 6 | 0.1403, 0.1397, **0.1171**, **0.1165**, **0.1166** |
| 7 | 0.1176, 0.1177, 0.1168, **0.1002**, 0.1172 |
| 8 | 0.1018, **0.0887**, **0.0888**, 0.1005, **0.0891** |
| 9 | 0.0904, 0.0889, 0.0891, 0.0890, **0.0802** |
| 11 | 0.0739, 0.0726, 0.0729, **0.0665**, **0.0662** |
| 12 | 0.0673, 0.0669, 0.0670, 0.0671, **0.0617** |
| 15 | 0.0560, 0.0543, 0.0543, 0.0547, **0.0511** |
| 16 | 0.0541, 0.0509, 0.0488, 0.0511, 0.0512 |

- **DCP1's medians at 6–8 threads differ from the earlier sweeps.** At 6 threads the median was 0.1399 s in the control sweep (from session notes, no file) and 0.1171 s here, because 3 of these 5 reps were fast.
- **Worked out afterwards, not verified by tracing these reps:** each fast rep is close to T3NG's time at the same T, e.g. 0.3423 against 0.3405 at T = 2. That fits a rep in which the `single` thread happened not to be idle; the 3-call traces show such calls do occur under TRED=1/3 (§3d, §3f).

### 3j. Conclusion for goal 1 (from the files above)
- **The prediction was 15.3–15.5×, i.e. 450–457 GFLOP/s or about 43.8 ms at 16 threads. No build reached it.**
- **Closest: T3NG (TRED=3, NOTG).**
  - **16 threads:** 15.22×, 447.7 GFLOP/s, 44.7 ms.
  - **1 thread:** 0.6797 s, within 0.1% of DCQ's 0.679 s, so it meets the "within 1%" part.
  - **Shape:** close to linear, with no staircase: 2.00, 2.99, 3.98 … 15.22×.
- **The best build that uses OpenMP's reduction clause** is T2NG: 14.34×, 421.8 GFLOP/s at 16 threads. TRED=3's reduction is hand-written.
- **The default DCP1 (TRED=1)** reaches 13.30× at 16 threads, with 50% rep spread.
- **The gap to linear for T3NG at 16 threads** (computed):
  - Linear is 0.6802 / 16 = 42.5 ms; the measured median is 44.7 ms.
  - The gap is 2.2 ms, or 5%. That is 0.9 ms above the predicted 43.8 ms.
- **Where the 2.2 ms goes.** This comes from the TRACE build (`diag/tr_T3NG_16.err`, calls 2–3), which is not the binary that was swept (§5.5):
  - **The leaf work itself is not inflated.** The per-thread leaf time sums to 680.6–681.0 ms, against 679.6–680.1 ms at 1 thread (`t3trace_1.err`). The mean per thread is 42.54–42.56 ms.
  - **Serial setup:** 0.28–0.91 ms. The prediction was 0.4 ms.
  - **Zeroing the private copies:** 0.79–0.88 ms. This phase was not in the prediction.
  - **Merge:** 0.21 ms. The prediction was about 1 ms.
  - **Leaf span exceeds the mean per-thread leaf time** by 0.5–0.7 ms (43.07–43.25 against 42.55), i.e. imbalance and first-leaf/last-leaf skew.
  - **Barrier → first leaf:** 0.05–0.10 ms. Arrival, last leaf → barrier, and free are each ≤ 0.005 ms.
  - **Trace totals:** 45.2 and 44.5 ms.
- **What was not separated:** how much of the zeroing is first-touch page faults versus the memset itself.

---

## 4. Tools and checks added

### `DCP1/tools/verify_timed.c` and `.sh`
- **Why it exists:** the gate only runs the CHECKING build's scalar `BPM_ACC` path, and every reported rate comes from the timed build's intrinsics path. PA3 had no `verify_time.c` on this machine to port, so this was written from scratch (from session notes, no file).
- **What it checks:**
  - It links `PolyMult_DCP1.c` with the `.time-par` flags plus the build's `-D` switches, and runs it under the sweep binding.
  - **32 configurations** (`verify_timed.c:36–45`):
    - tiny: 0×0, 0×500, 500×0, 1×1, 7×7, 8×3;
    - square: 100, 500, 1279, 1280, 1281, 2559, 2560, 2561, 3000, 7681, 9999, 20000;
    - unequal sizes: 5119×5121, 12345×6789, 6789×12345;
    - lopsided: 100000×3, 3×100000, 99999×0, 0×99999, 1×5000, 5000×1, 1280×20000, 20000×1281, 40000×2000, 2000×40000, 30001×301.
  - **Inputs:** uniform in [−1, 1]. `c` is pre-filled with NaN, so an output the kernel never writes also fails.
  - **Reference:** double precision. The bound per output is |c[k] − ref[k]| ≤ (n_k + T + 2)·FLT_EPSILON·m_k, where m_k = Σ|terms|.
- **Results:**

  | Build | Threads | Result | Worst err/(ε·m_k) | File |
  |---|---|---|---|---|
  | TRED=1, TRED=3 | 1, 3, 16 | 32/32 each | 2.02 | `diag/verify_timed_clean.txt` |
  | TRED=3 SPAWN=2, SPAWN=3 | 1, 3, 16 | 32/32 each | 2.02 | `diag/verify_timed_spawn.txt` |
  | T3NG, T2NG, RD, RDS3 | 1, 3, 16 | 32/32 each | 2.02 | `diag/verify_timed_notg_rootdirect.txt` |
  | TRED=1, TRED=3, after removing the plant | 16 | 32/32 each | 1.90 | `diag/verify_timed_unplanted.txt` |

- **The planted bug** (`diag/verify_timed_planted.txt`):
  - It was an off-by-one in the first vector load of the leaf's 24-wide body: `&brow[r][m]` became `&brow[r][m + 1]`.
  - At 16 threads, **24 of 32 configurations failed for both TRED=1 and TRED=3.** The worst error was 2.970 (500×500), against values of order 1.
  - After the plant was removed, all 32 passed. The TRED=1 binaries then matched the earlier ones instruction for instruction (from session notes, no file).
- **Why 8 configurations didn't catch it.** The 8 that passed were 0×0, 500×0, 1×1, 7×7, 8×3, 100000×3, 99999×0 and 5000×1. Their mirror images 0×500, 3×100000, 0×99999 and 1×5000 all failed.
  - **Worked out afterwards from the code, not tested further:** in every passing case `b` has at most 8 coefficients (dB ≤ 7). A row group then produces at most 8 + 7 = 15 outputs, fewer than the 24 needed to enter the planted body loop.
  - At the time I described these as "one operand of degree ≤ 3, 0×0, 1×1". The files show the condition is on `b` specifically, and 7×7 passes too.

### `DCP1/tools/sweep_raw.sh`
- **Why:** it replaces `run_sweep.sh` for experiments that need every rep. `run_sweep.sh` keeps only medians, and `DESIGN.md` says "the raw per-repetition rows are not kept".
- **How it binds:** it copies `run_sweep.sh`'s binding (lines 33–41, commented as copied).
- **Before timing:** it touches a wrapper whose included file is newer than its binary, runs `make -s`, then gates `.check` and `.check-par` and stops on failure.
- **Order:** for each T, it runs each variant in turn, `REPS` reps in one process each.
- **Output:** it appends `threads,rep,seconds,gflops,dA,dB,binary,build_params,date`.
- **Gate output is printed but not saved** (it filters to "suite"/"FAIL" lines).

### The TRACE build (`-DTRACE`)
- **How it is built:** the Makefile's `time-par` command plus `-DTRACE` (and `-DTRED=…` etc.), into separate `bin/PolyMult_DCP1.time-par-trace*` binaries. The `Makefile` is unchanged.
- **Output (stderr):** per thread: tid, cpu, leaves, ms in leaves, and whether it ran `single`. Later versions add which tid ran the root (`TR_ROOT`, from §3f on).
- **Phase lines:**
  - TRED=1/2: setup, start → first leaf (split into start → all arrived and all arrived → first leaf from §3c on), leaf span, last leaf → taskgroup end, taskgroup end → region end, free, total.
  - TRED=3 (`phase3`): setup, start → all arrived, zeroing, barrier → first leaf, leaf span, last leaf → `single`'s barrier, merge, merge end → region end, free, total.
- **What it adds to the code:** a timer around every leaf (`omp_get_wtime` twice per leaf), and threadprivate counters.
- **The first version** (`trace_*`) printed only tid/cpu at start and leaves/ms at the end, with no `single` tid and no phases.

### Wrappers
Each wrapper is `#define`s plus `#include "PolyMult_DCP1.c"`, so `make` and `run_sweep.sh` build and gate it as a normal variant.

| File | Switches | `build_params` | Experiment | Source of `build_params` |
|---|---|---|---|---|
| `PolyMult_DCP1.c` (default) | TRED=1, SPAWN=1 | `r=8;u=3` | §3.0–3b, sweep reference | CSV |
| `PolyMult_DCP1T3.c` | TRED 3 | `r=8;u=3;tred=3` | §3d, §3f | `diag/t3trace_1.csv` |
| `PolyMult_DCP1T3S2.c` | TRED 3, SPAWN 2 | `r=8;u=3;tred=3;spawn=2` | §3e | `diag/t3s2trace_2.csv` |
| `PolyMult_DCP1T3S3.c` | TRED 3, SPAWN 3 | `r=8;u=3;tred=3;spawn=3` | §3e | `diag/t3s3trace_2.csv` |
| `PolyMult_DCP1T3NG.c` | TRED 3, NOTG 1 | `r=8;u=3;tred=3;notg=1` | §3h, §3i | `diag/tr_T3NG_2.csv` |
| `PolyMult_DCP1T2NG.c` | TRED 2, NOTG 1 | `r=8;u=3;tred=2;notg=1` | §3h, §3i | `diag/tr_T2NG_2.csv` |
| `PolyMult_DCP1RD.c` | ROOTDIRECT 1 | `r=8;u=3;rootdirect=1` | §3h | `diag/tr_RD_2.csv` |
| `PolyMult_DCP1RDS3.c` | ROOTDIRECT 1, SPAWN 3 | `r=8;u=3;spawn=3;rootdirect=1` | §3h, §3i | `diag/tr_RDS3_2.csv` |

- **TRED=2 has no wrapper of its own** (§3c). Its builds were made directly with gcc as `bin/PolyMult_DCP1.{check-par,time-par}-tred2` and `time-par-trace-tred2`.
- **The RACE control** is `bin/PolyMult_DCP1.check-par-race`.

---

## 5. Decisions and caveats

1. **Combining `comp` in the checking build.** `accumulate.h` defines no way to combine two partial (c, comp) results.
   - **TRED=3** uses the rule you decided (`implementation.md`, "Checking build (decided)"), derived from the invariant that the precise value is ≈ c − comp:
     - total = Σ_t ((double)copy_t[k] − comp_t[k])
     - c[k] = (float)total
     - comp[k] = (double)c[k] − total
   - **TRED=1/2** combine `comp` with a plain `+` alongside `c`. I chose that myself when writing DCP1. It is **unapproved** and marked as such in a comment (`PolyMult_DCP1.c:62`), and it loses the merge's rounding. Both still pass the gate.
2. **ROOTDIRECT under TRED=1 relies on reduction behaviour that hasn't been verified.**
   - The `single` thread's own leaves write the original `c` directly, outside any `in_reduction` task. The taskgroup's merge then adds the private copies into whatever the original holds.
   - The gate passes with libgomp. **Not checked:** whether the OpenMP spec guarantees this.
3. **Stale wrappers.** The `Makefile` lists only the wrapper file as a prerequisite, so editing `PolyMult_DCP1.c` alone does not rebuild `DCP1T3*`, `DCP1T2NG`, `DCP1RD` or `DCP1RDS3`. `run_sweep.sh` would then gate and time old code.
   - **Workarounds:**
     - each wrapper has a comment saying to touch it;
     - `sweep_raw.sh` touches a wrapper automatically when the included file is newer;
     - the wrappers were touched by hand before the §3i sweep.
   - **A real fix would be a `Makefile` change,** which hasn't been proposed or approved.
   - **Separately,** `git diff Makefile` shows one changed comment line ("make OPQ" → "make BKQ"), dated 2026-10-28, from before these sessions' DCP1 work.
4. **Traces with one call per process.** `trace_*`, `trace2_*` and `t2trace_*` ran one call per process, so every call there was the process's first. Its setup and start-up numbers include thread creation and first-touch page faults.
   - **The 3-call traces show the difference** (`t3trace_16.err`, `tr_RD_16.err`):

     | Phase, T = 16 | Call 1 | Calls 2–3 |
     |---|---|---|
     | Start → all arrived (`t3trace`) | 0.545 ms | 0.005 / 0.004 ms |
     | Start → all arrived (`tr_RD`) | 0.570 ms | 0.005 / 0.006 ms |
     | free (`t3trace`) | 0.291 ms | 0.001 / 0.000 ms |

   - So the "start → first leaf" values in §3b–§3c overstate a steady-state call. TRED=2's arrival delay is the exception (§3h).
   - From §3d on, the traces ran 3 calls per process.
   - **Unexplained:** in RD, the idle `single` thread at T = 8 and 16 appeared only in call 1.
5. **TRACE binaries are not the swept binaries.** The trace adds two `omp_get_wtime()` calls per leaf and per-thread counters. Trace wall times run close to the sweep (e.g. T3NG at T = 16: trace 44.6–46.7 ms, sweep median 44.7 ms), but phase numbers are only available from the trace build.
6. **Phase marker in NOTG traces.** The "tg_end" mark is recorded right after the root task is created. For T2NG this makes `last_leaf_to_tg_end` negative, and moves the leaves into "tg_end → region end". The T3NG `phase3` line has no tg_end mark, so it is unaffected.
7. **What replaced `verify_time` and the missing raw rows.**
   - The requested "timed-build check" / `verify_time` didn't exist in the project or in PA3. `DCP1/tools/verify_timed.{c,sh}` replaced it.
   - `run_sweep.sh` keeps no raw rows. `DCP1/tools/sweep_raw.sh` replaced it, writing to `results/DCP1/`.
   - **Every sweep before §3i went through `run_sweep.sh`,** so those exist only as medians in session notes.
8. **Gates and binary identity are recorded only in session notes.** Every gate pass count except `race_gate.txt` was read from the terminal. So were the `objdump` comparisons showing the default DCP1 unchanged across the TRED=3, SPAWN and NOTG/ROOTDIRECT edits.
   - **One recorded difference:** after the SPAWN edit, DCP1T3 (SPAWN=1) differed only in the two `__LINE__` constants of its `assert`s, which moved by 68.
9. **DCP1T3S2 and DCP1T3S3 were not re-gated after the `dc_node`/`dc_task` refactor** done for NOTG/ROOTDIRECT, which changed their code path. Their §3e results come from before that refactor.
10. **The cutoff and leaf size were tested only at their defaults:** cutoff 2 leaves per side and L = 1,280. At the defaults, only 100000×3, 3×100000 and 20000×20000 create more than one task in the oracle suite (from session notes, no file).
11. **Things I interpreted when your instructions didn't specify:**
    - `DCP1/tools/` was created as `implementation.md` names it, even though the project layout is otherwise flat.
    - `implementation.md`'s garbled TRACE line was read as "save trace output to `results/DCP1/diagnostic/`". The earlier scratchpad traces and the RACE gate log were copied there.

---

## 6. Not done yet

- **Goal 2: taskwait only.** No code or run yet.
  - **Prediction** (yours): span ≈ 1,000 leaves; W/S ≈ 6,241 / 1,000 ≈ 6.2; greedy bound at 16 threads ≥ W / (W/16 + S) = 6,241 / (390 + 1,000) ≈ 4.5×.
  - "Span ≈ 1000 leaves (the taskawait-only order)" appears in `results.md`. W/S ≈ 6.2 and ≥ 4.5× are (from session notes, no file); the arithmetic shown is mine.
- **Goal 3:** it is not defined in any project file I found.
- **Leaf-size scan** (DCQ and DCP), and **cutoff scan** (`--task-threshold`).
- **PARPREP for DCP.** Listed as you named it; it is not defined in any project file.
- **Same-session BKQ rerun** and **DCQ raw baseline reps** (§2).
- **Re-gating DCP1T3S2/S3** after the refactor (§5.9).
- **TRED=3 SPAWN=1 sweep** and **TRED=2 sweep:** never run, because their conditions weren't met.
- **Open, unexplained items:**
  - DCP1's two groups of rep times (§3i);
  - TRED=1's 7 ≈ 8 flat spot in the first sweep only (§3.0);
  - the slow 4-thread trace (§3a);
  - RD's idle thread only in call 1 (§3h);
  - the about 3 ms before the first leaf in T2NG and RD (§3h);
  - setup varying between calls of one process (§3h).
- **Unverified items:**
  - the libgomp `taskgroup`-wait and `GOMP_parallel_reductions` behaviour (§3c, §3f);
  - ROOTDIRECT's reduction semantics (§5.2);
  - the E-core cluster CPU mapping (§1).
- **Decisions still pending:**
  - approving or replacing TRED=1/2's `comp` combine (§5.1);
  - a `Makefile` amendment for wrappers (§5.3).
- **`results.md` Experiment 2** still has an empty "Experimental Setup" and "Results"; Experiment 1 still has placeholders.
- **Plots.** `READ_ME_FIRST.md` describes `plot.py` and `sweep.sh`, which are not in `PA4/`. No plots exist.

---

## 7. File index

Paths are relative to `PA4/`, and `diag/` = `results/DCP1/diagnostic/`. Same-family files are on one line. Every trace `.csv` holds the harness's raw rows for the calls in its `.err` (wall seconds, GFLOP/s, `build_params`).

### Results
| File(s) | Contents |
|---|---|
| `results/DCP1/sweep_raw_notg_rootdirect_20261029_0959.csv` | Final sweep (§3i): DCP1, DCP1T3NG, DCP1T2NG, DCP1RDS3; T = 1–16 × 5 reps; 99,999²; 320 rows |
| `diag/race_gate.txt` | RACE control `.check-par` output at 16 threads: oracle 8/42 failed, gold 2/14 failed |
| `diag/trace_{1..16}.{err,csv}` | First TRED=1 trace: per-thread start tid/cpu, end leaves/ms; 1 call per process (§3a) |
| `diag/trace2_{1..16}.{err,csv}` | TRED=1 trace with `single` tid and phases; 1 call per process (§3a–b) |
| `diag/t2trace_{1,2,8,16}.{err,csv}` | TRED=2 trace with the arrival split; 1 call per process. CSV labelled `DCP1,"r=8;u=3"` (§3c) |
| `diag/t3trace_{1,2,8,16}.{err,csv}` | TRED=3 (DCP1T3) trace, `phase3` lines; 3 calls per process (§3d) |
| `diag/t3s2trace_{2,8,16}.{err,csv}` | TRED=3 SPAWN=2 trace; 3 calls per process (§3e) |
| `diag/t3s3trace_{2,8,16}.{err,csv}` | TRED=3 SPAWN=3 trace; 3 calls per process (§3e) |
| `diag/env_asis_{2,8,16}.{err,csv}` | TRED=3 SPAWN=1 trace with root tid, default environment (§3f) |
| `diag/env_active_{2,8,16}.{err,csv}` | Same, with `OMP_WAIT_POLICY=active` (§3f) |
| `diag/env_spininf_{2,8,16}.{err,csv}` | Same, with `GOMP_SPINCOUNT=infinite` (§3f) |
| `diag/tr_T3NG_{2,8,16}.{err,csv}` | TRED=3 NOTG trace with root tid; 3 calls per process (§3h) |
| `diag/tr_T2NG_{2,8,16}.{err,csv}` | TRED=2 NOTG trace; the tg_end marker isn't meaningful (§3h, §5.6) |
| `diag/tr_RD_{2,8,16}.{err,csv}` | TRED=1 ROOTDIRECT trace (§3h) |
| `diag/tr_RDS3_{2,8,16}.{err,csv}` | TRED=1 ROOTDIRECT + SPAWN=3 trace (§3h) |
| `diag/verify_timed_clean.txt` | verify_timed, TRED=1 and TRED=3 at 1/3/16 threads: all pass, worst 2.02 |
| `diag/verify_timed_planted.txt` | Planted bug, TRED=1 and TRED=3 at 16 threads: 24/32 fail each |
| `diag/verify_timed_unplanted.txt` | After removing the plant, TRED=1 and TRED=3 at 16 threads: all pass, worst 1.90 |
| `diag/verify_timed_spawn.txt` | TRED=3 SPAWN=2/3 at 1/3/16 threads: all pass, worst 2.02 |
| `diag/verify_timed_notg_rootdirect.txt` | T3NG, T2NG, RD, RDS3 at 1/3/16 threads: all pass, worst 2.02 |

### Sources and tools
| File | Contents |
|---|---|
| `PolyMult_DCQ.c` | Sequential divide and conquer (§2) |
| `PolyMult_DCP1.c` | DCP1 with all the switches: TRED 1/2/3, SPAWN 1/2/3, NOTG, ROOTDIRECT, RACE, TRACE |
| `PolyMult_DCP1T3.c`, `…T3S2.c`, `…T3S3.c`, `…T3NG.c`, `…T2NG.c`, `…RD.c`, `…RDS3.c` | Wrappers (§4 table) |
| `PolyMult_BKQ.c` | BKQ kernel, `build_params` "bs=1280;r=8;u=3"; not run in PA4 under this name |
| `DCP1/tools/verify_timed.c`, `verify_timed.sh` | Timed-build checker and its build/run script (§4) |
| `DCP1/tools/sweep_raw.sh` | Raw-rep sweep script (§4) |
| `implementation.md` | Your TRED=3 / tools specification, with the "(decided)" sections |
| `results.md` | Your write-up so far: Experiment 1 (DCQ) and the Experiment 2 hypothesis |

### Binaries in `bin/` built outside `make`
Built with direct `gcc` using the Makefile's flags plus switches:
- `PolyMult_DCP1.check-par-race`
- `PolyMult_DCP1.check-par-tred2`
- `PolyMult_DCP1.time-par-tred2`
- `PolyMult_DCP1.time-par-trace`
- `PolyMult_DCP1.time-par-trace-tred2`
- `PolyMult_DCP1.time-par-trace-tred3`
- `PolyMult_DCP1.time-par-trace-tred3-spawn{2,3}`
- `PolyMult_DCP1.time-par-trace-{T3NG,T2NG,RD,RDS3}`
- `verify_timed_*` (8 binaries)
