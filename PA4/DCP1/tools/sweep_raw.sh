#!/bin/bash
# DCP1/tools/sweep_raw.sh --out <csv> "<thread list>" <reps> <V> [<V> ...] [-- binary args...]
#
# Like run_sweep.sh, but keeps every repetition: appends one csv row per rep
# to <csv> (header written if the file is new):
#   threads,rep,seconds,gflops,dA,dB,binary,build_params,date
#
# Each variant is built (make -s <V>) and gated (.check, then .check-par,
# bound as run_sweep.sh --debug binds them) before any timing; a gate
# failure stops the script. With several variants the runs alternate:
# for each thread count, every variant in the order given.
#
#   DCP1/tools/sweep_raw.sh --out results/DCP1/raw.csv "1 2 4 8 16" 5 DCP1 DCP1T3
#
# Binding and environment copied from run_sweep.sh: a thread list of exactly
# "1" runs PolyMult_<V>.time pinned to BPM_SEQ_CORE; anything else runs
# PolyMult_<V>.time-par under taskset BPM_CORESET, OMP_PROC_BIND=close,
# OMP_PLACES=BPM_OMP_PLACES. Same defaults and overrides.

usage() { sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'; }

if [ $# -lt 1 ] || [ "$1" = "-h" ] || [ "$1" = "--help" ]; then usage; exit 0; fi
if [ "$1" != "--out" ] || [ $# -lt 5 ]; then usage >&2; exit 1; fi

OUT="$2"
THREADS="$3"
REPS="$4"
shift 4
VARIANTS=()
while [ $# -gt 0 ] && [ "$1" != "--" ]; do VARIANTS+=("$1"); shift; done
[ "${1:-}" = "--" ] && shift   # whatever is left is passed on to the binary

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

for V in "${VARIANTS[@]}"; do
    src="PolyMult_$V.c"
    [ -f "$src" ] || { echo "sweep_raw.sh: $src not found" >&2; exit 1; }

    # A wrapper variant (#include "PolyMult_X.c") is not rebuilt by the
    # Makefile when only the included file changes; touch it so it is.
    for inc in $(sed -n 's/^#include "\(PolyMult_[A-Za-z0-9_]*\.c\)".*/\1/p' "$src"); do
        if [ "$inc" -nt "bin/PolyMult_$V.$SUFFIX" ]; then touch "$src"; fi
    done

    make -s "$V" || { echo "sweep_raw.sh: build of $V failed" >&2; exit 1; }

    echo "== Correctness gate: PolyMult_$V.check =="
    seq_run "./bin/PolyMult_$V.check" | grep -E "suite|FAIL"
    [ "${PIPESTATUS[0]}" -eq 0 ] || { echo "Correctness gate FAILED ($V)"; exit 1; }
    echo "== Correctness gate: PolyMult_$V.check-par =="
    par_run "./bin/PolyMult_$V.check-par" | grep -E "suite|FAIL"
    [ "${PIPESTATUS[0]}" -eq 0 ] || { echo "Correctness gate FAILED ($V)"; exit 1; }
done
echo "Correctness gate PASSED"

mkdir -p "$(dirname "$OUT")"
[ -s "$OUT" ] || echo "threads,rep,seconds,gflops,dA,dB,binary,build_params,date" > "$OUT"

for t in $THREADS; do
    for V in "${VARIANTS[@]}"; do
        BIN="PolyMult_$V.$SUFFIX"
        DATE=$(date -Iseconds)
        # binary csv: variant,dA,dB,threads,...,"build_params",rep,seconds,gflops
        # build_params may contain commas, so rep/seconds/gflops come from the end.
        ROWS=$($RUN "./bin/$BIN" -t "$t" -r "$REPS" "$@" | tail -n +2)
        if [ -z "$ROWS" ]; then
            echo "sweep_raw.sh: no output from $BIN at threads=$t" >&2
            exit 1
        fi
        printf '%s\n' "$ROWS" | awk -F, -v bin="$BIN" -v date="$DATE" '{
            match($0, /"[^"]*"/); bp = substr($0, RSTART, RLENGTH)
            printf "%s,%s,%s,%s,%s,%s,%s,%s,%s\n", $4, $(NF-2), $(NF-1), $NF, $2, $3, bin, bp, date
        }' >> "$OUT"
        echo "$V threads=$t: $REPS rep(s) saved"
    done
done
echo "Saved to $OUT"
