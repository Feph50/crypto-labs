#!/usr/bin/env python3
"""Statistics and plots for the CSV written by `rsabench` (one CSV per operating system).

    python3 scripts/analyze_bench.py output/windows/bench/bench_*.csv --label Windows --out-dir output/windows/analysis
    python3 scripts/analyze_bench.py output/ubuntu/bench/bench_*.csv  --label Ubuntu  --out-dir output/ubuntu/analysis

For every group (suite, op, RSA bits, payload size): n, mean, median, standard deviation, 95% confidence interval of the mean
(Student t), min, max of the per-operation time, and the throughput for aesgcm/hybrid. Outputs in --out-dir:
  summary.csv, summary.md (Markdown tables to paste into the report) and PNG plots with error bars (95% CI):
  latency_rsa_keygen.png, latency_rsa_encdec.png, throughput_aesgcm.png, latency_hybrid.png, throughput_hybrid.png
The numbers come straight from the CSV. Only the Python standard library and matplotlib (for the plots) are needed.
"""
import argparse
import csv
import math
import pathlib
import statistics
import sys
from collections import defaultdict
from statistics import NormalDist


def t_crit(df, p=0.975):
    """Two-sided 95% Student-t quantile (scipy if installed, otherwise the Cornish-Fisher expansion, exact enough for df >= 5)."""
    try:
        from scipy import stats  # type: ignore
        return float(stats.t.ppf(p, df))
    except Exception:
        z = NormalDist().inv_cdf(p)
        g1 = (z ** 3 + z) / 4
        g2 = (5 * z ** 5 + 16 * z ** 3 + 3 * z) / 96
        g3 = (3 * z ** 7 + 19 * z ** 5 + 17 * z ** 3 - 15 * z) / 384
        g4 = (79 * z ** 9 + 776 * z ** 7 + 1482 * z ** 5 - 1920 * z ** 3 - 945 * z) / 92160
        return z + g1 / df + g2 / df ** 2 + g3 / df ** 3 + g4 / df ** 4


def stats_of(values):
    n = len(values)
    mean = statistics.fmean(values)
    sd = statistics.stdev(values) if n > 1 else 0.0
    ci = t_crit(n - 1) * sd / math.sqrt(n) if n > 1 else 0.0
    return {"n": n, "mean": mean, "median": statistics.median(values), "sd": sd, "ci95": ci, "min": min(values), "max": max(values)}


