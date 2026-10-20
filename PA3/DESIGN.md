# DESIGN

This document explains the code structure and design of the common `C`
code harness and the kernel interface. Neither Claude nor the user will
normally need to modify it. If a bug fix is required, ask the user to
report it to the instructors.

## Layout

The project is flat: every variant is one source file in the project
root, and one `Makefile` and one run script serve all of them.

- `PolyMult_<V>.c` — variant `<V>`'s kernel (`PolyMult_GSQ.c`,
  `PolyMult_OPQ.c`, ...). Parallel variants follow the same pattern
  (`PolyMult_OPP1.c`, `PolyMult_EXP2.c`, ...).
- `Makefile` — builds every variant (see Build system below).
- `run_sweep.sh` — builds a variant, optionally runs its correctness
  gate, and prints the median time per thread count (see CLI reference).
- `bin/` — build output: `PolyMult_<V>.{check,check-par,time,time-par}`
  and `gsq_gold.o`.
- `results/<V>/`, `plots/` — saved results and workbooks.
- `shared/include/kernel.h` — the interface every variant implements.
- `shared/include/accumulate.h` — the accumulation macro that EVERY
  VARIANT MUST USE (reasons are given below).
- `shared/harness/args.{c,h}` — CLI argument parsing (`bpm_args_t`,
  `bpm_parse_args`).
- `shared/harness/range.{c,h}` — thread-range parsing (`-t`).
- `shared/harness/oracle.{c,h}` — the oracle suite.
- `shared/harness/gold.h`, `shared/harness/gold.o` — the gold-comparison
  suite (linked by every variant except `GSQ`). Provided as a
  precompiled object only — `gold.c` itself is never distributed to
  students, and is never rebuilt by the `Makefile`.
- `shared/harness/time_main.c` — builds `PolyMult_<V>.time[-par]`: fills
  random inputs, times `kernel_multiply`, prints one raw csv row per
  repetition to stdout.
- `shared/harness/check_main.c` — builds `PolyMult_<V>.check[-par]`: runs
  the oracle suite, and the gold suite if `BPM_HAVE_GOLD` is defined.

## The kernel contract

Every variant defines `kernel_multiply` in its own `PolyMult_<V>.c`,
matching the signature in `shared/include/kernel.h`. What isn't obvious
just from reading the header:

- `dA` and `dB` are independent — never assume `dA == dB`.
- Every pointer parameter must be `restrict`-qualified, on the
  definition as well as the declaration. This isn't compiler-enforced
  — it's a promise that `a`, `b`, `c` never alias each other or
  internally, which licenses optimizations the compiler couldn't
  otherwise make.
- A variant with a compile-time parameter (a `-D`, not a runtime flag)
  must define `kernel_build_params(void)`, returning a
  `key=value;key=value` string. A variant with none defines nothing —
  `time_main.c` supplies a weak default returning `""`.

## Accumulation

Every loop body must use `BPM_ACC`, from
`shared/include/accumulate.h`, in place of a direct accumulation
statement:

```c
BPM_COMP_DECL(comp, n);   /* once, before the accumulation loop */
...
BPM_ACC(c, comp, k, term);  /* c[k] += term, for every accumulation */
...
BPM_COMP_FREE(comp);
```

Skipping the macro will fail the correctness check: our
transformations often change the order of accumulations, which
introduces significant numerical error, so during checking we use
Kahan's algorithm to get a precise answer to compare against.

## Build system

One root `Makefile`. Variants are discovered from the filenames —
every `PolyMult_<V>.c` is a variant, so adding a file adds a variant
with no `Makefile` edit. `make` builds all of them; `make <V>` builds
one.

Each variant gets four binaries, into `bin/`, all with
`-DBPM_VARIANT='"<V>"'`:

