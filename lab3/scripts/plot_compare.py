#!/usr/bin/env python3
"""Cross-OS comparison plots for Lab 3, from the summary.csv written by scripts/analyze_bench.py for each OS.

    python3 scripts/plot_compare.py            # reads output/windows/analysis and output/ubuntu/analysis, writes output/compare

Figures (error bars = 95% confidence interval, taken from summary.csv):
  keygen_compare.png   RSA key generation time, 3072 / 4096, Windows vs Ubuntu
  rsa_compare.png      RSA-OAEP encrypt / decrypt time per operation (log scale), Windows vs Ubuntu
  aesgcm_compare.png   AES-256-GCM throughput at 1 KiB / 1 MiB / 100 MiB, encrypt and decrypt
  hybrid_compare.png   hybrid encrypt / decrypt time vs payload size (log-log), RSA-3072 and RSA-4096
Only matplotlib and the standard library are needed.
"""
import argparse
import csv
import pathlib

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

OS_LABEL = {"windows": "Windows (MinGW GCC 16.2)", "ubuntu": "Ubuntu 24.04 (GCC 13.3)"}
SIZE_LABEL = {1024: "1 KiB", 1 << 20: "1 MiB", 100 << 20: "100 MiB"}


def load(path):
    rows = {}
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            rows[(r["suite"], r["op"], int(r["rsa_bits"]), int(r["size_bytes"]))] = {k: (float(v) if v not in ("", None) and k not in ("suite", "op", "ops_per_block") else v) for k, v in r.items()}
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default="output")
    ap.add_argument("--out", default="output/compare")
    a = ap.parse_args()
    root = pathlib.Path(a.root)
    data = {}
    for osn in ("windows", "ubuntu"):
        p = root / osn / "analysis" / "summary.csv"
        if p.exists():
            data[osn] = load(p)
    if not data:
        raise SystemExit("no summary.csv found: run scripts/analyze_bench.py first")
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    plt.rcParams["axes.titlesize"] = 9
    oss = list(data)
    n = len(oss)

    # keygen
    fig, ax = plt.subplots(figsize=(6, 4))
    w = 0.8 / n
    for k, osn in enumerate(oss):
        xs, ys, es = [], [], []
        for i, bits in enumerate((3072, 4096)):
            r = data[osn].get(("keygen", "keygen", bits, 0))
            if r:
                xs.append(i + (k - (n - 1) / 2) * w)
                ys.append(r["mean"] / 1e6)
                es.append(r["ci95"] / 1e6)
        ax.bar(xs, ys, w * 0.95, yerr=es, capsize=3, label=OS_LABEL[osn])
    ax.set_xticks([0, 1])
    ax.set_xticklabels(["3072", "4096"])
    ax.set_xlabel("RSA modulus (bits)")
    ax.set_ylabel("Key generation time (ms)")
    ax.set_title("RSA key generation, 30 keys per size (mean, 95% CI)")
    ax.grid(axis="y", alpha=0.3)
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(out / "keygen_compare.png", dpi=150)
    plt.close(fig)

    # rsa enc/dec
    fig, ax = plt.subplots(figsize=(7, 4.2))
    cases = [("encrypt", 3072), ("decrypt", 3072), ("encrypt", 4096), ("decrypt", 4096)]
    for k, osn in enumerate(oss):
        xs, ys, es = [], [], []
        for i, (op, bits) in enumerate(cases):
            r = data[osn].get(("rsa", op, bits, 0))
            if r:
                xs.append(i + (k - (n - 1) / 2) * w)
                ys.append(r["mean"] / 1e3)
                es.append(r["ci95"] / 1e3)
        ax.bar(xs, ys, w * 0.95, yerr=es, capsize=3, label=OS_LABEL[osn])
    ax.set_xticks(range(len(cases)))
    ax.set_xticklabels(["%s\nRSA-%d" % (op, b) for op, b in cases])
    ax.set_yscale("log")
    ax.set_ylabel("Time per operation (us, log scale)")
    ax.set_title("RSA-OAEP encrypt vs decrypt (mean, 95% CI)")
    ax.grid(axis="y", which="both", alpha=0.3)
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(out / "rsa_compare.png", dpi=150)
    plt.close(fig)

    # aes-gcm throughput
    sizes = [1024, 1 << 20, 100 << 20]
    fig, axes = plt.subplots(1, 2, figsize=(10, 4))
    for ax, op in zip(axes, ("encrypt", "decrypt")):
        for k, osn in enumerate(oss):
            xs, ys, es = [], [], []
            for i, s in enumerate(sizes):
                r = data[osn].get(("aesgcm", op, 0, s))
                if r:
                    xs.append(i + (k - (n - 1) / 2) * w)
                    ys.append(r["thr_mean_MBps"])
                    es.append(r["thr_ci95_MBps"])
            ax.bar(xs, ys, w * 0.95, yerr=es, capsize=3, label=OS_LABEL[osn])
        ax.set_xticks(range(len(sizes)))
        ax.set_xticklabels([SIZE_LABEL[s] for s in sizes])
        ax.set_xlabel("Payload size")
        ax.set_ylabel("Throughput (MB/s)")
        ax.set_title("AES-256-GCM %s throughput (mean, 95%% CI)" % op)
        ax.grid(axis="y", alpha=0.3)
        ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(out / "aesgcm_compare.png", dpi=150)
    plt.close(fig)

    # hybrid latency vs size
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.4))
    styles = {"windows": "-", "ubuntu": "--"}
    for ax, op in zip(axes, ("encrypt", "decrypt")):
        for osn in oss:
            for bits, marker in ((3072, "o"), (4096, "s")):
                pts = [(s, data[osn].get(("hybrid", op, bits, s))) for s in sizes]
                pts = [(s, r) for s, r in pts if r]
                ax.errorbar([s for s, _ in pts], [r["mean"] / 1e6 for _, r in pts], yerr=[r["ci95"] / 1e6 for _, r in pts], fmt=styles[osn] + marker,
                            capsize=3, label="%s, RSA-%d" % (osn, bits))
        ax.set_xscale("log")
        ax.set_yscale("log")
        ax.set_xticks(sizes)
        ax.set_xticklabels([SIZE_LABEL[s] for s in sizes])
        ax.set_xlabel("Payload size")
        ax.set_ylabel("Time per operation (ms, log scale)")
        ax.set_title("Hybrid %s: time vs payload size (mean, 95%% CI)" % op)
        ax.grid(which="both", alpha=0.3)
        ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(out / "hybrid_compare.png", dpi=150)
    plt.close(fig)
    for name in ("keygen_compare", "rsa_compare", "aesgcm_compare", "hybrid_compare"):
        print("wrote", out / (name + ".png"))


if __name__ == "__main__":
    main()
