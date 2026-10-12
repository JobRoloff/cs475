# PA3 Status and Next Steps

*Snapshot as of 2026-10-11 (commit `0658212`).*

## What PA3 asks for

1. **Part A, single core:** block (tile) the OPQ code, then tune and
   vectorize it on one core. Races don't matter yet.
2. **Part B, multiple cores:** parallelize across blocks, so different
   cores take different tiles, and use your Part A code inside each tile.
   Races matter here, and you have to explore at least two approaches:
   - the `reduction` clause, including the array-reduction form
   - a non-reduction approach that relies on tiles only conflicting with
     their immediate neighbours
3. **Goal throughout:** get as close to theoretical peak performance (TPP)
   as you can, using both fine-grained and coarse-grained parallelism.

## Where your numbers are (dA=dB=2000)

| Variant | GFLOP/s | Share of 1 E-core's peak (35.2) | Share of 16 E-cores' peak (563.2) |
|---|---|---|---|
| GSQ | ~5.8  | ~16% | ~1%   |
| OPQ | ~13.0 | ~37% | ~2.3% |

Neither variant changes with thread count (1, 2, 4, 8, 16 threads,
5 reps each).

Source data (local only, since `.gitignore` excludes csv/json/xlsx):

- `00-GSQ/results/seq.csv`, `00-GSQ/results/gsq-2000x2000.csv`,
  `00-GSQ/results/seq_time.json`
- `01-OPQ/results/opq-2000x2000.csv` (the older `01-OPQ/results/01-OPQ.csv`
  is also there)
- `plots/baseline/workbook.xlsx` (GSQ 1-thread point plus the older OPQ csv)

Your infrastructure is ready:

- Both variants pass their correctness gates.
- OPQ is now checked against GSQ (the gold suite) as well as the oracle.
- Both variants have baselines at 2000×2000.

## Questions the numbers raise

- **OPQ is flat from 1 to 16 threads.** Does `01-OPQ/src/PolyMult.c` have
  any OpenMP pragmas at all? What does a flat line tell you about whether
  the extra threads did any work?
- **On one core, OPQ reaches 37% of peak.** What would it take to reach the
  remaining 63%? `reference/Procinfo1.md`–`Procinfo3.md` explain where the
  35.2 GFLOP/s figure comes from: vector width and operations per cycle.
  How does that compare with what the compiler currently produces for
  OPQ's inner loop?
- **Each OPQ kernel call at 2000×2000 takes about 0.6 ms.** Is that long enough to
  give stable measurements once threads are added, or will start-up
  overhead dominate? Problem size is your choice.
- **Blocking needs a block size.** The harness already passes one at run
  time: `--block-size` on the timing binary, `tune->block_size` in the
  kernel. How will you choose the range to sweep?
- **Before Part B:** you can sketch which tiles conflict with which others
  and work out a schedule from that, before writing any code.

## Things to decide before your first new variant

- What to name the Part A and Part B variants and how to number their
  directories. `VARIANTS.md` and `TUNING.md` only cover the older
  GSQ→DCP sequence.
- Whether to record those decisions in a `PROJECT_STATE.md`. `CLAUDE.md`
  treats that file as the record of what you've approved.

## Loose ends

- `PA3/.claude/settings.json` is excluded by the `*.json` rule in
  `.gitignore`, so it isn't version-controlled.
- `EXPERIMENTS.md` is referenced by the docs but missing. The old repo
  only has it as `StudentPolyMult/html/EXPERIMENTS.html`.
- Plotting needs the course virtualenv, because the system Python lacks
  `xlsxwriter`: `../.venv/bin/python plot.py ...`
