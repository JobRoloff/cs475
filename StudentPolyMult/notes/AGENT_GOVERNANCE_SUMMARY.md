# How This Project Is Structured for AI Coding Agents

This document summarizes how the repo constrains an AI coding agent (e.g. Claude Code) so it assists the student without originating design decisions, experiments, or unsupervised runs.

## Documentation layer — the governance docs

- **`CLAUDE.md`** — Claude's entry point. States the mandatory invariants: student controls the session; one issue/variant at a time; implementation needs an explicit green light; every performance *measurement* needs its own separate green light; shared harness/collection/plotting code stays common unless a demonstrated problem justifies changing it. It names `READ_ME_FIRST.md` as authoritative and says not to infer permission from prior chat history absent a record in `PROJECT_STATE.md`.
  - Note: `PROJECT_STATE.md` is referenced as the source of "was this already approved" but does not currently exist in the repo or its git history — there's no state file backing that check right now.
- **`READ_ME_FIRST.md`** — the authoritative workflow doc. Defines the **Seven-Step Protocol**: Design → Discuss → Implement (Claude needs a go-ahead per edit-compile-test-run cycle, unless the student hand-edits code and just asks for a build/gate check) → student proposes an experiment (hypothesis, variables, range, reps, pass/fail criteria) → Run it (separate green light from implementation) → Gather/plot → student chooses the next direction. Frames surprising results as data, not bugs to silently fix.
- **`StudentInstruction.md`** — addressed to the *student*, not Claude. Its "Keeping Claude in Bounds" section lists ways a student could accidentally cede control (bypass-permissions mode, unsupervised loops, broad `permissions.allow` wildcards, running as root, unreviewed MCP servers, asking Claude for "a recommendation + yes/no" that lets it originate design, letting it explore multiple variants in parallel, trusting Claude's summary instead of reading the real diff). It documents exactly how the rules are enforced in tooling (below) and says any needed loosening of that config is a signal to ask the instructor, not to self-edit.
- **`VARIANTS.md`** — per-variant technical requirements; opens with "Claude is a robot: it implements what the student decides, not its own design choices."
- **`EXPERIMENTS.md`** — reference for `sweep.sh`/`collect.py`/`plot.py`, including a `--allow-gate-failure` diagnostic mode that's deliberately incompatible with recording a trusted sequential baseline and writes to `results/diagnostic/` so a quick diagnostic run can never be mistaken for a trusted measurement.
- **`TUNING.md` / `DESIGN.md`** — technical references (tuning knobs per variant; kernel interface/build system). Explicitly discussion inputs, not authorization by themselves; `DESIGN.md` says the shared harness shouldn't normally need modification by anyone.

## Enforcement layer — `.claude/settings.json`

This is the actual teeth behind the docs. The full file:

```json
{
  "permissions": {
    "defaultMode": "default",
    "disableBypassPermissionsMode": "disable",
    "deny": ["Bash(sudo *)"]
  }
}
```

- `defaultMode: "default"` — every Edit/Write/Bash call still prompts individually; no session-wide auto-accept.
- `disableBypassPermissionsMode: "disable"` — `--dangerously-skip-permissions` can't be used to blanket-approve a run.
- `deny: ["Bash(sudo *)"]` — sudo is blocked outright, no override.

No hooks, no custom subagents/skills, no `settings.local.json` — enforcement is entirely through these three permission settings, not scripting.

## How the two layers combine in practice

- The docs tell Claude to ask permission per step even when it *could* technically do more (e.g., it could run a whole sweep after one approval, but the doc rule says not to).
- The settings make the riskiest bypass paths (auto-accept mode, sudo) structurally unavailable, so the doc rules aren't purely honor-system for the highest-stakes actions.
- The workflow scripts (`run.sh`, `sweep.sh`, `collect.py`, `plot.py`) are designed to run completely standalone from Claude — a student can and often will run them directly from the terminal, and a `results.csv`/`.xlsx` may not have touched Claude at all. Claude's role is optional infrastructure around a workflow the student can always drive by hand, not a gatekeeper the student must go through.
- The `--allow-gate-failure` / `results/diagnostic/` split in `EXPERIMENTS.md` is a tooling-level analog of the same principle applied to data: a fast, possibly-broken diagnostic run is physically routed somewhere it can't be confused with a trusted measurement.
