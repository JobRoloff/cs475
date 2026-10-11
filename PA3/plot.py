#!/usr/bin/env python3
"""Turn one or more StudentPolyMult results csvs (collect.py's preamble +
per-thread-count table format) into an .xlsx workbook with native Speedup
and Performance charts.

Each --csv is one run/series. Multiple --csv values are superposed on the
Speedup chart (one line per run) and grouped side by side on the
Performance chart (one bar per run at each thread count) -- never stacked.

%TPP uses a thread-scaled denominator, so an ideal linear speedup reads as
a flat 100% line:

    %TPP(t) = GFLOPs/s(t) / (TPP_per_core * t) * 100

This is the corrected formula (matches BestPolyMult/tools/plot_results.py);
an earlier draft of this tool (PlottingScripts/plot.py) used a fixed,
non-thread-scaled denominator, which grows linearly with thread count under
ideal speedup instead of staying flat, and has not itself been fixed yet.

Speedup(t) = seq_time / stat_time(t), where seq_time is the csv preamble's
own T(1) (supplied by sweep.sh's T(seq) carry-through, see
EXPERIMENTS.md) and stat_time is the median or mean_wo_outliers
column, per --stat. A csv missing seq_time is refused (SystemExit) rather
than silently plotted without a baseline.

Output placement: if --out is not given, it's derived from where the input
csvs live. Every csv under the same NN-VARIANT directory (e.g. 00-GSQ) is a
single-variant plot and lands inside that variant's own tree:
<variant-dir>/plots/<topic>/workbook.xlsx. Csvs spanning more than one
variant directory are a cross-variant plot and land one level up, at the
common parent's plots/<topic>/workbook.xlsx -- e.g. two csvs under 00-GSQ
and 01-OPQ produce StudentPolyMult/plots/<topic>/workbook.xlsx. <topic> is
--topic if given, else a slugified --title. An explicit --out always wins.

Usage:
  plot.py --csv FILE [--csv FILE ...] --tpp-per-core VALUE
          [--out WORKBOOK.xlsx] [--topic SLUG] [--label NAME ...]
          [--tmp VALUE] [--stat median|mean_wo_outliers] [--title TITLE]
"""
import argparse
import csv
import os
import re
import sys

import xlsxwriter

STAT_CHOICES = ("median", "mean_wo_outliers")


def read_run_csv(path):
    """Returns (preamble: dict[str,str], table: dict[int, dict[str,float-or-None]])."""
    with open(path, newline="") as f:
        rows = list(csv.reader(f))

    preamble = {}
    i = 0
    header = None
    while i < len(rows):
        row = rows[i]
        if row and row[0] == "threads":
            header = row
            i += 1
            break
        if len(row) >= 2:
            preamble[row[0]] = row[1]
        i += 1
    if header is None:
        raise SystemExit(f"error: {path}: no 'threads' header row found "
                          f"(malformed results csv)")

    table = {}
    for row in rows[i:]:
        if not row or not row[0]:
            continue
        vals = dict(zip(header, row))
        t = int(float(vals["threads"]))
        entry = {}
        for key in ("median", "mean_wo_outliers", "gflops_median",
                    "gflops_mean_wo_outliers"):
            raw = vals.get(key, "")
            entry[key] = float(raw) if raw not in (None, "") else None
        table[t] = entry
    return preamble, table


def parse_args(argv):
    p = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--csv", action="append", required=True, dest="csvs",
                    help="input results csv (collect.py format); repeatable")
    p.add_argument("--label", action="append", default=[], dest="labels",
                    help="series label, paired positionally with --csv; "
                         "defaults to the csv's own 'variant' preamble value")
    p.add_argument("--tpp-per-core", required=True, type=float,
                    help="theoretical peak GFLOPs/s of one core; the "
                         "thread-scaled %%TPP denominator is this * threads")
    p.add_argument("--tmp", type=float, default=None,
                    help="theoretical machine peak GFLOPs/s; stored in the "
                         "Config tab only, not used in any chart")
    p.add_argument("--stat", choices=STAT_CHOICES, default="median",
                    help="which aggregate time feeds GFLOPs/s and speedup "
                         "(default: median)")
    p.add_argument("--out", default=None,
                    help="output .xlsx path; if omitted, derived "
                         "automatically (see module docstring)")
    p.add_argument("--topic", default=None,
                    help="topic slug for the auto-derived output path "
                         "(plots/<topic>/workbook.xlsx); defaults to a "
                         "slugified --title")
    p.add_argument("--title", default=None)
    args = p.parse_args(argv)
    if args.labels and len(args.labels) != len(args.csvs):
        p.error("--label, if given, must be given exactly once per --csv")
    if args.out is None and args.topic is None and args.title is None:
        p.error("--out requires either an explicit value, or --topic, or "
                 "--title to derive one from")
    return args


