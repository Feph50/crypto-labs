#!/usr/bin/env python3
"""Cross-OS plots for Lab 2 from the CSV files written by `aestool bench` (run from the lab2 directory).

    python3 scripts/plot_compare.py                 # reads output/windows/bench and output/ubuntu/bench, writes output/compare

Figures (throughput in MB/s, log scale, error bars = 95% confidence interval taken from the CSV):
  throughput_vs_size.png    pure AES-128 (table S-box) vs Crypto++ vs OpenSSL at 1 MiB / 100 MiB / 1 GiB, encrypt and decrypt, per OS
  sbox_table_vs_computed.png table S-box vs computed S-box at every size where both were measured (4 KiB, 1 MiB, 100 MiB), per OS
Numbers are read from the CSV, never recomputed. Only matplotlib and the standard library are needed.
"""
import argparse
import csv
import pathlib

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

OS_NAMES = {"windows": "Windows (MinGW-w64 GCC)", "ubuntu": "Ubuntu 24.04 (GCC)"}
SIZE_LABEL = {4096: "4 KiB", 1 << 20: "1 MiB", 100 << 20: "100 MiB", 1 << 30: "1 GiB"}


def load_dir(d):
    rows = []
    for f in sorted(pathlib.Path(d).glob("bench_*.csv")):
        with open(f, newline="") as fh:
            for r in csv.DictReader(fh):
                r["_file"] = f.name
                rows.append(r)
    return rows


def pick(rows, impl, direction, size):
    for r in rows:
        if r["implementation"] == impl and r["direction"] == direction and int(r["size_bytes"]) == size:
            return float(r["mean_MBps"]), float(r["ci95_hi_MBps"]) - float(r["mean_MBps"])
    return None


def fig_throughput(data, out):
    impls = ["pure/sbox-table", "cryptopp", "openssl"]
    sizes = [1 << 20, 100 << 20, 1 << 30]
    fig, axes = plt.subplots(2, len(data), figsize=(6.2 * len(data), 7.4), squeeze=False)
    for col, (osn, rows) in enumerate(data.items()):
        for row, direction in enumerate(("encrypt", "decrypt")):
            ax = axes[row][col]
            w = 0.8 / len(impls)
            for k, impl in enumerate(impls):
                xs, ys, es = [], [], []
                for i, s in enumerate(sizes):
                    v = pick(rows, impl, direction, s)
                    if v:
                        xs.append(i + (k - (len(impls) - 1) / 2) * w)
                        ys.append(v[0])
                        es.append(v[1])
                ax.bar(xs, ys, w * 0.95, yerr=es, capsize=2, label=impl)
            ax.set_xticks(range(len(sizes)))
            ax.set_xticklabels([SIZE_LABEL[s] for s in sizes])
            ax.set_yscale("log")
            ax.set_ylabel("MB/s (log)")
            ax.set_title(f"{OS_NAMES.get(osn, osn)} - {direction}")
            ax.grid(axis="y", which="both", alpha=0.3)
            ax.legend(fontsize=8, loc="upper center", bbox_to_anchor=(0.5, -0.1), ncol=3)
    fig.tight_layout()
    fig.savefig(out, dpi=150)
    plt.close(fig)


def fig_sbox(data, out):
    all_sizes = [4096, 1 << 20, 100 << 20]
    sizes = [s for s in all_sizes
             if any(pick(rows, impl, "encrypt", s) for rows in data.values()
                    for impl in ("pure/sbox-table", "pure/sbox-computed"))]
    fig, axes = plt.subplots(1, 2, figsize=(11.5, 4.6))
    for ax, direction in zip(axes, ("encrypt", "decrypt")):
        series = []
        for osn, rows in data.items():
            for impl, name in (("pure/sbox-table", "table"), ("pure/sbox-computed", "computed")):
                series.append((osn, impl, f"{osn} / {name}"))
        w = 0.8 / len(series)
        for k, (osn, impl, label) in enumerate(series):
            xs, ys, es = [], [], []
            for i, s in enumerate(sizes):
                v = pick(data[osn], impl, direction, s)
                if v:
                    xs.append(i + (k - (len(series) - 1) / 2) * w)
                    ys.append(v[0])
                    es.append(v[1])
            if xs:
                ax.bar(xs, ys, w * 0.95, yerr=es, capsize=2, label=label)
        ax.set_xticks(range(len(sizes)))
        ax.set_xticklabels([SIZE_LABEL[s] for s in sizes])
        ax.set_yscale("log")
        ax.set_ylabel("MB/s (log)")
        ax.set_title(f"Table vs computed S-box - {direction}")
        ax.grid(axis="y", which="both", alpha=0.3)
        ax.legend(fontsize=8, loc="upper center", bbox_to_anchor=(0.5, -0.1), ncol=2)
    fig.tight_layout()
    fig.savefig(out, dpi=150)
    plt.close(fig)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default="output")
    ap.add_argument("--out", default="output/compare")
    args = ap.parse_args()
    root = pathlib.Path(args.root)
    data = {}
    for osn in ("windows", "ubuntu"):
        d = root / osn / "bench"
        if d.is_dir():
            rows = load_dir(d)
            if rows:
                data[osn] = rows
    if not data:
        raise SystemExit("no bench CSV found under %s" % root)
    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    fig_throughput(data, out / "throughput_vs_size.png")
    fig_sbox(data, out / "sbox_table_vs_computed.png")
    print("wrote", out / "throughput_vs_size.png")
    print("wrote", out / "sbox_table_vs_computed.png")


if __name__ == "__main__":
    main()
