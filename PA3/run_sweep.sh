#!/bin/bash
# run_sweep.sh [--debug] VARIANT "THREAD LIST" REPS [ARGS...]
#
# Builds VARIANT (PolyMult_VARIANT.c) and prints the median time and GFLOP/s
# for each thread count. Any ARGS after REPS are passed on to the binary.
#
#   ./run_sweep.sh OPQ "1" 5 -dA 2000 -dB 2000
#   ./run_sweep.sh OPQ "1 2 4 8 16" 5 -dA 2000 -dB 2000
#   ./run_sweep.sh --debug OPQ "1 2 4" 3 -dA 2000 -dB 2000
#
# --debug runs the correctness gate (check and check-par) before timing and
# stops if it fails. Without it, only the timing binary runs.
#
# A thread list of exactly "1" runs the sequential (no -fopenmp) binary pinned
# to one core; anything else runs the parallel binary, bound. Override the
# binding with BPM_CORESET / BPM_SEQ_CORE / BPM_OMP_PLACES.

if [ $# -lt 1 ] || [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    sed -n '2,17p' "$0" | sed 's/^# \{0,1\}//'
    exit 0
fi

DEBUG=0
if [ "$1" = "--debug" ]; then DEBUG=1; shift; fi

VARIANT="$1"
THREADS="${2:-1}"
REPS="${3:-5}"
shift $(( $# < 3 ? $# : 3 ))   # whatever is left is passed on to the binary

: "${BPM_CORESET:=16-31}"
: "${BPM_SEQ_CORE:=16}"
# ${VAR:=default} stops at the first unescaped }, truncating a value like
# this one, so use a plain conditional assignment instead.
if [ -z "${BPM_OMP_PLACES:-}" ]; then
    BPM_OMP_PLACES='{16},{20},{24},{28},{17},{21},{25},{29},{18},{22},{26},{30},{19},{23},{27},{31}'
fi

cd "$(dirname "${BASH_SOURCE[0]}")" || exit 1

if [ ! -f "PolyMult_$VARIANT.c" ]; then
    echo "run_sweep.sh: PolyMult_$VARIANT.c not found" >&2
    exit 1
fi

make -s "$VARIANT" || { echo "run_sweep.sh: build failed" >&2; exit 1; }

seq_run() { taskset -c "$BPM_SEQ_CORE" "$@"; }
par_run() { OMP_PROC_BIND=close OMP_PLACES="$BPM_OMP_PLACES" taskset -c "$BPM_CORESET" "$@"; }

if [ "$DEBUG" -eq 1 ]; then
    echo "== Correctness gate: PolyMult_$VARIANT.check =="
    seq_run "./bin/PolyMult_$VARIANT.check" || { echo "Correctness gate FAILED"; exit 1; }
    echo "== Correctness gate: PolyMult_$VARIANT.check-par =="
    par_run "./bin/PolyMult_$VARIANT.check-par" || { echo "Correctness gate FAILED"; exit 1; }
    echo "Correctness gate PASSED"
    echo
fi

if [ "$THREADS" = "1" ]; then
    RUN=seq_run; BIN="./bin/PolyMult_$VARIANT.time"
else
    RUN=par_run; BIN="./bin/PolyMult_$VARIANT.time-par"
fi

median() { sort -g | awk '{v[NR]=$1} END{print (NR%2) ? v[(NR+1)/2] : (v[NR/2]+v[NR/2+1])/2}'; }

printf '%-8s %-14s %-12s %s\n' threads median_s gflops reps
for t in $THREADS; do
    # csv: ...,threads,...,"build_params",rep,seconds,gflops -- build_params
    # may contain commas, so seconds and gflops are read from the end.
    ROWS=$($RUN "$BIN" -t "$t" -r "$REPS" "$@" | tail -n +2)
    if [ -z "$ROWS" ]; then
        echo "run_sweep.sh: no output from $BIN at threads=$t" >&2
        exit 1
    fi
    SECS=$(printf '%s\n' "$ROWS" | awk -F, '{print $(NF-1)}' | median)
    GFLOPS=$(printf '%s\n' "$ROWS" | awk -F, '{print $NF}' | median)
    printf '%-8s %-14s %-12s %s\n' "$t" "$SECS" "$GFLOPS" "$REPS"
done