VARIANT_DIR_RE = re.compile(r"^\d{2}-[A-Za-z0-9]+$")


def slugify(text):
    s = re.sub(r"[^A-Za-z0-9]+", "-", text.strip()).strip("-")
    return s or "plot"


def find_variant_dir(csv_path):
    """Walk up from csv_path looking for an ancestor matching the project's
    NN-VARIANT directory convention (e.g. 00-GSP1, 01-OPQ). Returns that
    ancestor's absolute path, or None if csv_path isn't under one."""
    p = os.path.abspath(csv_path)
    d = os.path.dirname(p)
    while d and d != os.path.dirname(d):
        if VARIANT_DIR_RE.match(os.path.basename(d)):
            return d
        d = os.path.dirname(d)
    return None


def derive_out_path(csv_paths, topic):
    """Single-variant plot (every --csv under the same NN-VARIANT dir) goes
    inside that variant's own plots/ subdirectory; a plot spanning more than
    one variant dir goes one level up, at their common parent's plots/ --
    matching U-012's plots/<topic-slug>/workbook.xlsx shape at whichever
    level is appropriate. A csv not under any recognized variant dir can't
    be placed automatically."""
    variant_dirs = {find_variant_dir(c) for c in csv_paths}
    if None in variant_dirs:
        return None
    if len(variant_dirs) == 1:
        base_dir = next(iter(variant_dirs))
    else:
        parents = {os.path.dirname(d) for d in variant_dirs}
        if len(parents) != 1:
            return None
        base_dir = next(iter(parents))
    return os.path.join(base_dir, "plots", topic, "workbook.xlsx")


def qref(sheet, row, col):
    from xlsxwriter.utility import xl_rowcol_to_cell
    return f"'{sheet}'!{xl_rowcol_to_cell(row, col)}"


LINEBREAK_RUN_RE = re.compile(
    r"<a:r>(<a:rPr[^>]*/>)<a:t>([^<]*?)\n([^<]*?)</a:t></a:r>")


def fix_axis_title_linebreaks(xlsx_path):
    """xlsxwriter embeds a chart title's "\\n" as a literal newline character
    inside one <a:t> text run -- valid XML, but not how DrawingML represents
    a line break (that needs a real <a:br/> element between two runs), and
    not something every consumer renders as two lines. Post-process the
    written file to split any such run into two runs joined by <a:br/>,
    each carrying the same run properties as the original."""
    import shutil
    import tempfile
    import zipfile

    with zipfile.ZipFile(xlsx_path) as zin:
        names = zin.namelist()
        changed = {}
        for name in names:
            if not re.match(r"xl/charts/chart\d+\.xml", name):
                continue
            data = zin.read(name).decode("utf-8")
            new_data, n = LINEBREAK_RUN_RE.subn(
                r"<a:r>\1<a:t>\2</a:t></a:r><a:br>\1</a:br><a:r>\1<a:t>\3</a:t></a:r>",
                data)
            if n:
                changed[name] = new_data
        if not changed:
            return

    fd, tmp_path = tempfile.mkstemp(suffix=".xlsx", dir=os.path.dirname(xlsx_path) or ".")
    os.close(fd)
    with zipfile.ZipFile(xlsx_path) as zin, zipfile.ZipFile(tmp_path, "w", zipfile.ZIP_DEFLATED) as zout:
        for item in zin.infolist():
            data = changed.get(item.filename)
            if data is not None:
                zout.writestr(item, data.encode("utf-8"))
            else:
                zout.writestr(item, zin.read(item.filename))
    shutil.move(tmp_path, xlsx_path)