| Binary                      | Sources                              | Flags                         |
|-----------------------------|--------------------------------------|-------------------------------|
| `PolyMult_<V>.check`        | `PolyMult_<V>.c` + oracle, check_main | `-DCHECKING`                  |
| `PolyMult_<V>.check-par`    | same                                 | `-DCHECKING -DPAR -fopenmp`   |
| `PolyMult_<V>.time`         | `PolyMult_<V>.c` + range, args, time_main | `-ffast-math`             |
| `PolyMult_<V>.time-par`     | same                                 | `-ffast-math -DPAR -fopenmp`  |

For a non-`GSQ` variant, `PolyMult_<V>.check`/`.check-par` also get
`-DBPM_HAVE_GOLD` and link two extra objects: `shared/harness/gold.o`
(provided, precompiled — see Layout above) and `bin/gsq_gold.o`, a
recompile of `PolyMult_GSQ.c` with `kernel_multiply` renamed to
`bpm_gold_multiply` (so gold.o can call it as the reference alongside
the variant's own `kernel_multiply`). That recompile takes neither
`-DBPM_VARIANT` nor `-DPAR` — `gold.o` is variant-agnostic, and the GSQ
recompile must stay GSQ's true checking build regardless of which
variant links it. `GSQ` itself links no gold: checking it against a
copy of itself would prove nothing.

Every header the sources include (`kernel.h`, `accumulate.h`,
`args.h`, `range.h`, `oracle.h`) is a prerequisite of every binary
target. Without this, editing a header leaves stale binaries in place
and a correctness gate silently re-runs old code.

`make debug` rebuilds with `-O0 -g -Wall -Wextra`; `make clean` empties
`bin/`.

## CLI reference

`run_sweep.sh`:

```
./run_sweep.sh [--debug] <V> "<thread list>" <reps> [binary args...]

  ./run_sweep.sh OPQ "1" 5 -dA 2000 -dB 2000
  ./run_sweep.sh OPQ "1 2 4 8 16" 5 -dA 2000 -dB 2000
  ./run_sweep.sh --debug OPQ "1 2 4" 3 -dA 2000 -dB 2000
```

- Runs `make -s <V>` first.
- `--debug` runs the correctness gate (`.check`, then `.check-par`)
  before timing and stops if it fails. Without it, only the timing
  binary runs.
- A thread list of exactly `"1"` runs `PolyMult_<V>.time` (no
  `-fopenmp`) pinned to `BPM_SEQ_CORE`; any other list runs
  `PolyMult_<V>.time-par` under `taskset -c $BPM_CORESET`,
  `OMP_PROC_BIND=close`, `OMP_PLACES=$BPM_OMP_PLACES`. The gate's two
  binaries are bound the same way.
- Defaults assume the department's fish machines: `BPM_CORESET=16-31`,
  `BPM_SEQ_CORE=16`, cluster-cyclic `OMP_PLACES`
  (`{16},{20},{24},{28},{17},...`). Override any of them in the
  environment.
- Prints `threads  median_s  gflops  reps`, one line per thread count.
  The raw per-repetition rows are not kept — run the binary directly
  for those.

`PolyMult_<V>.time[-par]`:

```
-dA <int>              degree of polynomial A
-dB <int>              degree of polynomial B
-t <range>             thread count(s): enumeration, stride, or powers of 2
-r <int>               repetitions per configuration (default: 3)
--block-size <int>
--leaf-size <int>
--task-threshold <int>
```

`PolyMult_<V>.check[-par]`: `[-h|--help]` only. `-dA`/`-dB`/`--validate`
are accepted for command-line-shape consistency but do not change what
runs — the oracle (and gold, if linked) suites use their own fixed
cases.

## Raw output format

`PolyMult_<V>.time[-par]` prints one csv row per repetition to stdout:

```
variant,dA,dB,threads,block_size,leaf_size,task_threshold,build_params,rep,seconds,gflops
```

`build_params` is quoted and may contain commas (a schedule clause
does), so read `seconds` and `gflops` from the end of the row.
`run_sweep.sh` reduces these rows to a median per thread count.
