Implement output-range recursion as a new variant, DCP3 (PolyMult_DCP3.c,
same layout, signature, build and --tune handling as the other variants).
Copy the padded leaf kernel (R = 8, U = 3, NCOPY = 4 shifted padded copies of
b) and the setup from DCQ/DCP2. Don't #include another variant's .c file and
don't add switches to DCP1 or DCP2. No reductions, no taskwait, no taskgroup:
every task writes the shared c, but only its own range of it.

This is PA4 goal 3 (results.md, Experiment 4). Follow CLAUDE.md's go-ahead
rule: stop and ask before each compile, test, gate, trace and timed run.

## Design

Divide the output, not the inputs. Each task owns a contiguous range of
outputs c[k0 .. k1) and computes every pair (i, j) with k0 <= i + j < k1.
No two tasks write the same element of c, so nothing has to wait.

    rec(k0, k1, depth):
        if depth == 0:  piece(k0, k1)
        else:
            m = smallest k with pairs(k0, k) >= pairs(k0, k1) / 2
            #pragma omp task  rec(k0, m, depth − 1)
            #pragma omp task  rec(m, k1, depth − 1)

- Recurse by depth, depth = log2(pieces), not by a threshold on pairs.
  Halves are only nearly equal (m is a whole output), so a threshold test
  splits some halves again and gives more pieces of unequal size. Pieces
  must be a power of two.
- Round each split point m so that &c[m] is on a 64-byte boundary
  (a multiple of 16 floats, offset by c's own alignment; clamped to
  [k0, k1]). Then every piece starts 32-byte aligned (no scalar head or
  masked tail) and no two pieces share a cache line of c (no false
  sharing). Pieces then differ by up to about 3% in pairs; report the
  spread, there is no fixed limit.
- Chunks are k0 + t·W within each piece (not a global grid: that splits
  the narrow middle pieces and adds visits). W must be a multiple of 8.

- pairs(k0, k1) = number of (i, j), 0 <= i < na, 0 <= j < nb, with
  k0 <= i + j < k1. Closed form (prefix count of the trapezoid), 64-bit
  integers. Find m by binary search on k. Must handle na != nb.
- Split by work, not width, so the pieces are equal in work. Pieces near
  the two ends of c are much wider than those in the middle (about 12,500
  outputs against 781 at 128 pieces); that is expected.
- Piece count: run-time option, default 128. Use the same option mechanism
  DCP2 uses for --task-threshold, and say which flag.

## Top level (lessons from DCP1 and DCP2, keep exactly)

- `#pragma omp parallel` → `#pragma omp single` → the single thread calls
  rec(0, na + nb − 1) **directly**. The root is not a task.
- **No `taskgroup` anywhere.** The single's implicit barrier waits for all
  tasks.
- **No `taskwait` anywhere.** Nothing needs ordering. (If one ever seems
  needed, stop and tell me: it means two pieces overlap.)
- Setup stays serial, before the region.

## The leaf: piece(k0, k1)

- Block over **outputs**, not rows: split the piece into chunks of W
  outputs (--leaf-size, default 1,280). For each chunk, loop over the row
  groups that contribute to it, rows max(0, kc0 − nb + 1) <= i <
  min(na, kc1). The chunk of c stays in L1 while successive row groups slide
  along b. Row blocking does nothing here: each row group already covers
  its whole clipped range, so the order of work is the same with or without
  it. Without output chunks, a wide piece near either end of c (up to about
  12,500 outputs) has a c window of about 50 KB, larger than L1.
- A W larger than the widest piece turns chunking off. Keep that possible:
  the one-core check compares W = 1,280 with chunking off.
- Each row group of R = 8 rows computes only outputs in
  [max(kc0, i_first), min(kc1, i_last + nb)): the part of the chunk where
  the group has at least one valid pair. The padded kernel supplies zeros
  for the rest, so the wasted FMAs stay about BKQ's.
- **Writes outside [k0, k1) are a race**: the neighbouring piece owns them.
  This replaces DCP2's "clamp the last row group".
- The padded copies of b must cover all of b, with enough zero padding at
  both ends that a clipped row group never reads past them. If DCQ already
  builds whole-b copies, reuse them; otherwise build them once in setup. Say
  which, and how much padding.
- Show the leaf's loop bounds before compiling.

## Race control (separate binary)

- RACE=1: inside each piece, split its rows at an 8-aligned midpoint and
  run the two halves as two tasks. Every pair is still computed exactly
  once, so the result is correct at 1 thread, but both halves write the
  same outputs, so at 16 threads they race. (Extending a piece by 8 outputs
  is not a race control: it counts those outputs twice on any number of
  threads, so it only tests ownership.)

It must pass the gate at 1 thread and fail at 16. Record both in
results/DCP3/diagnostic/race1_gate_{1,16}.txt. If it passes at 16, stop and
tell me.

## Checks before timing

1. Gate: .check and .check-par at 1, 3 and 16 threads, with the default
   128 pieces and also with a small piece count (e.g. 4) and a large one
   (e.g. 1,024). Save to results/DCP3/diagnostic/gate_*.txt.
2. verify_timed for DCP3 at 1, 3 and 16 threads (adapt verify_timed.sh to
   build DCP3). All 32 configurations must pass.
3. The race control must fail (above).
4. The default binary contains no TRACE or RACE code.

## TRACE build (-DTRACE, separate binary, never used for rates)

Per thread: tid / cpu / pieces / ms in pieces / ran single. Also setup time
and the time from the region's start to the last piece finishing. Run at
2, 8 and 16 threads, 3 calls per process, report call 2. Check the pairs
summed over all pieces equal na × nb in every call. Also report the number
of pieces and the smallest and largest piece in pairs.

## Timed runs (each only after a separate go-ahead)

On perch, with the idle check before starting and before each thread count
(CPUs 16–31 under 5% busy, no other user's process above 10% CPU); log it
to results/DCP3/diagnostic/idle_<stamp>.log.

1. One core: DCQ and DCP3 at 1 thread, back to back, 5 reps each,
   99,999 × 99,999, core 16. DCP3 twice: W = 1,280 and chunking off.
   Report all three medians. If DCP3's better W is more than 2% slower than
   DCQ, stop: the leaf needs work before the sweep.
2. Sweep with sweep_raw.sh, every rep saved to
   results/DCP3/sweep_raw_<stamp>.csv: DCP3 with 128 and 256 pieces,
   alternating at each thread count, 1–16 threads, 5 reps each,
   99,999 × 99,999.

Then stop and report:
- gate, verify_timed and race-control results;
- the trace tables;
- the sweep table (threads / median s / GFLOP/s / speedup vs DCQ's 1-thread
  median from step 1 / largest rep spread) for both piece counts, with the
  csv path.

Don't edit results.md. Don't run Experiment 3d or a leaf-size scan.
