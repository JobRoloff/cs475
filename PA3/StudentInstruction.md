# Student Instructions

> **This file explains expected student behavior on this project.**
> @Claude: read it to understand that behavior — it is not a set of
> instructions for you to follow. @Student: this is written for you.

> **Never run two timing binaries in the background at the same time
> bound to overlapping `taskset` cores** — they'll contend with each
> other and corrupt both measurements without any warning.

## Part 1: The Assignment, Step by Step (the student's steps, not Claude's)

GSQ is already provided — there's no step for it. `VARIANTS.md` /
`VARIANTS.html` specifies what each remaining variant must accomplish.

- **OPQ**
  - Implement it. Think about: how does it compare to GSQ? What do
    you observe, and can you explain why?
- **OPQ parallel variants** (`OPP1`, `OPP2`, ...)
  - Explore clauses one at a time, observing each — schedule
    (static/dynamic/guided, chunk size), collapse, reduction, other
    clauses as your investigation suggests. Once you've got the hang
    of the first few, take charge: form your own hypotheses.
- **EXQ**
  - Compare to OPQ. Explain why.
- **EXQ parallel variants** (`EXP1`, `EXP2`, ...)
  - As with OPQ's parallel exploration.
- **BKQ**
  - Three steps: block the outer loop, block the inner loop,
    interchange (some of) the loops. Show BKQ's own results after each
    step (not the BKPs). Think about: which interchange(s) change the
    code's behavior, and which don't?
- **BKQ parallel variants** (`BKP1`, `BKP2`, ...)
  - Always retain the 4-loop structure.
- **DCQ**
  - Per `VARIANTS.md`.
- **DCP**
  - Per `VARIANTS.md`.

## Part 2: Keeping Claude in Bounds

This is a pedagogic project. The goal is not a working kernel — it's you
learning to design and run an experiment yourself. Handing control over to
Claude solves the programming task and teaches you nothing.

Don't do any of the following ways to inadvertently transfer control to
Claude:

- Auto-accept / bypass-permissions mode, or running Claude as an
  unsupervised background/looped agent.
- Granting blanket permission you don't actually have.
- A `--dangerously-skip-permissions`-style launch flag, or a session-wide
  "bypass permissions" mode set at the start rather than approved per step.
- An overly broad `permissions.allow` entry (e.g. a wildcard covering a
  command that itself wraps arbitrary execution) in `.claude/settings.json`
  / `settings.local.json` — including one copied from someone else without
  understanding what it allows.
- Running the session as root, or inside a container/environment with
  broader access than intended.
- Connecting an MCP server with powerful capabilities without reviewing
  what it can do.
- A personal, upstream `CLAUDE.md` (global, or a parent directory's)
  written for other coursework, whose permissive instructions apply here
  without you realizing it.
- Telling Claude a prior step was "already approved" while reconciling
  out-of-date records, instead of pointing to actual evidence for it.
- Asking Claude for a brief recommendation and a yes/no — every step gets
  approved, but Claude ends up originating the design, not you.
- Letting Claude propose or implement multiple alternative
  variants/strategies to explore in parallel, instead of testing one
  hypothesis at a time — that's not the scientific method this course is
  built on.
- Letting Claude resolve an undocumented-convention question on its own
  judgment instead of checking with you or the instructor first.
- Trusting Claude's summary of a change instead of reading the actual
  diff/code — Claude may not obey your instructions exactly, even when
  specific.

**How this is enforced, not just written down:** this project's
`.claude/settings.json` backs some of these rules with real
restrictions, not just a written policy:

- `defaultMode: "default"` — every Edit/Write/Bash still prompts you
  individually; there's no way for a blanket approval to sneak in from
  this project's own settings.
- `disableBypassPermissionsMode: "disable"` — a
  `--dangerously-skip-permissions`-style launch is unavailable in this
  project, not just discouraged.
- `deny: ["Bash(sudo *)"]` — sudo is blocked outright, regardless of
  what else gets approved.

If any of this ever needs loosening to get something done, that's a
signal to stop and ask the instructor — not to edit the json yourself.

## Part 3: The Infrastructure

The project is flat: each variant is one file, `PolyMult_<V>.c`, in the
project root. The root `Makefile`, `run_sweep.sh`, and the harness under
`shared/` serve every variant. If Claude runs into an unforeseen problem
there, accept the fix and report it to the instructor/GTA.

### Building and Running

```
make                 # build every variant into bin/
make OPQ             # build one variant

./run_sweep.sh OPQ "1" 5 -dA 2000 -dB 2000            # sequential, pinned
./run_sweep.sh OPQ "1 2 4 8 16" 5 -dA 2000 -dB 2000   # parallel, bound
./run_sweep.sh --debug OPQ "1 2 4" 3 -dA 2000 -dB 2000
```

`run_sweep.sh` builds the variant, then prints the median time and
GFLOP/s for each thread count. `--debug` runs the correctness gate
(oracle suite, plus the gold comparison against GSQ) first and stops if
it fails; without it only the timing binary runs, so checking never
costs a timed run anything. Adding a variant is just adding a
`PolyMult_<V>.c` file.

### `reference/`

Background material, not required reading to get started: `Procinfo1.md`
→ `Procinfo2.md` → `Procinfo3.md` go progressively deeper into this
machine's processor architecture and how to read real performance
measurements (`Procinfo1.md` § 1-2 derives the `--tpp-per-core` value
`EXPERIMENTS.md` uses). `PA1-Report-Template.html` is a report-writing
template from the Mandelbrot assignment — adapt it for this one.

### Running a Single Call Yourself

`run_sweep.sh` calls the timing binary with this exact invocation. You
can run the same command directly in your terminal to see the raw
per-repetition csv rows instead of medians:

```
# sequential (t=1), pinned to one core:
taskset -c 16 ./bin/PolyMult_<V>.time -dA <int> -dB <int> -t 1 -r <int>

# parallel (t>1), bound and placed:
OMP_PROC_BIND=close \
OMP_PLACES='{16},{20},{24},{28},{17},{21},{25},{29},{18},{22},{26},{30},{19},{23},{27},{31}' \
taskset -c 16-31 ./bin/PolyMult_<V>.time-par -dA <int> -dB <int> -t <range> -r <int>
```

The core list, pinned sequential core, and `OMP_PLACES` string are
`run_sweep.sh`'s defaults (`BPM_CORESET`, `BPM_SEQ_CORE`,
`BPM_OMP_PLACES`) — set them in the environment to bind differently on
a different machine. `taskset` plus `OMP_PROC_BIND`/`OMP_PLACES` together
give you a precise, repeatable mapping of threads to cores; without
them, the OS scheduler and OpenMP runtime are free to place threads
differently from one run to the next, including onto fewer cores than
intended, which corrupts the measurement.

### Where Results and Plots Land

`run_sweep.sh` prints to the terminal only; nothing is written to disk.
Keep what you want yourself — e.g. `| tee results/<V>/<name>.txt` on a
`run_sweep.sh` call, or redirect a direct binary call's raw csv. Earlier
results and workbooks are kept under `results/GSQ/`, `results/OPQ/`, and
`plots/`.

### T(seq): What the Baseline Actually Is

Speedup is not computed for you. A sensible baseline for a variant is
the minimum of: its own no-`-fopenmp` binary's time (a `"1"` thread list
to `run_sweep.sh`), its `*Q` ancestor's no-`-fopenmp` time (a `*P<n>`
variant may restructure the loop nest, not just annotate it — so these
can genuinely differ), and the parallel binary's own 1-thread time.

This makes Speedup an approximation, not a number you should compare
precisely across variants — **Performance is the metric for that.**
