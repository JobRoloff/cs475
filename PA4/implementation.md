
# DCP1: TRED=3 (hand-written per-thread reduction)

Add a third reduction mode to DCP1, TRED=3: a hand-written per-thread
reduction with no OpenMP reduction clauses. Same source file, selected with
-DTRED=3. TRED=1 (the default) and TRED=2 must stay exactly as they are.
Same recursion, same task cutoff, same leaf kernel; only how the private
copies of c are created, used and merged changes.


Copies are allocated on every call, not reused, so the comparison with
task_reduction is fair (the runtime also allocates per call). Page faults are
part of the measured cost; the trace's zeroing phase shows how much.

## Design

1. Before the parallel region (in kernel_multiply, after the existing setup):
   allocate one buffer of nthreads private copies of c with
   aligned_alloc(64, ...), each copy's stride rounded up to a multiple of
   16 floats so every copy starts on a 64-byte boundary. Check for NULL.
   nthreads = omp_get_max_threads(); inside the region, assert
   omp_get_num_threads() matches.
2. Inside `#pragma omp parallel`, before the single: each thread zeroes its
   own copy (first touch on its own core), then `#pragma omp barrier`.
3. `#pragma omp single`: start the recursion as now, with no task_reduction /
   in_reduction clauses. Wherever a leaf (or a sequential subtree below the
   cutoff) runs, read omp_get_thread_num() at that moment and pass that
   thread's copy as the c pointer. Never use x->c in TRED=3 paths. A leaf has
   no task scheduling points, so the thread can't change during a leaf. The
   single's implicit barrier guarantees every task has finished.
4. Merge, still inside the region, after the single's barrier:
   `#pragma omp for schedule(static)` over c in slices of whole cache lines
   (multiples of 16 floats), setting c[k] = Σ_t copy_t[k]. Since this assigns
   c, skip zeroing c in the setup for TRED=3 only, and say so in a comment.
5. Free the copies after the region.

## Checking build (decided)

comp is the Kahan compensation array (precise value ≈ c[k] − comp[k]).
accumulate.h defines no combine, so use this rule, derived from that
invariant, and say so in a comment:

  total   = Σ_t ((double)copy_t[k] − comp_t[k]);
  c[k]    = (float)total;
  comp[k] = (double)c[k] − total;

Each thread gets a private comp as well. TRED=1/2 keep their existing
plain-+ combine unchanged; note in a comment that this rule is unapproved
and loses the merge's rounding.

## Alignment (decided)

DCP1 computes h from the actual address. Add a debug assert that each TRED=3
copy is 16-byte aligned. TRED=1 unchanged.

## Variant wrapper (decided)

Add PolyMult_DCP1T3.c containing `#define TRED 3` and
`#include "PolyMult_DCP1.c"`, so make and run_sweep.sh build and gate it
as a normal variant. No changes to shared files (Makefile, run_sweep.sh,
shared/). kernel_build_params() must include the TRED value (e.g.
"r=8;u=3;tred=3").

## Timed-build checker (decided)

The gate only runs the CHECKING build's scalar path; every reported rate
comes from the timed build's intrinsics path. Write DCP1/tools/verify_timed.c
(if PA3's 03-BKQ/tools/verify_time.c exists on this machine, port it instead
and say where it was):
- built with the same flags as .time-par, for TRED=1 and TRED=3;
- a few dozen sizes: square (incl. 20000×20000), lopsided (100000×3,
  3×100000), not divisible by the leaf size, tiny (0×0, 0×500);
- compares every output with a double-precision reference using a relative
  tolerance; prints failing configurations and the worst error;
- run at 1, 3 and 16 threads with the sweep binding;
- once: plant an off-by-one in the timed leaf's window offset, show it fails,
  remove the plant, show it passes.

## Raw sweep rows (decided)

Save every rep, not just medians, to results/DCP1/ as csv with columns:
threads, rep, seconds, GFLOP/s, dA, dB, binary, build_params, date. Use
run_sweep.sh's exact binding and environment (copied into a DCP1-local
script, with a comment saying where from). Gate the matching check binary
before timing.

## TRACE build (TRED=3 with -DTRACE)

Per-thread table (tid / cpu / leaves / ms in leaves / ran single) and phase
table: setup; region start → all arrived; zeroing the copies (until the
barrier); barrier → first leaf; first leaf → last leaf; last leaf → single's
barrier passed; merge; free; total. 3 calls per process; report call 2 or
later. Check the leaves add up to 6,241 in every run.in
/tmp/.../scratchpad/, into results/DCP1/diagnostic/.

## Checks, in this order

1. Rebuild default binaries; gate (.check, .check-par at 16 threads).
2. Gate DCP1T3 at 1, 3 and 16 threads; verify_timed for TRED=1 and TRED=3 at
   1, 3 and 16 threads, including the planted-bug demonstration. Stop on any
   failure and report.
3. TRED=3 trace at 1, 2, 8 and 16 threads, sweep binding.
4. If no thread is idle: raw-row sweeps of TRED=1 and TRED=3, 1–16 threads,
   5 reps each, alternating the two, same size as earlier DCP1 sweeps
   (state it).