#!/bin/bash
# run_sweep.sh BINARY "THREAD LIST" REPS
#
# Runs BINARY once per repetition at each thread count and prints the median
# run time for each count.
#
#   ./run_sweep.sh ./Merge_sort "1" 5
#   ./run_sweep.sh ./Merge_sortP "1 2 3 4 5 6 7 8" 5
#
# The thread count is passed to the binary through OMP_NUM_THREADS. The
# sequential Merge_sort ignores it, so use "1" there.

if [ $# -lt 1 ] || [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
    exit 0
fi

BIN="$1"
THREADS="${2:-1}"
REPS="${3:-5}"

case "$BIN" in */*) ;; *) BIN="./$BIN" ;; esac

if [ ! -x "$BIN" ]; then
    echo "run_sweep.sh: $BIN not found or not executable (run make first)" >&2
    exit 1
fi

printf '%-8s %-12s %s\n' threads median reps
for t in $THREADS; do
    TIMES=""
    for ((rep=1; rep<=REPS; rep++)); do
        OUT=$(OMP_NUM_THREADS="$t" "$BIN"); RC=$?
        SECS=$(printf '%s\n' "$OUT" | sed -n 's/^elapsed time = \([0-9.]*\).*/\1/p')
        if [ "$RC" -ne 0 ] || printf '%s\n' "$OUT" | grep -q '^FAILED'; then
            printf '%s\n' "$OUT" | grep '^FAILED' >&2
            echo "run_sweep.sh: $BIN failed its correctness check at threads=$t rep=$rep" >&2
            exit 1
        fi
        if [ -z "$SECS" ]; then
            echo "run_sweep.sh: no 'elapsed time' line from $BIN at threads=$t rep=$rep" >&2
            exit 1
        fi
        TIMES="$TIMES$SECS"$'\n'
    done
    MEDIAN=$(printf '%s' "$TIMES" | sort -g | awk '{v[NR]=$1} END{print (NR%2) ? v[(NR+1)/2] : (v[NR/2]+v[NR/2+1])/2}')
    printf '%-8s %-12s %s\n' "$t" "$MEDIAN" "$REPS"
done
