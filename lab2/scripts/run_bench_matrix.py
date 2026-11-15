#!/usr/bin/env python3
"""Lab 2 benchmark matrix, driven by the native `aestool bench` (single source of truth for the protocol).

Protocol (same for every case): warm-up 1.5 s -> block of ~1000 operations (fewer for big payloads, see --budget-mb)
-> N independent runs (30..100) -> mean / median / SD / 95% CI.  Nothing here times subprocesses or file I/O.

Usage:
    python scripts/run_bench_matrix.py [--tool PATH_TO_AESTOOL] [--runs 50] [--threads 1] [--out-dir DIR] [--quick]

Output: one CSV per (key size, payload size) in <out-dir> plus a merged bench_lab2_<os>.csv with the extra columns
key_bits and sbox backend, as read by generate_lab2_rich_visuals.py.
"""
import argparse
import csv
import os
import platform
import subprocess
import sys
from pathlib import Path

LAB_DIR = Path(__file__).resolve().parent.parent
OS_NAME = "windows" if platform.system() == "Windows" else "ubuntu"

SIZES = ["1MiB", "100MiB", "1GiB"]          # per-size run counts below stay within the 30..100 protocol range
RUNS_BY_SIZE = {"1MiB": 100, "100MiB": 50, "1GiB": 30}
BACKENDS = "table,ttable,aesni"


def find_tool(explicit):
    if explicit:
        return explicit
    env = os.environ.get("AESTOOL")
    if env:
        return env
    for c in [LAB_DIR / "build_windows_msvc" / "Release" / "aestool.exe", LAB_DIR / "build_windows_gcc" / "aestool.exe",
              LAB_DIR / "build_ubuntu_gcc" / "aestool", LAB_DIR / "build" / "aestool", LAB_DIR / "build" / "aestool.exe"]:
        if c.exists():
            return str(c)
    sys.exit("aestool not found: pass --tool, set AESTOOL, or build first")


def backend_of(impl):
    """'pure/sbox-table-256' -> 'table'; 'openssl' -> 'openssl'."""
    name = impl.split("/")[-1]
    for suffix in ("-128", "-192", "-256"):
        if name.endswith(suffix):
            name = name[: -len(suffix)]
    return {"sbox-table": "table", "sbox-computed": "computed"}.get(name, name)


def collect(csv_path, bits, size, merged):
    """Append the native rows plus the column names read by generate_lab2_rich_visuals.py."""
    with open(csv_path, newline="") as f:
        for row in csv.DictReader(f):
            row["key_bits"] = bits
            row["size"] = size
            row["sbox_mode"] = backend_of(row["implementation"])
            row["op"] = row["direction"]
            row["label"] = f"AES-{bits} ({row['sbox_mode']})"
            row["thr_mean_MBps"] = row["mean_MBps"]
            merged.append(row)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tool")
    ap.add_argument("--runs", type=int, help="override the run count for every payload size (30..100)")
    ap.add_argument("--threads", type=int, default=1)
    ap.add_argument("--out-dir")
    ap.add_argument("--quick", action="store_true", help="smoke test: 1 MiB only, 3 runs, short warm-up")
    args = ap.parse_args()
    if args.runs is not None and not 30 <= args.runs <= 100 and not args.quick:
        sys.exit("--runs must be within 30..100 (protocol)")

    tool = find_tool(args.tool)
    out_dir = Path(args.out_dir) if args.out_dir else LAB_DIR / "Output" / OS_NAME / "bench"
    out_dir.mkdir(parents=True, exist_ok=True)
    sizes = ["1MiB"] if args.quick else SIZES

    merged = []
    for bits in (128, 192, 256):
        for size in sizes:
            runs = 3 if args.quick else (args.runs or RUNS_BY_SIZE[size])
            csv_path = out_dir / f"bench_aes{bits}_{size}.csv"
            cmd = [tool, "bench", "--bits", str(bits), "--sizes", size, "--runs", str(runs), "--backends", BACKENDS,
                   "--threads", str(args.threads), "--csv", str(csv_path)]
            if args.quick:
                cmd += ["--warmup", "0.1"]
            print("[bench]", " ".join(cmd), flush=True)
            subprocess.run(cmd, check=True)
            collect(csv_path, bits, size, merged)

    # The constant-time "computed" S-box is ~100x slower than the table: use a short block (documented deviation).
    comp_csv = out_dir / "bench_aes128_computed_1MiB.csv"
    cmd = [tool, "bench", "--bits", "128", "--sizes", "1MiB", "--runs", "3" if args.quick else "30", "--ops", "2",
           "--backends", "computed", "--ref", "none", "--threads", str(args.threads), "--csv", str(comp_csv)]
    if args.quick:
        cmd += ["--warmup", "0.1"]
    print("[bench]", " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True)
    collect(comp_csv, 128, "1MiB", merged)

    if merged:
        merged_path = out_dir / f"bench_lab2_{OS_NAME}.csv"
        with open(merged_path, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(merged[0].keys()))
            w.writeheader()
            w.writerows(merged)
        print("[bench] merged CSV ->", merged_path)


if __name__ == "__main__":
    main()