def size_label(n):
    n = int(n)
    if n == 0:
        return "-"
    for unit, div in (("GiB", 1 << 30), ("MiB", 1 << 20), ("KiB", 1 << 10)):
        if n >= div and n % div == 0:
            return "%d %s" % (n // div, unit)
    return "%d B" % n


def fmt_time(ns):
    if ns >= 1e9:
        return "%.3f s" % (ns / 1e9)
    if ns >= 1e6:
        return "%.3f ms" % (ns / 1e6)
    if ns >= 1e3:
        return "%.2f us" % (ns / 1e3)
    return "%.1f ns" % ns


def load(paths):
    groups = defaultdict(lambda: {"ns": [], "mbps": [], "ops": set()})
    for path in paths:
        with open(path, newline="") as f:
            for r in csv.DictReader(f):
                key = (r["suite"], r["op"], int(r["rsa_bits"]), int(r["size_bytes"]))
                g = groups[key]
                g["ns"].append(float(r["time_ns_per_op"]))
                g["ops"].add(int(r["ops"]))
                if r["throughput_MBps"]:
                    g["mbps"].append(float(r["throughput_MBps"]))
    if not groups:
        sys.exit("no rows in the given CSV files")
    return groups


ORDER = {"keygen": 0, "rsa": 1, "aesgcm": 2, "hybrid": 3}


def build_summary(groups):
    rows = []
    for (suite, op, bits, size), g in sorted(groups.items(), key=lambda kv: (ORDER.get(kv[0][0], 9), kv[0][2], kv[0][3], kv[0][1])):
        s = stats_of(g["ns"])
        row = {"suite": suite, "op": op, "rsa_bits": bits, "size_bytes": size, "ops_per_block": "/".join(str(x) for x in sorted(g["ops"])), **s}
        if g["mbps"]:
            m = stats_of(g["mbps"])
            row.update({"thr_mean_MBps": m["mean"], "thr_median_MBps": m["median"], "thr_sd_MBps": m["sd"], "thr_ci95_MBps": m["ci95"]})
        rows.append(row)
    return rows


def write_outputs(rows, out_dir, label):
    out = pathlib.Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    cols = ["suite", "op", "rsa_bits", "size_bytes", "ops_per_block", "n", "mean", "median", "sd", "ci95", "min", "max",
            "thr_mean_MBps", "thr_median_MBps", "thr_sd_MBps", "thr_ci95_MBps"]
    with open(out / "summary.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=cols, restval="")
        w.writeheader()
        for r in rows:
            w.writerow({k: ("%.4f" % v if isinstance(v, float) else v) for k, v in r.items() if k in cols})
    md = ["## %s" % label, "",
          "| Suite | Op | RSA bits | Payload | Ops/block | N | Mean | Median | SD | ±CI95 | Throughput mean (MB/s) | ±CI95 (MB/s) |",
          "| :--- | :--- | ---: | :---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |"]
    for r in rows:
        thr = ("%.1f" % r["thr_mean_MBps"], "±%.1f" % r["thr_ci95_MBps"]) if "thr_mean_MBps" in r else ("", "")
        md.append("| %s | %s | %s | %s | %s | %d | %s | %s | %s | ±%s | %s | %s |" % (
            r["suite"], r["op"], r["rsa_bits"] or "-", size_label(r["size_bytes"]), r["ops_per_block"], r["n"], fmt_time(r["mean"]),
            fmt_time(r["median"]), fmt_time(r["sd"]), fmt_time(r["ci95"]), thr[0], thr[1]))
    (out / "summary.md").write_text("\n".join(md) + "\n", encoding="utf-8")
    return out


def plots(rows, out, label):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not installed: plots skipped")
        return
    plt.rcParams["axes.titlesize"] = 9  # long titles must fit inside the figure

    def sel(suite, op=None, bits=None):
        return [r for r in rows if r["suite"] == suite and (op is None or r["op"] == op) and (bits is None or r["rsa_bits"] == bits)]

    # 1. key generation
    kg = sel("keygen")
    if kg:
        fig, ax = plt.subplots(figsize=(5.5, 4))
        ax.bar([str(r["rsa_bits"]) for r in kg], [r["mean"] / 1e6 for r in kg], yerr=[r["ci95"] / 1e6 for r in kg], capsize=4, color=["#4c78a8", "#f58518"][:len(kg)])
        ax.set_xlabel("RSA modulus (bits)")
        ax.set_ylabel("Key generation time (ms)")
        ax.set_title("%s: RSA key generation (mean, 95%% CI)" % label)
        ax.grid(axis="y", alpha=0.3)
        fig.tight_layout()
        fig.savefig(out / "latency_rsa_keygen.png", dpi=150)
        plt.close(fig)

    # 2. RSA-OAEP encrypt vs decrypt
    rs = sel("rsa")
    if rs:
        bits = sorted({r["rsa_bits"] for r in rs})
        fig, ax = plt.subplots(figsize=(6, 4))
        w = 0.35
        for k, op in enumerate(("encrypt", "decrypt")):
            vals = [next((r for r in rs if r["op"] == op and r["rsa_bits"] == b), None) for b in bits]
            ax.bar([i + (k - 0.5) * w for i in range(len(bits))], [v["mean"] / 1e3 if v else 0 for v in vals], w,
                   yerr=[v["ci95"] / 1e3 if v else 0 for v in vals], capsize=3, label=op)
        ax.set_xticks(range(len(bits)))
        ax.set_xticklabels([str(b) for b in bits])
        ax.set_yscale("log")
        ax.set_xlabel("RSA modulus (bits)")
        ax.set_ylabel("Time per operation (us, log scale)")
        ax.set_title("%s: RSA-OAEP encrypt vs decrypt (mean, 95%% CI)" % label)
        ax.grid(axis="y", which="both", alpha=0.3)
        ax.legend()
        fig.tight_layout()
        fig.savefig(out / "latency_rsa_encdec.png", dpi=150)
        plt.close(fig)

    # 3. AES-GCM throughput
    ag = sel("aesgcm")
    if ag:
        sizes = sorted({r["size_bytes"] for r in ag})
        fig, ax = plt.subplots(figsize=(6, 4))
        w = 0.35
        for k, op in enumerate(("encrypt", "decrypt")):
            vals = [next((r for r in ag if r["op"] == op and r["size_bytes"] == s), None) for s in sizes]
            ax.bar([i + (k - 0.5) * w for i in range(len(sizes))], [v["thr_mean_MBps"] if v else 0 for v in vals], w,
                   yerr=[v["thr_ci95_MBps"] if v else 0 for v in vals], capsize=3, label=op)
        ax.set_xticks(range(len(sizes)))
        ax.set_xticklabels([size_label(s) for s in sizes])
        ax.set_xlabel("Payload size")
        ax.set_ylabel("Throughput (MB/s)")
        ax.set_title("%s: AES-256-GCM throughput (mean, 95%% CI)" % label)
        ax.grid(axis="y", alpha=0.3)
        ax.legend()
        fig.tight_layout()
        fig.savefig(out / "throughput_aesgcm.png", dpi=150)
        plt.close(fig)

    # 4/5. hybrid total latency and throughput
    hy = sel("hybrid")
    if hy:
        bits = sorted({r["rsa_bits"] for r in hy})
        sizes = sorted({r["size_bytes"] for r in hy})
        for fname, key, err, ylabel, logy, title in (
                ("latency_hybrid.png", "mean", "ci95", "Time per operation (ms, log scale)", True, "hybrid encrypt/decrypt time"),
                ("throughput_hybrid.png", "thr_mean_MBps", "thr_ci95_MBps", "Throughput (MB/s, log scale)", True, "hybrid throughput")):
            fig, ax = plt.subplots(figsize=(6.5, 4.4))
            for b in bits:
                for op, ls in (("encrypt", "-o"), ("decrypt", "--s")):
                    pts = [next((r for r in hy if r["op"] == op and r["rsa_bits"] == b and r["size_bytes"] == s), None) for s in sizes]
                    xs = [s for s, p in zip(sizes, pts) if p]
                    scale = 1e6 if key == "mean" else 1.0
                    ys = [p[key] / scale for p in pts if p]
                    es = [p[err] / scale for p in pts if p]
                    ax.errorbar(xs, ys, yerr=es, fmt=ls, capsize=3, label="RSA-%d %s" % (b, op))
            ax.set_xscale("log")
            ax.set_yscale("log" if logy else "linear")
            ax.set_xticks(sizes)
            ax.set_xticklabels([size_label(s) for s in sizes])
            ax.set_xlabel("Payload size")
            ax.set_ylabel(ylabel)
            ax.set_title("%s: %s (mean, 95%% CI)" % (label, title))
            ax.grid(which="both", alpha=0.3)
            ax.legend(fontsize=8)
            fig.tight_layout()
            fig.savefig(out / fname, dpi=150)
            plt.close(fig)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", nargs="+", help="one or more rsabench CSV files (rows are merged)")
    ap.add_argument("--label", default=None, help="name used in titles (default: file name)")
    ap.add_argument("--out-dir", default="analysis")
    a = ap.parse_args()
    label = a.label or pathlib.Path(a.csv[0]).stem
    rows = build_summary(load(a.csv))
    out = write_outputs(rows, a.out_dir, label)
    plots(rows, out, label)
    print("wrote %s (summary.csv, summary.md, PNG plots)" % out)


if __name__ == "__main__":
    main()
