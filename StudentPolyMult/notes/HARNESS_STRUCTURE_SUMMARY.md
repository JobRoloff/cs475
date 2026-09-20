# How `shared/harness` and `shared/include` Work

`shared/` is the common test-and-timing infrastructure every variant links against unmodified. It splits into two directories:

## `shared/include/` — the contract every variant must satisfy

- **`kernel.h`** — declares `kernel_multiply(a, dA, b, dB, c, tune)`, the one function every variant implements in its own `src/`. All pointers are `restrict`-qualified so the compiler can hoist/vectorize without worrying about `c` aliasing `a`/`b`. `bpm_tune_t` (block_size/leaf_size/task_threshold) carries runtime tuning knobs; thread count is deliberately *not* in it — that's an OpenMP setting the harness applies, not something a kernel reads. Also declares `kernel_build_params()`, a weak-by-default hook a variant overrides to self-report compile-time `-D` knobs (the invoking shell never sees those, so only the binary can report them).
- **`accumulate.h`** — the `BPM_ACC(c, comp, k, term)` macro. Same loop structure in both builds, only the accumulation differs: plain `c[k] += term` normally, Kahan-compensated double accumulation when `-DCHECKING` is defined. This is what makes a passing checking build *evidence* about the production (`-ffast-math`) build — control flow is identical, only rounding behavior changes.

## `shared/harness/` — the two gates and their plumbing

- **`args.c/h` + `range.c/h`** — shared CLI parsing for both binaries below: `-dA/-dB`, `-t <range>` (enumeration `1,2,4`, stride `2:16:2`, or powers-of-two `1...16`), `-r` reps, `--block-size/--leaf-size/--task-threshold`. A sequential (`non-PAR`) build rejects any `-t` value other than 1 at parse time, so a thread sweep can't silently get reported as if it were parallel.
- **`oracle.c/h` + `check_main.c`** → builds `PolyMult.check`/`.check-par`. Runs a fixed suite of six hand-derivable cases (constants, impulse, deterministic-pattern, single-random-operand, alternating-unit, alternating-ramp) across a shared list of sizes (`bpm_check_sizes`, including large symmetric sizes specifically to force real float roundoff). Error is checked against a bound normalized by `bpm_term_magnitude_sum` (sum of `|a[i]*b[j]|` per anti-diagonal) rather than by the result itself — so an ill-conditioned case (small result, large cancellation) doesn't produce a false failure.
- **`gold.h` / `gold.o`** — the second gate, but note **only the compiled object is distributed, not `gold.c`**. It's GSQ's own kernel, relinked as `bpm_gold_multiply`, used to blind-compare a variant's output against GSQ on random inputs that have no closed form (so the oracle can't cover them). Only non-GSQ variants link this (`-DBPM_HAVE_GOLD`) — checking GSQ against a copy of itself would be tautological, so GSQ's Makefile deliberately omits it and relies on the oracle alone.
- **`time_main.c`** → builds `PolyMult.time`/`.time-par`. Allocates `a`/`b`/`c` (optionally cache-line aligned via `BPM_ALIGN`), fills them with random floats, then loops over every thread count × rep calling `kernel_multiply` under `CLOCK_MONOTONIC`, printing one raw, unfiltered CSV row per (thread, rep) — no warm-ups discarded, so `sweep.sh`/`collect.py` see every observation.

## How a variant's Makefile wires it together

Each variant's `Makefile` (e.g. `00-GSQ/Makefile`) compiles its own `src/PolyMult.c` together with the relevant harness sources into four binaries, distinguished purely by `-D` flags: `CHECKING` (Kahan path + both gates), `PAR -fopenmp` (parallel), and — for non-GSQ variants — `BPM_HAVE_GOLD` plus the extra `gold.o`/renamed-source link. Headers are listed as explicit Makefile prerequisites so a change to e.g. `accumulate.h` forces a rebuild rather than leaving a stale binary that silently reruns old code.

Net effect: a variant only ever writes `kernel_multiply`; everything about parsing args, generating test cases, timing, and reporting is fixed and shared, which is what lets `sweep.sh`/`collect.py`/`plot.py` treat every variant's output identically.
