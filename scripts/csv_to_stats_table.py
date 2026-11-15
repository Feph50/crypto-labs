#!/usr/bin/env python3
"""Turn an `aestool bench` CSV (Lab 1 or Lab 2) into a Markdown table with the required statistics:
mean, median, standard deviation and 95% confidence interval, for throughput (MB/s) and latency.

Usage:
    python scripts/csv_to_stats_table.py lab1/output/ubuntu/benchmarks/bench_ubuntu_gcc.csv --size 1048576
    python scripts/csv_to_stats_table.py lab2/Output/windows/bench/bench_lab2_windows.csv --size 1048576 > table.md

Options:
    --size BYTES      keep only this payload size (repeatable); default: all sizes
    --mode NAME       Lab 1: keep only this mode / Lab 2: keep only this implementation (repeatable)
    --dir enc|dec     keep only one direction
    --md-out FILE     write to FILE instead of stdout

The format is detected from the CSV header (Lab 1: thr_mean_MBps...; Lab 2: mean_MBps...). Only the Python
standard library is needed. Numbers are copied from the CSV, never recomputed or rounded away.
"""
import argparse
import csv
import sys


def fmt_size(n):
    n = int(n)
    for unit, div in (("MiB", 1 << 20), ("KiB", 1 << 10)):
        if n >= div and n % div == 0:
            return f"{n // div} {unit}"
    return f"{n} B"


def num(x, digits=2):
    return f"{float(x):,.{digits}f}"


def rows_lab1(r):
    # thr_ci95 / lat_ci95 are already half-widths (mean +/- ci95)
    return dict(
        name=r["mode"], dir=r["dir"], size=int(r["size_bytes"]), ops=r["ops_per_block"], runs=r["runs"],
        t_mean=r["thr_mean_MBps"], t_med=r["thr_median_MBps"], t_sd=r["thr_sd"], t_ci=r["thr_ci95"],
        l_mean=r["lat_mean_us"], l_med=r["lat_median_us"], l_sd=r["lat_sd"], l_ci=r["lat_ci95"], l_unit="µs/op")


def rows_lab2(r):
    half = lambda lo, hi: (float(hi) - float(lo)) / 2.0
    return dict(
        name=r["implementation"], dir=r["direction"][:3], size=int(r["size_bytes"]), ops=r["ops_per_run"], runs=r["runs"],
        t_mean=r["mean_MBps"], t_med=r["median_MBps"], t_sd=r["stddev_MBps"],
        t_ci=half(r["ci95_lo_MBps"], r["ci95_hi_MBps"]),
        l_mean=r["mean_ms_per_op"], l_med=r["median_ms_per_op"], l_sd=r["stddev_ms_per_op"],
        l_ci=half(r["ci95_lo_ms"], r["ci95_hi_ms"]), l_unit="ms/op")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv")
    ap.add_argument("--size", action="append", type=int)
    ap.add_argument("--mode", action="append")
    ap.add_argument("--dir", choices=["enc", "dec"])
    ap.add_argument("--md-out")
    a = ap.parse_args()

    with open(a.csv, newline="", encoding="utf-8-sig") as f:
        reader = csv.DictReader(f)
        cols = set(reader.fieldnames or [])
        if "thr_mean_MBps" in cols:
            conv = rows_lab1
        elif "mean_MBps" in cols:
            conv = rows_lab2
        else:
            sys.exit("unrecognised CSV header: " + ",".join(sorted(cols)))
        rows = [conv(r) for r in reader]

    if a.size:
        rows = [r for r in rows if r["size"] in a.size]
    if a.mode:
        rows = [r for r in rows if r["name"] in a.mode]
    if a.dir:
        rows = [r for r in rows if r["dir"] == a.dir]
    if not rows:
        sys.exit("no rows left after filtering")

    unit = rows[0]["l_unit"]
    out = [
        f"| Cấu hình | Chiều | Payload | Ops/khối | N | Mean (MB/s) | Median (MB/s) | SD (MB/s) | ±CI95 (MB/s) | "
        f"Mean ({unit}) | Median ({unit}) | SD ({unit}) | ±CI95 ({unit}) |",
        "| :--- | :---: | :---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for r in rows:
        out.append(
            f"| {r['name']} | {r['dir']} | {fmt_size(r['size'])} | {r['ops']} | {r['runs']} | {num(r['t_mean'])} | "
            f"{num(r['t_med'])} | {num(r['t_sd'])} | ±{num(r['t_ci'])} | {num(r['l_mean'], 3)} | {num(r['l_med'], 3)} | "
            f"{num(r['l_sd'], 3)} | ±{num(r['l_ci'], 3)} |")
    text = "\n".join(out) + "\n"
    if a.md_out:
        with open(a.md_out, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    else:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
