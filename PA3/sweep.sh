#!/bin/bash
# sweep.sh -- run one thread-count sweep for a StudentPolyMult variant and
# write one results csv (preamble of constant parameters, then one row per
# thread count) via collect.py.
#
# Every parallel invocation -- gate and timed run alike -- is bound (see
# _gate.sh): a parallel binary is never invoked bare.
#
# A compile-time knob (schedule clause, unroll factor, ...) needs its own
# rebuild per value -- pass it via one or more --make-var KEY=VALUE, forwarded
# to `make` before the gate/timed run. This script does not sweep such a knob
# itself: call it once per value, each to its own output.
#
# This never launches its own sequential baseline run to fill in a missing
# --seq-time on its own initiative.
#
# --allow-gate-failure lets a deliberately-incorrect build (e.g. a diagnostic
# nowait) still be timed after a failing gate, instead of the normal hard
# refusal -- the csv lands under results/diagnostic/ and records
# gate_status=PASSED|FAILED in its preamble.
#
# T(seq) carry-through: --record-seq (a --threads 1 call, no --make-var)
# writes the measured value to <variant-dir>/results/seq_time.json -- the
# one explicit, deliberate measurement that establishes it. Every later
# sweep.sh call for that variant then picks it up automatically (combined
# with that call's own t=1 row via min(), per the speedup-baseline rule)
# unless --seq-time overrides it. See EXPERIMENTS.md.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=_gate.sh
source "$SCRIPT_DIR/_gate.sh"

usage() {
    cat <<'EOF'
Usage: sweep.sh --variant-dir DIR --dA N --dB N --threads RANGE --reps N
                 (--out FILE.csv | --name NAME)
                 [--make-var KEY=VALUE ...]
                 [--extra KEY=VALUE ...]
                 [--seq-time SECONDS [--seq-source TEXT] | --record-seq | --renew-seq]
                 [--allow-gate-failure]

Rebuilds the variant (with any --make-var overrides), runs its correctness
gate under the project's bound-thread protocol, runs the timed sweep across
the given thread range, and writes one csv.

A sweep's raw data belongs to its variant: with --name (not --out), the csv
is written to <variant-dir>/results/<name>.csv automatically. --out is an
explicit override for a different location.

Required:
  --variant-dir DIR      variant directory containing a Makefile
  --dA N                 degree of polynomial A
  --dB N                 degree of polynomial B
  --threads RANGE        enumeration (1,2,4), stride (2:16:2), or powers
                          of 2 (1...16)
  --reps N                repetitions per thread count
  --name NAME             output filename stem; writes to
                          <variant-dir>/results/<name>.csv
  --out FILE.csv          explicit output csv path (overrides --name)

Optional:
  --make-var KEY=VALUE    forwarded to `make` before building, e.g.
                           --make-var SCHED=dynamic,8 ; repeatable. One
                           rebuild covers every thread count in this call --
                           to compare values, call sweep.sh once per value.
  --extra KEY=VALUE       recorded verbatim in the output csv's preamble
                           (e.g. --extra unroll=4, for a knob held constant
                           this call); repeatable; passed through to
                           collect.py

Sequential baseline (needed for plot.py to compute speedup from this csv):
  --seq-time SECONDS       T(1), the sequential baseline execution time.
                            Overrides any recorded seq_time.json. sweep.sh
                            never measures this on its own initiative.
  --seq-source TEXT         provenance note for --seq-time.
  --record-seq              this call IS the sequential-baseline
                            measurement for a (dA, dB) never recorded
                            before: requires --threads 1 and no
                            --make-var, adds/updates just this size's
                            entry in <variant-dir>/results/seq_time.json --
                            every other recorded size is left alone. Every
                            later sweep.sh call at this same size picks it
                            up automatically.
  --renew-seq               same measurement, same requirements, but
                            wipes every other recorded size first, keeping
                            only this one. Use this -- and only this --
                            when the variant's sequential kernel source
                            changed: every previously recorded size, not
                            just the current one, is now stale.

If neither --seq-time, --record-seq, nor --renew-seq is given, a matching
entry in seq_time.json (same variant, same dA/dB) is used automatically,
combined with this call's own t=1 row via min() when the thread range
includes 1. A sweep at a (dA, dB) with no matching entry gets no baseline
at all.

  --allow-gate-failure      time the build even if its correctness gate
                            fails -- for deliberately-incorrect diagnostic
                            runs (e.g. a nowait that should race). The gate
                            still runs and its outcome is recorded
                            (--extra gate_status=PASSED|FAILED); the csv
                            lands under <variant-dir>/results/diagnostic/
                            instead of results/, and this can never be
                            combined with --record-seq/--renew-seq (a
                            diagnostic build must never become the trusted
                            sequential baseline).

  -h, --help                this message
EOF
}