def main(argv=None):
    args = parse_args(sys.argv[1:] if argv is None else argv)
    time_key = args.stat
    gflops_key = "gflops_" + args.stat

    runs = []
    for i, path in enumerate(args.csvs):
        preamble, table = read_run_csv(path)
        label = args.labels[i] if args.labels else preamble.get("variant", f"run{i+1}")
        seq_time = preamble.get("seq_time")
        seq_time = float(seq_time) if seq_time not in (None, "") else None
        runs.append({
            "label": label, "path": path, "preamble": preamble,
            "table": table, "seq_time": seq_time,
        })
        if seq_time is None:
            raise SystemExit(
                f"error: {path}: no seq_time in preamble. Every sweep's "
                f"csv must carry a sequential baseline -- run "
                f"sweep.sh --record-seq once for this variant (see "
                f"EXPERIMENTS.md), which every later sweep then "
                f"picks up automatically.")

    # A bar/column chart's x-axis is always a category axis in Excel -- it
    # can never be a true numeric (value) axis, so bars for 1,2,4,8,16 would
    # otherwise sit evenly spaced regardless of their actual magnitude. Since
    # a category axis's own spacing IS linear across its category list, using
    # every integer from 1 to the largest swept thread count as the category
    # list (with unmeasured thread counts left as =NA(), so no bar/point is
    # drawn for them) makes the axis read as genuinely linear.
    measured_threads = sorted({t for r in runs for t in r["table"].keys()})
    all_threads = list(range(1, measured_threads[-1] + 1)) if measured_threads else []

    # Axis-cap rule: if every bar sits well below 100% TPP, showing the axis
    # out to 100 leaves the chart mostly white space. Cap the axis to just
    # above the tallest bar and drop the 100% reference (it would be
    # off-chart anyway) whenever the tallest bar is at or below 67% of TPP;
    # above that, always show the 100% reference, on whatever scale Excel
    # auto-picks. Computed up front (not from written cells) so both the
    # Speedup and Performance sheets can be built knowing which columns are
    # actually needed.
    CAP_THRESHOLD_PCT = 67.0
    max_pct = 0.0
    for r in runs:
        for t in all_threads:
            entry = r["table"].get(t)
            gflops = entry[gflops_key] if entry else None
            if gflops is not None:
                max_pct = max(max_pct, gflops / (args.tpp_per_core * t) * 100)
    cap_axis = max_pct <= CAP_THRESHOLD_PCT
    axis_max = (max_pct // 10 + 2) * 10 if cap_axis else max(100, (max_pct // 10 + 1) * 10)

    out_path = args.out
    if out_path is None:
        topic = args.topic or slugify(args.title)
        out_path = derive_out_path(args.csvs, topic)
        if out_path is None:
            raise SystemExit(
                "error: --out omitted and it can't be derived automatically "
                "-- not every --csv is under a recognized NN-VARIANT "
                "directory (or they don't share a common parent); pass "
                "--out explicitly")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    wb = xlsxwriter.Workbook(out_path)
    fmt_hdr = wb.add_format({"bold": True, "bg_color": "#DCE6F1"})
    fmt_bold = wb.add_format({"bold": True})
    fmt_2_1 = wb.add_format({"num_format": "00.0"})

    # -- Config sheet --
    cfg = wb.add_worksheet("Config")
    cfg.set_column(0, 0, 22)
    cfg.set_column(1, 1, 40)
    cfg.write(0, 0, "Parameter", fmt_hdr)
    cfg.write(0, 1, "Value", fmt_hdr)
    cfg.write(1, 0, "TPP per core (GFLOPs/s)")
    cfg.write_number(1, 1, args.tpp_per_core)
    wb.define_name("TPP_PER_CORE", "=Config!$B$2")
    cfg.write(2, 0, "TMP (GFLOPs/s)")
    if args.tmp is not None:
        cfg.write_number(2, 1, args.tmp)
    cfg.write(3, 0, "Stat used")
    cfg.write(3, 1, args.stat)
    row = 5
    cfg.write(row, 0, "Runs", fmt_hdr)
    row += 1
    for r in runs:
        cfg.write(row, 0, r["label"])
        cfg.write(row, 1, r["path"])
        row += 1
        for k, v in r["preamble"].items():
            cfg.write(row, 0, f"  {k}")
            cfg.write(row, 1, v)
            row += 1

    # -- Raw sheets: one per run, preamble + table as given --
    used_names = {"Config"}

    def safe_name(name):
        cleaned = "".join(c if c.isalnum() else "_" for c in name)[:28] or "Raw"
        candidate = cleaned
        n = 1
        while candidate in used_names:
            candidate = f"{cleaned[:26]}_{n}"
            n += 1
        used_names.add(candidate)
        return candidate

    for r in runs:
        sname = safe_name(f"{r['label']}_Raw")
        r["raw_sheet"] = sname
        ws = wb.add_worksheet(sname)
        rr = 0
        for k, v in r["preamble"].items():
            ws.write(rr, 0, k, fmt_bold)
            ws.write(rr, 1, v)
            rr += 1
        rr += 1
        cols = ["threads", "median", "mean_wo_outliers", "gflops_median",
                "gflops_mean_wo_outliers"]
        for c, name in enumerate(cols):
            ws.write(rr, c, name, fmt_hdr)
        header_row = rr
        rr += 1
        r["table_rows"] = {}
        for t in sorted(r["table"].keys()):
            entry = r["table"][t]
            ws.write_number(rr, 0, t)
            for c, name in enumerate(cols[1:], start=1):
                val = entry[name]
                if val is None:
                    ws.write_formula(rr, c, "=NA()")
                else:
                    ws.write_number(rr, c, val)
            r["table_rows"][t] = rr
            rr += 1
        r["header_row"] = header_row

    # -- Speedup sheet + chart --
    spd = wb.add_worksheet("Speedup")
    spd.write(0, 0, "Threads", fmt_hdr)
    for j, r in enumerate(runs):
        spd.write(0, 1 + j, f"{r['label']} speedup", fmt_hdr)
    c_ideal = 1 + len(runs)
    spd.write(0, c_ideal, "Ideal speedup", fmt_hdr)
    c_perf0 = c_ideal + 1
    for j, r in enumerate(runs):
        spd.write(0, c_perf0 + j, f"{r['label']} Thread-normalized Performance", fmt_hdr)
    n_t = len(all_threads)
    for i, t in enumerate(all_threads):
        row_i = i + 1
        spd.write_number(row_i, 0, t)
        for j, r in enumerate(runs):
            col = 1 + j
            entry = r["table"].get(t)
            stat_time = entry[time_key] if entry else None
            if entry is not None and stat_time and r["seq_time"] is not None:
                spd.write_number(row_i, col, r["seq_time"] / stat_time)
            else:
                spd.write_formula(row_i, col, "=NA()")
            gflops = entry[gflops_key] if entry else None
            if gflops is not None:
                spd.write_number(row_i, c_perf0 + j, gflops / (args.tpp_per_core * t) * 100, fmt_2_1)
            else:
                spd.write_formula(row_i, c_perf0 + j, "=NA()", fmt_2_1)
        spd.write_number(row_i, c_ideal, t)
    AXIS_FONT = {"size": 14}

    # -- Performance sheet + chart --
    perf = wb.add_worksheet("Performance")
    perf.write(0, 0, "Threads", fmt_hdr)
    col_map = {}
    col = 1
    for r in runs:
        c_pct, c_gf = col, col + 1
        col_map[r["label"]] = (c_pct, c_gf)
        perf.write(0, c_pct, f"{r['label']} % of TPP", fmt_hdr)
        perf.write(0, c_gf, f"{r['label']} GFLOPs/s", fmt_hdr)
        col += 2
    c_ref = None
    if not cap_axis:
        c_ref = col
        perf.write(0, c_ref, "100% TPP", fmt_hdr)

    for i, t in enumerate(all_threads):
        row_i = i + 1
        perf.write_number(row_i, 0, t)
        for r in runs:
            c_pct, c_gf = col_map[r["label"]]
            entry = r["table"].get(t)
            gflops = entry[gflops_key] if entry else None
            if gflops is not None:
                perf.write_number(row_i, c_gf, gflops, fmt_2_1)
                pct = gflops / (args.tpp_per_core * t) * 100
                perf.write_formula(
                    row_i, c_pct,
                    f"={qref('Performance', row_i, c_gf)}/(TPP_PER_CORE*{t})*100",
                    fmt_2_1, pct)
            else:
                perf.write_formula(row_i, c_pct, "=NA()", fmt_2_1)
                perf.write_formula(row_i, c_gf, "=NA()", fmt_2_1)
        if c_ref is not None:
            perf.write_number(row_i, c_ref, 100, fmt_2_1)

    # -- Combined Speedup + Performance chart --
    # Speedup (line, left axis) is the primary chart; Performance (column,
    # right axis, %TPP) is combined in as the secondary chart. Excel/xlsxwriter
    # can't combine a scatter-type chart with a column chart, so Speedup uses
    # a category (line) x-axis here rather than a true numeric one -- it now
    # shares the same category axis as Performance's bars.
    #
    # The 100% TPP reference (when shown) lives in the SAME chart object as
    # the %TPP bars, as a second column series, not as a separate line
    # series on the primary chart -- xlsxwriter/Excel corrupts the combined
    # chart's axis XML (a dangling axis reference) whenever the *primary*
    # chart object mixes a plain series with a y2-flagged one. Putting both
    # secondary-axis series in the *secondary* chart object instead is the
    # structurally valid pattern (verified against the raw chart XML), at
    # the cost of the reference rendering as a bar rather than a dashed line.
    line_chart = wb.add_chart({"type": "line"})
    for j, r in enumerate(runs):
        col = 1 + j
        line_chart.add_series({
            "name": f"{r['label']} speedup",
            "categories": ["Speedup", 1, 0, n_t, 0],
            "values": ["Speedup", 1, col, n_t, col],
            "marker": {"type": "circle", "size": 6},
        })
    line_chart.add_series({
        "name": "Ideal speedup",
        "categories": ["Speedup", 1, 0, n_t, 0],
        "values": ["Speedup", 1, c_ideal, n_t, c_ideal],
        "line": {"dash_type": "dash", "color": "#888888", "width": 1.5},
        "marker": {"type": "none"},
    })
    x_axis_name = "Threads"
    if cap_axis:
        x_axis_name += (
            f"\nNote: y-axis capped at {axis_max:.0f}%, (tallest bar is "
            f"{max_pct:.1f}% of TPP)")
    line_chart.set_title({"name": (args.title or "Speedup & Performance") + " vs. thread count"})
    line_chart.set_x_axis({"name": x_axis_name,
                            "name_font": AXIS_FONT, "num_font": AXIS_FONT})
    line_chart.set_y_axis({"name": "Speedup",
                            "name_font": AXIS_FONT, "num_font": AXIS_FONT})

    bar = wb.add_chart({"type": "column", "subtype": "clustered"})
    for r in runs:
        c_pct, c_gf = col_map[r["label"]]
        custom_labels = [{"value": qref("Performance", 1 + i, c_gf)}
                          for i in range(n_t)]
        bar.add_series({
            "name": r["label"],
            "categories": ["Performance", 1, 0, n_t, 0],
            "values": ["Performance", 1, c_pct, n_t, c_pct],
            "y2_axis": True,
            "data_labels": {"custom": custom_labels, "num_format": "00.0"},
        })
    if not cap_axis:
        bar.add_series({
            "name": "100% TPP",
            "categories": ["Performance", 1, 0, n_t, 0],
            "values": ["Performance", 1, c_ref, n_t, c_ref],
            "y2_axis": True,
            "fill": {"color": "#888888"},
        })
    bar.set_y2_axis({"name": "Thread-normalized Performance", "min": 0, "max": axis_max,
                      "num_format": "0", "name_font": AXIS_FONT, "num_font": AXIS_FONT})

    line_chart.combine(bar)
    line_chart.set_legend({"position": "bottom", "font": AXIS_FONT})
    line_chart.set_size({"width": 760, "height": 920})
    spd.insert_chart(1, 7, line_chart)

    spd.activate()
    wb.close()
    fix_axis_title_linebreaks(out_path)
    print(f"wrote {out_path}: {len(runs)} run(s), threads={all_threads}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
