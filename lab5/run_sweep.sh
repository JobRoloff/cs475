#!/bin/bash
# run_sweep.sh BINARY "THREAD LIST" REPS [ARGS...]
#
# Runs BINARY once per repetition at each thread count and prints the median
# run time for each count. Any ARGS after REPS are passed on to BINARY.
#
#   ./run_sweep.sh ./Fib_count "1" 5 40
#   ./run_sweep.sh ./Fib_count_red_cutoff "1 2 3 4 5 6 7 8" 5 40 25
#
# The thread count is passed to the binary through OMP_NUM_THREADS. The
# sequential Fib_count ignores it, so use "1" there.

if [ $# -lt 1 ] || [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
    exit 0
fi

BIN="$1"
THREADS="${2:-1}"
REPS="${3:-5}"
shift $(( $# < 3 ? $# : 3 ))   # whatever is left is passed on to BINARY

case "$BIN" in */*) ;; *) BIN="./$BIN" ;; esac

if [ ! -x "$BIN" ]; then
    echo "run_sweep.sh: $BIN not found or not executable (run make first)" >&2
    exit 1
fi

printf '%-8s %-12s %s\n' threads median reps
for t in $THREADS; do
    TIMES=""
    for ((rep=1; rep<=REPS; rep++)); do
        SECS=$(OMP_NUM_THREADS="$t" "$BIN" "$@" | sed -n 's/^elapsed time = \([0-9.]*\).*/\1/p')
        if [ -z "$SECS" ]; then
            echo "run_sweep.sh: no 'elapsed time' line from $BIN at threads=$t rep=$rep" >&2
            exit 1
        fi
        TIMES="$TIMES$SECS"$'\n'
    done
    MEDIAN=$(printf '%s' "$TIMES" | sort -g | awk '{v[NR]=$1} END{print (NR%2) ? v[(NR+1)/2] : (v[NR/2]+v[NR/2+1])/2}')
    printf '%-8s %-12s %s\n' "$t" "$MEDIAN" "$REPS"
done