VARIANT_DIR=""
DA=""
DB=""
THREADS=""
REPS=""
OUT=""
NAME=""
SEQ_TIME=""
SEQ_SOURCE=""
RECORD_SEQ=0
RENEW_SEQ=0
ALLOW_GATE_FAILURE=0
MAKE_VARS=()
EXTRAS=()

while [ $# -gt 0 ]; do
    case "$1" in
        -h|--help)          usage; exit 0 ;;
        --variant-dir)       VARIANT_DIR="$2"; shift 2 ;;
        --dA)                DA="$2"; shift 2 ;;
        --dB)                DB="$2"; shift 2 ;;
        --threads)           THREADS="$2"; shift 2 ;;
        --reps)              REPS="$2"; shift 2 ;;
        --out)               OUT="$2"; shift 2 ;;
        --name)               NAME="$2"; shift 2 ;;
        --make-var)           MAKE_VARS+=("$2"); shift 2 ;;
        --extra)              EXTRAS+=("$2"); shift 2 ;;
        --seq-time)          SEQ_TIME="$2"; shift 2 ;;
        --seq-source)        SEQ_SOURCE="$2"; shift 2 ;;
        --record-seq)         RECORD_SEQ=1; shift ;;
        --renew-seq)           RENEW_SEQ=1; shift ;;
        --allow-gate-failure)  ALLOW_GATE_FAILURE=1; shift ;;
        *) echo "Unrecognized argument: $1" >&2; usage; exit 1 ;;
    esac
done

for req in VARIANT_DIR DA DB THREADS REPS; do
    if [ -z "${!req}" ]; then
        echo "Missing required argument: --$(echo "$req" | tr 'A-Z' 'a-z')" >&2
        exit 1
    fi
done
if [ -z "$OUT" ] && [ -z "$NAME" ]; then
    echo "Missing required argument: --out or --name" >&2
    exit 1
fi
if [ -n "$SEQ_SOURCE" ] && [ -z "$SEQ_TIME" ]; then
    echo "--seq-source given without --seq-time" >&2
    exit 1
fi
if [ "$RECORD_SEQ" -eq 1 ] && [ "$RENEW_SEQ" -eq 1 ]; then
    echo "--record-seq and --renew-seq are mutually exclusive" >&2
    exit 1
fi
if [ "$ALLOW_GATE_FAILURE" -eq 1 ] && { [ "$RECORD_SEQ" -eq 1 ] || [ "$RENEW_SEQ" -eq 1 ]; }; then
    echo "--allow-gate-failure cannot be combined with --record-seq/--renew-seq (a diagnostic build must never become the trusted sequential baseline)" >&2
    exit 1
