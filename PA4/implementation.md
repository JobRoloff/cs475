 Implement the taskwait-only parallel version of DCQ as a new variant, DCP2
  (PolyMult_DCP2.c, same layout, signature, build and --tune handling as the
  other variants). Copy the leaf kernel, the setup (zeroing c, the padded b
  copies) and the recursion from DCQ/DCP1. Don't #include PolyMult_DCP1.c and
other variants). Copy the leaf kernel, the setup (zeroing c, the padded b
copies) and the recursion from DCQ/DCP1. Don't #include PolyMult_DCP1.c and
don't add more switches to it. No reductions of any kind: every task writes
the shared c.

## Design

At each node above the task cutoff that splits both a and b:

    stage 1:  #pragma omp task  dc(LL)
              #pragma omp task  dc(HH)
              #pragma omp taskwait
    stage 2:  dc(LH)          // called directly, not as a task
    stage 3:  dc(HL)          // called directly, after LH returns

- LL and HH write disjoint outputs, so they run together. LH and HL write
  the same outputs and each overlaps LL and HH, so they run alone, one after
  the other. Each is still parallel inside.
- A node that splits only one range has two children whose outputs overlap
  (whenever the other range has more than one element). Run them in
  sequence, directly, with no tasks. Say in a comment that this makes
  lopsided shapes sequential; it's out of scope for this experiment.
- At or below the cutoff: sequential DCQ.
- The cutoff is `--task-threshold`, in leaves per side. Default 1 for DCP2,
  so tasks go down to single leaves and the code matches the span analysis.

## Top level (lessons from DCP1, keep exactly)

- `#pragma omp parallel` → `#pragma omp single` → the single thread calls
  dc() on the root **directly**. The root is not a task.
- **No `taskgroup` anywhere.** The single's implicit barrier waits for any
  remaining tasks.
- Setup stays as in DCQ (serial, before the region).

## Leaves must write only their own outputs

With a shared c, a leaf that writes `c[k] += 0` past its own range is a race.
- Keep DCP1's rejection of leaf sizes with L % 8 ≠ 0.
- Also clamp the last row group of a leaf: when rows past ilast are
  zero-padded, limit nout so the group writes no output above
  ilast + j0 + len − 1. Show the change, and confirm the timed build's rate
  at 1 thread is unchanged (within 1% of DCQ).

## Race controls (separate binaries, as with DCP1's RACE build)

- RACE=1: remove the taskwait (LL and HH are tasks, then LH and HL run
  immediately).
- RACE=2: keep the taskwait, but run LH and HL as two tasks together, then
  taskwait.
Both must fail the 16-thread gate. Record the oracle/gold failure counts in
results/DCP2/diagnostic/. If either passes, stop and tell me.

## Checks before timing
2. verify_timed for DCP2 at 1, 3 and 16 threads (adapt verify_timed.sh to
   build DCP2). All 32 configurations must pass.
3. The two race controls must fail (above).

## TRACE build (-DTRACE, separate binary, never used for rates)

Same per-thread table as DCP1 (tid / cpu / leaves / ms in leaves / ran
single). Also record per thread the time spent inside taskwait (from entering
it to leaving it). Run at 1, 2, 8 and 16 threads, 3 calls per process, report
call 2 or later. Check the leaves add up to 6,241 in every call.

## Sweep

DCP1/tools/sweep_raw.sh (or a copy for DCP2), saving every rep to
results/DCP2/ as csv: DCP2 with --task-threshold 1 and with
--task-threshold 2, alternating, 1–16 threads, 5 reps each,
99,999 × 99,999, L = 1,280. Gate before timing.

Then stop and report:
- gate, verify_timed and race-control results;
- the trace tables;
- the sweep table (threads / median s / GFLOP/s / speedup vs DCP2's own
  1-thread time) for both cutoffs, with the csv path.

Don't run a leaf-size scan yet.