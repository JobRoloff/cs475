#!/usr/bin/env python3
"""Turn one raw per-rep timing CSV (as emitted by PolyMult.time[-par]) into
one StudentPolyMult results csv: a preamble of parameters constant for this
sweep, then one row per thread count.

Preamble: plain two-column csv rows (name,value), terminated by the row
whose first field is literally "threads" -- that row is the data table's
own header, everything after is data. No fixed preamble length, no blank
line needed: a new constant parameter is one more preamble line, nothing
else changes.

Data table columns: threads,rep1..repN,median,mean_wo_outliers,
gflops_median,gflops_mean_wo_outliers. GFLOPs/s is computed once per
aggregate stat, never per rep, using dA/dB (degrees, so a degree-d
polynomial has d+1 coefficients): flops = 2*(dA+1)*(dB+1).

Outlier rule: matches BestPolyMult's existing tools/collect_results.py --
a repetition more than 2x its thread-count group's fastest rep is excluded
from median/mean_wo_outliers (ratio-to-config-min, ratio=2.0). This is the
established, already-validated production rule; it supersedes an earlier,
unvalidated "drop exactly one slowest rep" phrasing that only ever existed
as draft text in PlottingScripts/Specs.md.
"""
import argparse
import csv
import statistics
import sys

OUTLIER_RATIO = 2.0


def read_raw(path):
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f))
    if not rows:
        raise SystemExit(f"error: {path}: no data rows")
    return rows


def group_by_threads(rows):
    groups = {}
    for r in rows:
        t = int(float(r["threads"]))
        groups.setdefault(t, []).append(float(r["seconds"]))
    return groups


def included_reps(seconds_list):
    fastest = min(seconds_list)
    limit = fastest * OUTLIER_RATIO
    return [s for s in seconds_list if s <= limit]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--raw-csv", required=True,
                     help="raw per-rep csv from PolyMult.time[-par]")
    ap.add_argument("--variant", required=True)
    ap.add_argument("--dA", required=True, type=int)
    ap.add_argument("--dB", required=True, type=int)
    ap.add_argument("--sched", default=None)
    ap.add_argument("--block-size", default=None, type=int)
    ap.add_argument("--leaf-size", default=None, type=int)
    ap.add_argument("--task-threshold", default=None, type=int)
    ap.add_argument("--seq-time", default=None, type=float,
                     help="T(1), the sequential baseline (seconds); "
                          "supplied by the caller, never discovered here")
    ap.add_argument("--seq-source", default=None,
                     help="provenance note/path for --seq-time")
    ap.add_argument("--extra", action="append", default=[], metavar="KEY=VALUE",
                     help="additional preamble field for a variant-specific "
                          "constant not covered by the flags above (e.g. "
                          "--extra unroll=4); repeatable")
    ap.add_argument("--out", required=True)
    args = ap.parse_args(argv)

    if args.seq_source and args.seq_time is None:
        ap.error("--seq-source given without --seq-time")

    extra_fields = []
    for item in args.extra:
        if "=" not in item:
            ap.error(f"--extra {item!r}: expected KEY=VALUE")
        k, v = item.split("=", 1)
        extra_fields.append((k, v))

    raw_rows = read_raw(args.raw_csv)
    groups = group_by_threads(raw_rows)
    threads_sorted = sorted(groups.keys())

    flop_count = 2.0 * (args.dA + 1) * (args.dB + 1)

    table_rows = []
    max_reps = 0
    for t in threads_sorted:
        secs = groups[t]
        included = included_reps(secs)
        median_s = statistics.median(included)
        mean_wo_s = statistics.fmean(included)
        gflops_median = flop_count / median_s / 1e9
        gflops_mean_wo = flop_count / mean_wo_s / 1e9
        table_rows.append({
            "threads": t,
            "reps": secs,
            "median": median_s,
            "mean_wo_outliers": mean_wo_s,
            "gflops_median": gflops_median,
            "gflops_mean_wo_outliers": gflops_mean_wo,
        })
        max_reps = max(max_reps, len(secs))

    with open(args.out, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["variant", args.variant])
        w.writerow(["dA", args.dA])
        w.writerow(["dB", args.dB])
        if args.sched is not None:
            w.writerow(["sched", args.sched])
        if args.block_size is not None:
            w.writerow(["block_size", args.block_size])
        if args.leaf_size is not None:
            w.writerow(["leaf_size", args.leaf_size])
        if args.task_threshold is not None:
            w.writerow(["task_threshold", args.task_threshold])
        for k, v in extra_fields:
            w.writerow([k, v])
        if args.seq_time is not None:
            w.writerow(["seq_time", args.seq_time])
            if args.seq_source:
                w.writerow(["seq_time_source", args.seq_source])

        header = ["threads"] + [f"rep{i+1}" for i in range(max_reps)] + \
            ["median", "mean_wo_outliers", "gflops_median", "gflops_mean_wo_outliers"]
        w.writerow(header)

        for row in table_rows:
            reps = row["reps"] + [""] * (max_reps - len(row["reps"]))
            w.writerow([row["threads"]] + reps + [
                row["median"], row["mean_wo_outliers"],
                row["gflops_median"], row["gflops_mean_wo_outliers"],
            ])

    print(f"wrote {args.out}: {len(threads_sorted)} thread-count row(s), "
          f"threads={threads_sorted}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
