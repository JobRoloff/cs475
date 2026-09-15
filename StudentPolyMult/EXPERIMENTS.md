# EXPERIMENTS

**Never run two timing binaries in the background at the same time bound
to overlapping `taskset` cores** — they'll contend with each other and
corrupt both measurements without any warning.

The user-facing interface to running an experiment and turning it into a
workbook: `sweep.sh`, `collect.py`, `plot.py`. Read this before running a
sweep or building a workbook.

## The three tools

**`sweep.sh`** — one thread-count sweep for one variant, one results csv.

```
sweep.sh --variant-dir DIR --dA N --dB N --threads RANGE --reps N
         (--out FILE.csv | --name NAME)
         [--make-var KEY=VALUE ...] [--extra KEY=VALUE ...]
         [--seq-time SECONDS [--seq-source TEXT] | --record-seq | --renew-seq]
```

Builds the variant, runs its correctness gate, runs the timed sweep, writes
the csv — every parallel invocation bound (see `_gate.sh`), no unbound
`.check-par`/`.time-par` possible.

- `--name` writes to `<variant-dir>/results/<name>.csv`; a sweep's raw data
  belongs to its own variant directory. `--out` overrides this.
- `--make-var KEY=VALUE` forwards a compile-time knob to `make` (e.g.
  `SCHED=dynamic,8`, `UNROLL=8`) — one rebuild per call. Comparing values
  means calling `sweep.sh` once per value, each to its own output.
- `--extra KEY=VALUE` records a constant metadata field in the csv's
  preamble that isn't one of the built-in flags (e.g. `unroll=4` for a knob
  held fixed this call).
- `--allow-gate-failure` times the build even if its correctness gate
  fails, for a deliberately-incorrect diagnostic run (e.g. a `nowait` that
  should race). The gate still runs; its outcome is recorded in the csv's
  preamble as `gate_status=PASSED|FAILED`, and the csv lands under
  `<variant-dir>/results/diagnostic/` instead of `results/`, so it can
  never be mistaken for a trusted measurement. Cannot be combined with
  `--record-seq`/`--renew-seq` — a diagnostic build must never become the
  trusted sequential baseline.
- `--seq-time`/`--record-seq`/`--renew-seq`: see below.

**`collect.py`** — called internally by `sweep.sh`; not normally invoked
directly. Turns the timing binary's raw per-rep output into the results csv
(preamble of constant parameters, then one row per thread count: `median`,
`mean_wo_outliers`, `gflops_median`, `gflops_mean_wo_outliers`, computed from
`dA`/`dB` alone — `GFLOPs = 2*(dA+1)*(dB+1)`, independent of any sequential
baseline).  Any rep more than 2x slower than the fastest rep is treated as an
outlier and excluded.

**`plot.py`** — turn one or more results csvs into an `.xlsx` workbook.

```
plot.py --csv FILE [--csv FILE ...] --tpp-per-core VALUE
        [--out WORKBOOK.xlsx | --topic SLUG] [--label NAME ...]
        [--tmp VALUE] [--stat median|mean_wo_outliers] [--title TITLE]
```

- Multiple `--csv` superpose on Speedup (one line per run) and group on
  Performance (one bar per run per thread count).
- Output path auto-derives when `--out` is omitted: every csv under the
  same `NN-VARIANT` directory (e.g. `00-GSQ`) is a single-variant plot,
  placed inside that variant's own `plots/<topic>/workbook.xlsx`; csvs
  spanning more than one variant directory are cross-variant, placed one
  level up at the common parent's `plots/<topic>/workbook.xlsx`. `--topic`
  names the folder (defaults to a slugified `--title`).
- `--tpp-per-core` for this machine's E-cores is 35.2 GFLOPs/s (16 Cores
  × 2.20 GHz × 16 Operations/Cycle = 563.2 GFLOPs total ÷ 16 cores; see
  `reference/Procinfo1.md` § 1-2 for the derivation).
- Speedup and Performance render as **one combined chart**: Speedup per run, plus an "Ideal speedup"
  (Speedup(t) = t) line, on the left axis; %TPP bars per run, grouped per
  thread count, on the right ("Thread-normalized Performance") axis.
  %TPP is thread-normalized
  (`%TPP(t) = GFLOPs/s(t) / (TPP_per_core * t) * 100`). A "100% TPP"
  reference bar is grouped alongside the %TPP bars whenever the tallest
  bar exceeds 67% of TPP; at or below that, the axis is capped just above
  the tallest bar and the reference is dropped, noted in the chart's own
  x-axis title.
- The thread axis lists every integer from 1 to the largest swept thread
  count, not just the counts actually measured, so the axis reads as
  linear even when the sweep itself is sparse/exponential; unmeasured
  counts are `=NA()` and draw nothing.
- **A csv with no `seq_time` in its preamble is refused (`SystemExit`, not
  a warning).** Every workbook `plot.py` builds needs a real sequential
  baseline behind it — see the carry-through mechanism below.

