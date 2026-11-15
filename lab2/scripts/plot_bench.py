#!/usr/bin/env python3
"""Plot the CSV files written by `aestool bench` (throughput MB/s with 95% CI error bars).

Usage:
    python3 scripts/plot_bench.py output/ubuntu/bench --out output/ubuntu/plots
    python3 scripts/plot_bench.py output/windows/bench/bench_aes128.csv --out output/windows/plots

For every CSV and every direction (encrypt / decrypt) one PNG is written: grouped bars per payload size, one bar
per implementation (pure/sbox-table, pure/ttable, pure/aesni, pure/sbox-computed, openssl, cryptopp), y axis in
log scale (the implementations differ by 2-4 orders of magnitude), error bars = 95% confidence interval.
Only matplotlib and the standard library are needed. The numbers are read from the CSV, never recomputed.
"""
import argparse
import csv
import pathlib
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

ORDER = ["pure/sbox-table", "pure/sbox-computed", "pure/ttable", "pure/aesni", "cryptopp", "openssl"]


def base_name(impl):
    for suffix in ("-128", "-192", "-256"):
        if impl.endswith(suffix):
            return impl[: -len(suffix)]
    return impl


def size_label(n):
    n = int(n)
    for unit, div in (("GiB", 1 << 30), ("MiB", 1 << 20), ("KiB", 1 << 10)):
        if n >= div and n % div == 0:
            return f"{n // div} {unit}"
    return f"{n} B"


def load(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def plot_one(csv_path, out_dir):
    rows = load(csv_path)
    if not rows:
        return []
    written = []
    for direction in ("encrypt", "decrypt"):
        sel = [r for r in rows if r["direction"] == direction]
        if not sel:
            continue
        sizes = sorted({int(r["size_bytes"]) for r in sel})
        impls = sorted({r["implementation"] for r in sel},
                       key=lambda s: (ORDER.index(base_name(s)) if base_name(s) in ORDER else 99, s))
        fig, ax = plt.subplots(figsize=(max(6.0, 2.2 * len(sizes) + 3), 4.6))
        width = 0.8 / len(impls)
        for k, impl in enumerate(impls):
            xs, ys, errs = [], [], []
            for i, size in enumerate(sizes):
                match = [r for r in sel if r["implementation"] == impl and int(r["size_bytes"]) == size]
                if not match:
                    continue
                r = match[0]
                mean = float(r["mean_MBps"])
                xs.append(i + (k - (len(impls) - 1) / 2) * width)
                ys.append(mean)
                errs.append(float(r["ci95_hi_MBps"]) - mean)
            ax.bar(xs, ys, width * 0.95, yerr=errs, capsize=2, label=impl)
        ax.set_xticks(range(len(sizes)))
        ax.set_xticklabels([size_label(s) for s in sizes])
        ax.set_yscale("log")
        ax.set_ylabel("Throughput (MB/s, log scale)")
        ax.set_xlabel("Payload size")
        ax.set_title(f"{pathlib.Path(csv_path).stem} - {direction} (mean, 95% CI)")
        ax.grid(axis="y", which="both", alpha=0.3)
        ax.legend(fontsize=8)
        fig.tight_layout()
        out = out_dir / f"{pathlib.Path(csv_path).stem}_{direction}.png"
        fig.savefig(out, dpi=150)
        plt.close(fig)
        written.append(out)
    return written


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("inputs", nargs="+", help="CSV files or directories containing bench_*.csv")
    ap.add_argument("--out", default="plots", help="output directory for the PNG files")
    args = ap.parse_args()

    files = []
    for item in args.inputs:
        p = pathlib.Path(item)
        files += sorted(p.glob("bench_*.csv")) if p.is_dir() else [p]
    if not files:
        sys.exit("no CSV files found")
    out_dir = pathlib.Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    for f in files:
        for out in plot_one(f, out_dir):
            print("wrote", out)


if __name__ == "__main__":
    main()
