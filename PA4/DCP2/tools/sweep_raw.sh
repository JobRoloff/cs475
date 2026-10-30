#!/bin/bash
# DCP2/tools/sweep_raw.sh --out <csv> "<thread list>" <reps> <V> "<args 1>" ["<args 2>" ...]
#
# DCP1/tools/sweep_raw.sh for one variant run with several argument sets
# (e.g. two --task-threshold values). Keeps every repetition: appends one
# csv row per rep to <csv> (header written if the file is new):
#   threads,rep,seconds,gflops,dA,dB,leaf_size,task_threshold,binary,build_params,args,date
#
# The variant is built (make -s <V>) and gated (.check, then .check-par,
# bound as run_sweep.sh --debug binds them) before any timing; a gate
# failure stops the script. The argument sets alternate: for each thread
# count, every set in the order given.
#
#   DCP2/tools/sweep_raw.sh --out results/DCP2/raw.csv "1 2 3" 5 DCP2 \
#       "--task-threshold 1" "--task-threshold 2"
#
# Binding and environment copied from run_sweep.sh: a thread list of exactly
# "1" runs PolyMult_<V>.time pinned to BPM_SEQ_CORE; anything else runs
# PolyMult_<V>.time-par under taskset BPM_CORESET, OMP_PROC_BIND=close,
# OMP_PLACES=BPM_OMP_PLACES. Same defaults and overrides.

usage() { sed -n '2,21p' "$0" | sed 's/^# \{0,1\}//'; }

if [ $# -lt 1 ] || [ "$1" = "-h" ] || [ "$1" = "--help" ]; then usage; exit 0; fi
if [ "$1" != "--out" ] || [ $# -lt 6 ]; then usage >&2; exit 1; fi

OUT="$2"
THREADS="$3"
REPS="$4"
V="$5"
shift 5
ARGSETS=("$@")

# --- copied from run_sweep.sh ---
: "${BPM_CORESET:=16-31}"
: "${BPM_SEQ_CORE:=16}"
if [ -z "${BPM_OMP_PLACES:-}" ]; then
    BPM_OMP_PLACES='{16},{20},{24},{28},{17},{21},{25},{29},{18},{22},{26},{30},{19},{23},{27},{31}'
fi
seq_run() { taskset -c "$BPM_SEQ_CORE" "$@"; }
par_run() { OMP_PROC_BIND=close OMP_PLACES="$BPM_OMP_PLACES" taskset -c "$BPM_CORESET" "$@"; }
# --- end copy ---

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
case "$OUT" in /*) ;; *) OUT="$PWD/$OUT" ;; esac
cd "$ROOT" || exit 1

if [ "$THREADS" = "1" ]; then
    RUN=seq_run; SUFFIX=time
else
    RUN=par_run; SUFFIX=time-par
fi

[ -f "PolyMult_$V.c" ] || { echo "sweep_raw.sh: PolyMult_$V.c not found" >&2; exit 1; }
make -s "$V" || { echo "sweep_raw.sh: build of $V failed" >&2; exit 1; }

echo "== Correctness gate: PolyMult_$V.check =="
seq_run "./bin/PolyMult_$V.check" | grep -E "suite|FAIL"
[ "${PIPESTATUS[0]}" -eq 0 ] || { echo "Correctness gate FAILED ($V)"; exit 1; }
echo "== Correctness gate: PolyMult_$V.check-par =="
par_run "./bin/PolyMult_$V.check-par" | grep -E "suite|FAIL"
[ "${PIPESTATUS[0]}" -eq 0 ] || { echo "Correctness gate FAILED ($V)"; exit 1; }
echo "Correctness gate PASSED"

mkdir -p "$(dirname "$OUT")"
[ -s "$OUT" ] || echo "threads,rep,seconds,gflops,dA,dB,leaf_size,task_threshold,binary,build_params,args,date" > "$OUT"

BIN="PolyMult_$V.$SUFFIX"
for t in $THREADS; do
    for A in "${ARGSETS[@]}"; do
        DATE=$(date -Iseconds)
        # binary csv: variant,dA,dB,threads,block_size,leaf_size,task_threshold,
        # "build_params",rep,seconds,gflops -- build_params may contain commas,
        # so rep/seconds/gflops come from the end.
        # shellcheck disable=SC2086  # $A is split into the binary's arguments
        ROWS=$($RUN "./bin/$BIN" -t "$t" -r "$REPS" $A | tail -n +2)
        if [ -z "$ROWS" ]; then
            echo "sweep_raw.sh: no output from $BIN $A at threads=$t" >&2
            exit 1
        fi
        printf '%s\n' "$ROWS" | awk -F, -v bin="$BIN" -v args="$A" -v date="$DATE" '{
            match($0, /"[^"]*"/); bp = substr($0, RSTART, RLENGTH)
            printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,\"%s\",%s\n", $4, $(NF-2), $(NF-1), $NF, $2, $3, $6, $7, bin, bp, args, date
        }' >> "$OUT"
        echo "$V $A threads=$t: $REPS rep(s) saved"
    done
done
echo "Saved to $OUT"