fi
if [ "$RECORD_SEQ" -eq 1 ] || [ "$RENEW_SEQ" -eq 1 ]; then
    FLAG_NAME="--record-seq"
    [ "$RENEW_SEQ" -eq 1 ] && FLAG_NAME="--renew-seq"
    if [ "$THREADS" != "1" ]; then
        echo "$FLAG_NAME requires --threads 1 (it must be the true sequential binary)" >&2
        exit 1
    fi
    if [ ${#MAKE_VARS[@]} -ne 0 ]; then
        echo "$FLAG_NAME cannot be combined with --make-var (T(seq) must be schedule-independent)" >&2
        exit 1
    fi
    if [ -n "$SEQ_TIME" ]; then
        echo "$FLAG_NAME cannot be combined with --seq-time" >&2
        exit 1
    fi
fi

VARIANT_DIR="$(cd "$VARIANT_DIR" && pwd)"
VARIANT="$(basename "$VARIANT_DIR" | sed 's/^[0-9-]*//')"
SEQ_TIME_JSON="$VARIANT_DIR/results/seq_time.json"

if [ -z "$OUT" ]; then
    if [ "$ALLOW_GATE_FAILURE" -eq 1 ]; then
        mkdir -p "$VARIANT_DIR/results/diagnostic"
        OUT="$VARIANT_DIR/results/diagnostic/$NAME.csv"
    else
        mkdir -p "$VARIANT_DIR/results"
        OUT="$VARIANT_DIR/results/$NAME.csv"
    fi
fi

bpm_build "$VARIANT_DIR" "${MAKE_VARS[@]}" || { echo "Build failed"; exit 1; }

USE_PAR=0
if [ "$THREADS" != "1" ]; then
    USE_PAR=1
fi

GATE_STATUS="PASSED"
if [ "$ALLOW_GATE_FAILURE" -eq 1 ]; then
    bpm_gate_check "$VARIANT_DIR" || GATE_STATUS="FAILED"
    if [ "$USE_PAR" -eq 1 ]; then
        bpm_gate_check_par "$VARIANT_DIR" || GATE_STATUS="FAILED"
    fi
    if [ "$GATE_STATUS" = "FAILED" ]; then
        echo "== Gate FAILED -- proceeding anyway (--allow-gate-failure): this is a deliberately-diagnostic run, not a trusted measurement =="
    fi
else
    bpm_gate_check "$VARIANT_DIR" || { echo "Gate FAILED"; exit 1; }
    if [ "$USE_PAR" -eq 1 ]; then
        bpm_gate_check_par "$VARIANT_DIR" || { echo "Gate FAILED"; exit 1; }
    fi
fi

BINARY="$VARIANT_DIR/bin/PolyMult.time"
[ "$USE_PAR" -eq 1 ] && BINARY="$VARIANT_DIR/bin/PolyMult.time-par"
if [ ! -x "$BINARY" ]; then
    echo "Expected binary not found: $BINARY" >&2
    exit 1
fi

TIME_ARGS=(-dA "$DA" -dB "$DB" -t "$THREADS" -r "$REPS")

echo "== Timed sweep (bound): $BINARY ${TIME_ARGS[*]} =="
RAW_CSV="$(mktemp)"
if [ "$USE_PAR" -eq 1 ]; then
    OMP_PROC_BIND=close OMP_PLACES="$BPM_OMP_PLACES" taskset -c "$BPM_CORESET" "$BINARY" "${TIME_ARGS[@]}" > "$RAW_CSV"
else
    taskset -c "$BPM_SEQ_CORE" "$BINARY" "${TIME_ARGS[@]}" > "$RAW_CSV"
fi
if [ $? -ne 0 ]; then
    echo "Timed run failed" >&2
    rm -f "$RAW_CSV"
    exit 1
fi

if [ "$RECORD_SEQ" -eq 1 ]; then
    echo "== Recording T(seq) to $SEQ_TIME_JSON (dA=$DA dB=$DB) =="
    SEQ_TIME="$(python3 "$SCRIPT_DIR/_seq_time.py" record "$RAW_CSV" "$SEQ_TIME_JSON" "$OUT" "$DA" "$DB")" \
        || { rm -f "$RAW_CSV"; exit 1; }
    SEQ_SOURCE="recorded via --record-seq ($OUT)"
elif [ "$RENEW_SEQ" -eq 1 ]; then
    echo "== Renewing T(seq) at $SEQ_TIME_JSON (dA=$DA dB=$DB, wiping other sizes) =="
    SEQ_TIME="$(python3 "$SCRIPT_DIR/_seq_time.py" renew "$RAW_CSV" "$SEQ_TIME_JSON" "$OUT" "$DA" "$DB")" \
        || { rm -f "$RAW_CSV"; exit 1; }
    SEQ_SOURCE="recorded via --renew-seq ($OUT)"
elif [ -z "$SEQ_TIME" ]; then
    echo "== Resolving T(seq) for $VARIANT_DIR (dA=$DA dB=$DB) =="
    RESOLVED="$(python3 "$SCRIPT_DIR/_seq_time.py" resolve "$RAW_CSV" "$VARIANT_DIR" "$DA" "$DB")" \
        || { rm -f "$RAW_CSV"; exit 1; }
    SEQ_TIME="$(printf '%s\n' "$RESOLVED" | sed -n '1p')"
    SEQ_SOURCE="$(printf '%s\n' "$RESOLVED" | sed -n '2p')"
    if [ -z "$SEQ_TIME" ]; then
        echo "   (no recorded T(seq) for dA=$DA dB=$DB -- run --record-seq here or in this variant's *Q ancestor first)"
    fi
fi

echo "== Collecting into $OUT =="
COLLECT_ARGS=(--raw-csv "$RAW_CSV" --variant "$VARIANT" --dA "$DA" --dB "$DB" --out "$OUT")
for kv in "${EXTRAS[@]}"; do
    COLLECT_ARGS+=(--extra "$kv")
done
if [ "$ALLOW_GATE_FAILURE" -eq 1 ]; then
    COLLECT_ARGS+=(--extra "gate_status=$GATE_STATUS")
fi
for kv in "${MAKE_VARS[@]}"; do
    case "$kv" in
        SCHED=*) COLLECT_ARGS+=(--sched "${kv#SCHED=}") ;;
    esac
done
if [ -n "$SEQ_TIME" ]; then
    COLLECT_ARGS+=(--seq-time "$SEQ_TIME")
    [ -n "$SEQ_SOURCE" ] && COLLECT_ARGS+=(--seq-source "$SEQ_SOURCE")
fi

python3 "$SCRIPT_DIR/collect.py" "${COLLECT_ARGS[@]}"
STATUS=$?
rm -f "$RAW_CSV"
exit $STATUS
