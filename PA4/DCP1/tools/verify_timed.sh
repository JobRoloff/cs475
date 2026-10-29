#!/bin/bash
# DCP1/tools/verify_timed.sh "<thread list>" [build list]
#
# Builds verify_timed.c against PolyMult_DCP1.c with the .time-par flags
# (one binary per build, into bin/verify_timed_tred<N>[_spawn<S>]) and runs
# each at every thread count, bound as run_sweep.sh binds a parallel run.
# A build is a TRED value, TRED/SPAWN (e.g. 3/2), or a comma-separated list
# of switches (e.g. TRED=3,NOTG=1); default "1 3".
#
#   DCP1/tools/verify_timed.sh "1 3 16"          # TRED=1 and TRED=3
#   DCP1/tools/verify_timed.sh "16" "1"          # TRED=1 only
#   DCP1/tools/verify_timed.sh "1 3 16" "3/2 3/3"
#   DCP1/tools/verify_timed.sh "1 3 16" "TRED=3,NOTG=1 ROOTDIRECT=1"
#
# Exit 0 only if every run passed.

if [ $# -lt 1 ] || [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'
    exit 0
fi

THREADS="$1"
TREDS="${2:-1 3}"

# Binding copied from run_sweep.sh (par_run), same defaults and overrides.
: "${BPM_CORESET:=16-31}"
if [ -z "${BPM_OMP_PLACES:-}" ]; then
    BPM_OMP_PLACES='{16},{20},{24},{28},{17},{21},{25},{29},{18},{22},{26},{30},{19},{23},{27},{31}'
fi
par_run() { OMP_PROC_BIND=close OMP_PLACES="$BPM_OMP_PLACES" taskset -c "$BPM_CORESET" "$@"; }

cd "$(dirname "${BASH_SOURCE[0]}")/../.." || exit 1
mkdir -p bin

# Same flags as the Makefile's bin/PolyMult_%.time-par rule.
CFLAGS="-std=gnu11 -O3 -march=native -Ishared/include -Ishared/harness -ffast-math -DPAR -fopenmp"

status=0
for build in $TREDS; do
    case "$build" in
        *=*)
            defs=""
            for kv in ${build//,/ }; do defs="$defs -D$kv"; done
            label="$build"
            bin="bin/verify_timed_$(echo "$build" | tr 'A-Z=,' 'a-z__')"
            ;;
        *)
            tred="${build%%/*}"
            spawn=1
            [ "$build" != "$tred" ] && spawn="${build#*/}"
            defs="-DTRED=$tred -DSPAWN=$spawn"
            label="TRED=$tred SPAWN=$spawn"
            bin="bin/verify_timed_tred$tred"
            [ "$spawn" != 1 ] && bin="${bin}_spawn$spawn"
            ;;
    esac
    gcc $CFLAGS $defs PolyMult_DCP1.c DCP1/tools/verify_timed.c -o "$bin" -lm \
        || { echo "verify_timed.sh: build failed for $label" >&2; exit 1; }
    for t in $THREADS; do
        echo "== $label threads=$t =="
        OMP_NUM_THREADS="$t" par_run "./$bin" || status=1
    done
done
exit $status