## T(seq) carry-through

The sequential baseline T(seq) starts from each variant's own true
sequential binary (`PolyMult.time`, no `-DPAR`/`-fopenmp`) run at a given
problem size — **every variant records its own**, including a `*P<n>`
parallel variant: it may restructure its `*Q` ancestor's loop nest, not
just annotate it, so its own no-`-fopenmp` binary is not assumed
identical to its ancestor's. A compile-time knob like `SCHED` has no
effect on this binary at all (an unrecognized `#pragma omp` is simply
ignored without `-fopenmp`), so one measurement covers every later sweep
of that variant *at the same size* regardless of schedule/tuning value —
but **`dA`/`dB` change the amount of work done, so T(seq) is specific to
the exact `(dA, dB)` pair it was measured at.** The sidecar is keyed on
that pair, not just the variant: `<variant-dir>/results/seq_time.json`
holds one entry per recorded `(dA, dB)` size, e.g.:

```json
{"entries": {"2000x2000": {"dA": 2000, "dB": 2000,
                             "seq_time": 0.0006037915, "seq_time_source": "..."}}}
```

**Establishing or extending it:** run `sweep.sh` once per size, with
`--threads 1`, no `--make-var`, and `--record-seq`:

```
sweep.sh --variant-dir 00-GSQ --dA 2000 --dB 2000 \
    --threads 1 --reps 11 --name seq --record-seq
```

This adds/updates just this size's entry in
`<variant-dir>/results/seq_time.json` (the measured value plus provenance)
in addition to the normal results csv — **every other recorded size is
left alone**, so recording a new `(dA, dB)` never disturbs one already in
use. `--record-seq` (like `--renew-seq` below) requires `--threads 1` and
refuses `--make-var`/`--seq-time` together with it — it must be the true,
schedule-independent sequential build.

**When the sequential kernel source itself changes, use `--renew-seq`
instead — same requirements, but it wipes every other recorded size
first, keeping only the one just measured:**

```
sweep.sh --variant-dir 00-GSQ --dA 2000 --dB 2000 \
    --threads 1 --reps 11 --name seq --renew-seq
```

The sequential kernel changes rarely, but when it does, every existing
entry is simultaneously stale, not just the one being re-recorded — a
later sweep at another size must not silently reuse a baseline from the
old kernel. Recording a *new* problem size, by contrast, is common and
shouldn't cost every other size its baseline. `sweep.sh` prints a warning
naming what got wiped whenever `--renew-seq` actually removes another
size's entry. Using plain `--record-seq` when the kernel changed would
leave other sizes' entries silently stale; using `--renew-seq` for a
merely-new size would cost unnecessary re-recording of sizes still in
use — pick the one that matches what actually happened.

**Every later sweep at a recorded size picks it up automatically, as the
minimum of up to three candidates**, none trustworthy alone:

1. **own** — this variant's own recorded `seq_time.json` entry.
2. **base** — for a `*P<n>` variant only, its `*Q` ancestor's own
   recorded `seq_time.json` entry (e.g. `03-BKP2`'s ancestor is
   `03-BKQ`, derived from the directory name). Not applicable to a `*Q`
   variant itself, where base and own are the same thing.
3. **par@1** — this sweep's own `t=1` row, when the thread range
   includes 1.

Whichever of the three are actually available get compared, and the
smallest wins; `seq_time_source` in the csv's preamble names which one
and its value. An explicit `--seq-time` on a call always overrides all
three.

This makes Speedup an *approximation*, not a precisely cross-variant
-fair number: a `*P<n>` that happens to have a slower or faster own
sequential binary than its `*Q` ancestor will show a correspondingly
skewed Speedup curve relative to a sibling `*P<n>` — Speedup is only
meant to judge one variant's own scaling against itself. **Performance
(%TPP) is the metric for comparing across variants** — it's computed
from `dA`/`dB` alone, independent of any T(seq), so it isn't subject to
this at all.

**Every `*P<n>` variant needs its own `--record-seq` call too** —
recording only lives in its own directory, at its own risk of the same
"never assume it matches" caveat above; there is no cross-directory
recording, only cross-directory *reading* (candidate 2).

**A sweep at a size with none of the three candidates available gets no
baseline at all** — never a value borrowed from a different size.
`sweep.sh` prints a note saying to `--record-seq` at that size (in this
variant or its `*Q` ancestor); the resulting csv has no `seq_time`, so
`plot.py` will later refuse it until that's done.

## Directory layout this produces

```
<NN-VARIANT>/results/<name>.csv       one file per sweep.sh call
<NN-VARIANT>/results/seq_time.json    one sidecar per variant
<NN-VARIANT>/plots/<topic>/workbook.xlsx     single-variant plot
plots/<topic>/workbook.xlsx                  cross-variant plot (project root)
```
