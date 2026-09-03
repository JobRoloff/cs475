#!/bin/bash
#
# sweep.sh - gather Mandelbrot timing data across a range of thread counts.
# Writes one CSV row per data point: a leading "SEQ" row (bin/Mandelbrot.time,
# no threading), followed by rows for p = pmin..pmax step stride
# (bin/Mandelbrot.time-par). Each row: label,rep1,rep2,...,repN
#
# Usage:
#   ./bin/sweep.sh [-pmin p] [-pmax p] [-stride s] [-r reps] [-o outfile] [passthrough args...]
#
#   -pmin p     lowest thread count to test           (default 1)
#   -pmax p     highest thread count to test           (default 36)
#   -stride s   step between thread counts              (default 1)
#   -r reps     number of repetitions per data point      (default 5)
#   -o outfile  CSV file to write                          (default sweep_results.csv)
#   --help      print this usage message and exit
#
# Any argument not recognized above is forwarded, as-is, to every invocation
# of Mandelbrot.time / Mandelbrot.time-par -- e.g.:
#   ./bin/sweep.sh -pmin 1 -pmax 8 -stride 1 -r 3 -w 1200 -h 1200 -i 20000
# forwards "-w 1200 -h 1200 -i 20000" straight through to the binaries. This
# is also how you'd probe a small range first (e.g. -pmin 1 -pmax 8) before
# committing to a full sweep.
#
# Note: -h is deliberately NOT sweep.sh's help flag -- it's reserved as
# passthrough for the binaries' HEIGHT flag (see -w/-h/-i above). Use --help
# for sweep.sh's own usage message.
#
# Do not pass -v (visualization) through here: it would write to the same
# file on every single trial, overwriting it each time for no benefit. Run
# ./bin/Mandelbrot.time -v file.ppm directly instead, once, outside a sweep.
#
# All flags are optional; fewer than all of them is fine, in any order.
#
# Must be run from the project root (the directory containing bin/).

set -u

pmin=1
pmax=36
stride=1
reps=5
outfile=sweep_results.csv
passthrough=()

usage() {
    sed -n '2,35p' "$0" | sed 's/^#//'
}

while [ $# -gt 0 ]; do
    case "$1" in
        --help)
            usage
            exit 0
            ;;
        -pmin)
            pmin="$2"
            shift 2
            ;;
        -pmax)
            pmax="$2"
            shift 2
            ;;
        -stride)
            stride="$2"
            shift 2
            ;;
        -r)
            reps="$2"
            shift 2
            ;;
        -o)
            outfile="$2"
            shift 2
            ;;
        *)
            passthrough+=("$1")
            shift
            ;;
    esac
done

if [ ! -x ./bin/Mandelbrot.time ]; then
    echo "Error: ./bin/Mandelbrot.time not found or not executable."
    echo "Run this script from the project root, after 'make all'."
    exit 1
fi
if [ ! -x ./bin/Mandelbrot.time-par ]; then
    echo "Error: ./bin/Mandelbrot.time-par not found or not executable."
    echo "Run this script from the project root, after 'make all'."
    exit 1
fi

# Build header: label,rep1,rep2,...,repN
header="p"
for ((r = 1; r <= reps; r++)); do
    header="${header},rep${r}"
done
echo "$header" > "$outfile"

# --- SEQ row (bin/Mandelbrot.time, no OMP_NUM_THREADS) ---
row="SEQ"
for ((r = 1; r <= reps; r++)); do
    result=$(./bin/Mandelbrot.time "${passthrough[@]}" | grep Current | awk '{print $5}')
    row="${row},${result}"
done
echo "$row" >> "$outfile"

# --- p = pmin..pmax step stride rows (bin/Mandelbrot.time-par) ---
for ((p = pmin; p <= pmax; p += stride)); do
    row="$p"
    for ((r = 1; r <= reps; r++)); do
        result=$(OMP_NUM_THREADS=$p ./bin/Mandelbrot.time-par "${passthrough[@]}" | grep Current | awk '{print $5}')
        row="${row},${result}"
    done
    echo "$row" >> "$outfile"
done

echo "Done. Results written to $outfile"
