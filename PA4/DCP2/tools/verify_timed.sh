#!/bin/bash
# DCP2/tools/verify_timed.sh "<thread list>" [variant]
#
# Builds DCP1/tools/verify_timed.c against PolyMult_<variant>.c (default
# DCP2; e.g. DCP2HD) with the .time-par flags (into
# bin/verify_timed_<variant, lower case>) and runs it at every thread count,
# bound as run_sweep.sh binds a parallel run. The kernel runs with its
# defaults (leaf 1280, --task-threshold 1).
#
#   DCP2/tools/verify_timed.sh "1 3 16"
#   DCP2/tools/verify_timed.sh "1 3 16" DCP2HD
#
# Exit 0 only if every run passed.

if [ $# -lt 1 ] || [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'
    exit 0
fi

THREADS="$1"
V="${2:-DCP2}"

# Binding copied from run_sweep.sh (par_run), same defaults and overrides.
: "${BPM_CORESET:=16-31}"
if [ -z "${BPM_OMP_PLACES:-}" ]; then
    BPM_OMP_PLACES='{16},{20},{24},{28},{17},{21},{25},{29},{18},{22},{26},{30},{19},{23},{27},{31}'
fi
par_run() { OMP_PROC_BIND=close OMP_PLACES="$BPM_OMP_PLACES" taskset -c "$BPM_CORESET" "$@"; }

cd "$(dirname "${BASH_SOURCE[0]}")/../.." || exit 1
mkdir -p bin

[ -f "PolyMult_$V.c" ] || { echo "verify_timed.sh: PolyMult_$V.c not found" >&2; exit 1; }

# Same flags as the Makefile's bin/PolyMult_%.time-par rule.
CFLAGS="-std=gnu11 -O3 -march=native -Ishared/include -Ishared/harness -ffast-math -DPAR -fopenmp"
BIN="bin/verify_timed_$(echo "$V" | tr 'A-Z' 'a-z')"

gcc $CFLAGS "PolyMult_$V.c" DCP1/tools/verify_timed.c -o "$BIN" -lm \
    || { echo "verify_timed.sh: build failed" >&2; exit 1; }

status=0
for t in $THREADS; do
    echo "== $V threads=$t =="
    OMP_NUM_THREADS="$t" par_run "./$BIN" || status=1
done
exit $status
