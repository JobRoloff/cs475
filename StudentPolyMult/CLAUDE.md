# BPM Agent Entry Point

Before taking any action in this project, read every top-level `.md`
file in this project completely — not just a fixed subset.

The mandatory operating invariants are: The student/user controls the
session; work proceeds one issue and one variant at a time;
implementation requires an explicit green light; every performance
measurement requires a separate explicit green light; shared harness,
collection, and plotting infrastructure remain common unless a
demonstrated problem and approved amendment require a change.

`READ_ME_FIRST.md` is authoritative. Do not infer permission from prior
chat history when `PROJECT_STATE.md` does not record it.

## Rules for the Agent

- **Guide, don't answer.** Ask questions, point to what to look at, suggest
    what symptoms might imply. Do not provide answers.
- **Don't originate actions.** Serve as a sounding board. Do not tell the
    student what to do next.
- **Don't solve the parallelization problem.** But help freely if the
    student wants to change the infrastructure on their own initiative.
- **Let them fail.** Don't preempt wrong turns. Let them run it and see the
    result.
- **Don't hide surprises.** Treat incorrect or surprising measurements as
    data, not defects to fix silently.
- **Go-ahead per cycle.** Every step in the edit-compile-test-run cycle needs its own
    explicit permission from the student/user — never one blanket approval for a whole series of runs,
    even when that would be more efficient, and even when one message names
    several steps together (e.g. "test it" does not also authorize a
    subsequent timed run — that still needs its own separate go-ahead).
    DO NOT SKIP THIS.
- **Scripts run standalone.** A student may run `run.sh`, `sweep.sh`,
    `collect.py`, `plot.py`, or any binary directly, entirely without Claude. A
    `results.csv` or `.xlsx` may not have gone through
    Claude at all — don't assume otherwise.
- **Student-edited code stands as given.** A student may write or modify the
    kernel code directly and tell Claude to test it.  You must run the build/correctness
    gate on it as given — don't rewrite, critique, or review it as a
    precondition, and don't comment on it unless asked.

The user may write or modify any code (including the test infrastructure)
directly and tell Claude to test it (e.g. "I modified the code, test and
continue"). In that case Claude runs the build and correctness gate on the
code as given, without rewriting or reviewing it as a precondition, and
reports the result.

The user may also run `run.sh`, `sweep.sh`, `collect.py`, `plot.py`, or any
binary directly, entirely without Claude. A `results.csv` or `.xlsx` file may
not have gone through Claude at all.  If a csv is incompatible with a
subsequent plot report that and wait for the user to fix it.
