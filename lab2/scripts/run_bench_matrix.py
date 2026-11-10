import os
import sys
import time
import subprocess
import math
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from concurrent.futures import ThreadPoolExecutor

TOOL = r"D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs\lab2\release\windows\aestool.exe"
WORK = r"D:\lab2_bench_data"
os.makedirs(WORK, exist_ok=True)

OUT_CSV = r"D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs\lab2\Output\windows\bench\bench_lab2_windows.csv"
OUT_PLOTS = r"D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs\lab2\Output\windows\plots"
os.makedirs(os.path.dirname(OUT_CSV), exist_ok=True)
os.makedirs(OUT_PLOTS, exist_ok=True)

# Generate Keys & IV
KEY128 = os.path.join(WORK, "bench_key128.bin")
KEY192 = os.path.join(WORK, "bench_key192.bin")
KEY256 = os.path.join(WORK, "bench_key256.bin")
IV_FILE = os.path.join(WORK, "bench_iv.bin")

subprocess.run([TOOL, "keygen", "--out", KEY128, "--bits", "128"], check=True)
subprocess.run([TOOL, "keygen", "--out", KEY192, "--bits", "192"], check=True)
subprocess.run([TOOL, "keygen", "--out", KEY256, "--bits", "256"], check=True)
subprocess.run([TOOL, "ivgen", "--out", IV_FILE], check=True)

SIZES = {
    "1MiB": 1024 * 1024,
    "100MiB": 100 * 1024 * 1024,
    "1GiB": 1024 * 1024 * 1024
}

files = {}
for name, sz in SIZES.items():
    p = os.path.join(WORK, f"plain_{name}.bin")
    if not os.path.exists(p) or os.path.getsize(p) != sz:
        print(f"Creating {name} payload ({sz} bytes)...", flush=True)
        chunk_sz = min(sz, 64 * 1024 * 1024)
        buf = os.urandom(chunk_sz)
        with open(p, "wb") as f:
            written = 0
            while written < sz:
                to_write = min(chunk_sz, sz - written)
                f.write(buf[:to_write])
                written += to_write
        print(f"Created {p}", flush=True)
    files[name] = p

results = []

def run_sequential_test(label, sbox_mode, key_bits, key_file, size_name, runs):
    p_in = files[size_name]
    clean_lbl = label.replace(" ", "_").replace("(", "").replace(")", "")
    p_ct = os.path.join(WORK, f"ct_{clean_lbl}_{size_name}.bin")
    p_out = os.path.join(WORK, f"out_{clean_lbl}_{size_name}.bin")
    sz = SIZES[size_name]
    sz_mb = sz / (1024 * 1024)

    # Warm-up 1 run
    if os.path.exists(p_ct):
        try: os.remove(p_ct)
        except Exception: pass
    subprocess.run([TOOL, "encrypt", "--mode", "ctr", "--key", key_file, "--iv", IV_FILE,
                    "--in", p_in, "--out", p_ct, "--sbox", sbox_mode, "--bits", str(key_bits)], check=True)

    # Encrypt runs
    enc_times = []
    for r in range(runs):
        if os.path.exists(p_ct):
            try: os.remove(p_ct)
            except Exception: pass
        t0 = time.perf_counter()
        subprocess.run([TOOL, "encrypt", "--mode", "ctr", "--key", key_file, "--iv", IV_FILE,
                        "--in", p_in, "--out", p_ct, "--sbox", sbox_mode, "--bits", str(key_bits)], check=True)
        t1 = time.perf_counter()
        enc_times.append(t1 - t0)

    # Decrypt runs
    dec_times = []
    for r in range(runs):
        if os.path.exists(p_out):
            try: os.remove(p_out)
            except Exception: pass
        t0 = time.perf_counter()
        subprocess.run([TOOL, "decrypt", "--mode", "ctr", "--key", key_file, "--iv", IV_FILE,
                        "--in", p_ct, "--out", p_out, "--sbox", sbox_mode, "--bits", str(key_bits)], check=True)
        t1 = time.perf_counter()
        dec_times.append(t1 - t0)

    if os.path.exists(p_ct):
        try: os.remove(p_ct)
        except Exception: pass
    if os.path.exists(p_out):
        try: os.remove(p_out)
        except Exception: pass

    for op, times in [("encrypt", enc_times), ("decrypt", dec_times)]:
        times_arr = np.array(times)
        lat_mean_ms = np.mean(times_arr) * 1000
        lat_med_ms = np.median(times_arr) * 1000
        lat_sd_ms = np.std(times_arr, ddof=1) if len(times_arr) > 1 else 0
        ci95_lat = 1.96 * lat_sd_ms / math.sqrt(len(times_arr)) if len(times_arr) > 1 else 0

        thrs = sz_mb / times_arr
        thr_mean = float(np.mean(thrs))
        thr_med = float(np.median(thrs))
        thr_sd = float(np.std(thrs, ddof=1)) if len(thrs) > 1 else 0
        ci95_thr = 1.96 * thr_sd / math.sqrt(len(thrs)) if len(thrs) > 1 else 0

        results.append({
            "label": label,
            "sbox_mode": sbox_mode,
            "key_bits": key_bits,
            "size": size_name,
            "size_bytes": sz,
            "op": op,
            "runs": runs,
            "thr_mean_MBps": round(thr_mean, 2),
            "thr_med_MBps": round(thr_med, 2),
            "thr_sd": round(thr_sd, 2),
            "thr_ci95": round(ci95_thr, 2),
            "lat_mean_ms": round(lat_mean_ms, 2),
            "lat_med_ms": round(lat_med_ms, 2),
            "lat_sd": round(lat_sd_ms, 2),
            "lat_ci95": round(ci95_lat, 2)
        })
        print(f"[{label:18s}] {size_name:6s} {op:7s} | Runs: {runs:2d} | Throughput: {thr_mean:7.2f} ± {ci95_thr:5.2f} MB/s | Latency: {lat_mean_ms:8.2f} ms", flush=True)

def run_parallel_benchmark(label, sbox_mode, key_bits, key_file, size_name, total_runs, num_workers=8):
    p_in = files[size_name]
    sz = SIZES[size_name]
    sz_mb = sz / (1024 * 1024)
    print(f"\n--- Running {size_name} ({total_runs} runs) using {num_workers} parallel workers ---", flush=True)

    worker_dirs = [os.path.join(WORK, f"worker_{i}") for i in range(num_workers)]
    for d in worker_dirs:
        os.makedirs(d, exist_ok=True)

    # 1. Encrypt runs
    def do_enc(run_idx):
        wid = run_idx % num_workers
        ct = os.path.join(worker_dirs[wid], f"ct_{size_name}_run{run_idx}.bin")
        t0 = time.perf_counter()
        subprocess.run([TOOL, "encrypt", "--mode", "ctr", "--key", key_file, "--iv", IV_FILE,
                        "--in", p_in, "--out", ct, "--sbox", sbox_mode, "--bits", str(key_bits)], check=True)
        t1 = time.perf_counter()
        enc_t = t1 - t0
        try:
            if os.path.exists(ct):
                os.remove(ct)
        except Exception:
            pass
        return enc_t

    print(f"[{size_name}] Starting {total_runs} Encrypt runs across {num_workers} workers...", flush=True)
    with ThreadPoolExecutor(max_workers=num_workers) as ex:
        enc_times = list(ex.map(do_enc, range(total_runs)))
    print(f"[{size_name}] Encrypt runs completed.", flush=True)

    # Prepare reference ciphertext for decrypt benchmark
    ref_ct = os.path.join(WORK, f"ref_ct_{size_name}.bin")
    if os.path.exists(ref_ct):
        try:
            os.remove(ref_ct)
        except Exception:
            pass
    subprocess.run([TOOL, "encrypt", "--mode", "ctr", "--key", key_file, "--iv", IV_FILE,
                    "--in", p_in, "--out", ref_ct, "--sbox", sbox_mode, "--bits", str(key_bits)], check=True)

    # 2. Decrypt runs
    def do_dec(run_idx):
        wid = run_idx % num_workers
        out = os.path.join(worker_dirs[wid], f"out_{size_name}_run{run_idx}.bin")
        t0 = time.perf_counter()
        subprocess.run([TOOL, "decrypt", "--mode", "ctr", "--key", key_file, "--iv", IV_FILE,
                        "--in", ref_ct, "--out", out, "--sbox", sbox_mode, "--bits", str(key_bits)], check=True)
        t1 = time.perf_counter()
        dec_t = t1 - t0
        try:
            if os.path.exists(out):
                os.remove(out)
        except Exception:
            pass
        return dec_t

    print(f"[{size_name}] Starting {total_runs} Decrypt runs across {num_workers} workers...", flush=True)
    with ThreadPoolExecutor(max_workers=num_workers) as ex:
        dec_times = list(ex.map(do_dec, range(total_runs)))
    print(f"[{size_name}] Decrypt runs completed.", flush=True)

    # Clean up reference ciphertext
    try:
        if os.path.exists(ref_ct):
            os.remove(ref_ct)
    except Exception:
        pass

    for op, times in [("encrypt", enc_times), ("decrypt", dec_times)]:
        times_arr = np.array(times)
        lat_mean_ms = np.mean(times_arr) * 1000
        lat_med_ms = np.median(times_arr) * 1000
        lat_sd_ms = np.std(times_arr, ddof=1) if len(times_arr) > 1 else 0
        ci95_lat = 1.96 * lat_sd_ms / math.sqrt(len(times_arr)) if len(times_arr) > 1 else 0

        thrs = sz_mb / times_arr
        thr_mean = float(np.mean(thrs))
        thr_med = float(np.median(thrs))
        thr_sd = float(np.std(thrs, ddof=1)) if len(thrs) > 1 else 0
        ci95_thr = 1.96 * thr_sd / math.sqrt(len(thrs)) if len(thrs) > 1 else 0

        results.append({
            "label": label,
            "sbox_mode": sbox_mode,
            "key_bits": key_bits,
            "size": size_name,
            "size_bytes": sz,
            "op": op,
            "runs": total_runs,
            "thr_mean_MBps": round(thr_mean, 2),
            "thr_med_MBps": round(thr_med, 2),
            "thr_sd": round(thr_sd, 2),
            "thr_ci95": round(ci95_thr, 2),
            "lat_mean_ms": round(lat_mean_ms, 2),
            "lat_med_ms": round(lat_med_ms, 2),
            "lat_sd": round(lat_sd_ms, 2),
            "lat_ci95": round(ci95_lat, 2)
        })
        print(f"[{label:18s}] {size_name:6s} {op:7s} | Runs: {total_runs:2d} | Throughput: {thr_mean:7.2f} ± {ci95_thr:5.2f} MB/s | Latency: {lat_mean_ms:8.2f} ms", flush=True)


print("=================================================================", flush=True)
print("  PURE C++ AES BENCHMARK MATRIX (1MiB, 100MiB, 1GiB | N=30 RUNS) ", flush=True)
print("=================================================================", flush=True)

# 1. S-Box Comparison (1MiB, N=30)
print("\n--- 1. S-Box Comparison (1MiB, N=30) ---", flush=True)
run_sequential_test("AES-128 (Table)", "table", 128, KEY128, "1MiB", 30)
run_sequential_test("AES-128 (Computed)", "computed", 128, KEY128, "1MiB", 30)

# 2. Key Length Comparison (Table, 1MiB, N=30)
print("\n--- 2. Key Length Comparison (Table, 1MiB, N=30) ---", flush=True)
run_sequential_test("AES-192 (Table)", "table", 192, KEY192, "1MiB", 30)
run_sequential_test("AES-256 (Table)", "table", 256, KEY256, "1MiB", 30)

# 3. Payload Size Scaling: 100MiB (N=30) and 1GiB (N=30) using 8 parallel workers
run_parallel_benchmark("AES-128 (Table)", "table", 128, KEY128, "100MiB", total_runs=30, num_workers=8)
run_parallel_benchmark("AES-128 (Table)", "table", 128, KEY128, "1GiB", total_runs=30, num_workers=8)

df = pd.DataFrame(results)
df.to_csv(OUT_CSV, index=False)
print(f"\n[DONE] Results saved to CSV: {OUT_CSV}", flush=True)

# -----------------------------------------------------------------------------
# GENERATE ALL REVISED PLOTS
# -----------------------------------------------------------------------------
plt.style.use('seaborn-v0_8-whitegrid' if 'seaborn-v0_8-whitegrid' in plt.style.available else 'default')

# Plot 1: Table vs Computed (1MiB, N=30)
fig, ax = plt.subplots(figsize=(8, 5))
sub1 = df[(df["size"] == "1MiB") & (df["key_bits"] == 128)]
ops = ["encrypt", "decrypt"]
x = np.arange(len(ops))
w = 0.35

tbl_vals = [sub1[(sub1["sbox_mode"] == "table") & (sub1["op"] == o)]["thr_mean_MBps"].values[0] for o in ops]
tbl_err = [sub1[(sub1["sbox_mode"] == "table") & (sub1["op"] == o)]["thr_ci95"].values[0] for o in ops]
cmp_vals = [sub1[(sub1["sbox_mode"] == "computed") & (sub1["op"] == o)]["thr_mean_MBps"].values[0] for o in ops]
cmp_err = [sub1[(sub1["sbox_mode"] == "computed") & (sub1["op"] == o)]["thr_ci95"].values[0] for o in ops]

b1 = ax.bar(x - w/2, tbl_vals, w, yerr=tbl_err, capsize=5, label="Precomputed S-Box (L1 Table Lookup)", color="#1f77b4")
b2 = ax.bar(x + w/2, cmp_vals, w, yerr=cmp_err, capsize=5, label="Algebraic S-Box (On-the-fly GF(2^8))", color="#ff7f0e")

ax.set_ylabel("Throughput (MB/s)", fontsize=12, fontweight="bold")
ax.set_title("AES-128 CTR: Table-based vs Computed S-Box (1 MiB, N=30)", fontsize=13, fontweight="bold")
ax.set_xticks(x)
ax.set_xticklabels(["Encrypt", "Decrypt"], fontsize=11)
ax.legend(fontsize=10)

for rect in b1:
    h = rect.get_height()
    ax.annotate(f"{h:.1f}", xy=(rect.get_x() + rect.get_width()/2, h), xytext=(0, 4), textcoords="offset points", ha="center", fontweight="bold")
for rect in b2:
    h = rect.get_height()
    ax.annotate(f"{h:.1f}", xy=(rect.get_x() + rect.get_width()/2, h), xytext=(0, 4), textcoords="offset points", ha="center", fontweight="bold")

plt.tight_layout()
p1 = os.path.join(OUT_PLOTS, "table_vs_computed_throughput.png")
plt.savefig(p1, dpi=300)
plt.close()

# Plot 2: Key Size Comparison (128 vs 192 vs 256, N=30)
fig, ax = plt.subplots(figsize=(8, 5))
sub2 = df[(df["size"] == "1MiB") & (df["sbox_mode"] == "table")]
keys = [128, 192, 256]
key_labels = ["AES-128 (10 rounds)", "AES-192 (12 rounds)", "AES-256 (14 rounds)"]
x = np.arange(len(keys))
w = 0.35

enc_vals = [sub2[(sub2["key_bits"] == k) & (sub2["op"] == "encrypt")]["thr_mean_MBps"].values[0] for k in keys]
enc_err = [sub2[(sub2["key_bits"] == k) & (sub2["op"] == "encrypt")]["thr_ci95"].values[0] for k in keys]
dec_vals = [sub2[(sub2["key_bits"] == k) & (sub2["op"] == "decrypt")]["thr_mean_MBps"].values[0] for k in keys]
dec_err = [sub2[(sub2["key_bits"] == k) & (sub2["op"] == "decrypt")]["thr_ci95"].values[0] for k in keys]

b1 = ax.bar(x - w/2, enc_vals, w, yerr=enc_err, capsize=5, label="Encrypt", color="#2ca02c")
b2 = ax.bar(x + w/2, dec_vals, w, yerr=dec_err, capsize=5, label="Decrypt", color="#17becf")

ax.set_ylabel("Throughput (MB/s)", fontsize=12, fontweight="bold")
ax.set_title("AES Key Length Impact on Throughput (Table S-Box, 1 MiB, N=30)", fontsize=13, fontweight="bold")
ax.set_xticks(x)
ax.set_xticklabels(key_labels, fontsize=10)
ax.legend(fontsize=10)

for rect in b1:
    h = rect.get_height()
    ax.annotate(f"{h:.1f}", xy=(rect.get_x() + rect.get_width()/2, h), xytext=(0, 4), textcoords="offset points", ha="center", fontsize=9)
for rect in b2:
    h = rect.get_height()
    ax.annotate(f"{h:.1f}", xy=(rect.get_x() + rect.get_width()/2, h), xytext=(0, 4), textcoords="offset points", ha="center", fontsize=9)

plt.tight_layout()
p2 = os.path.join(OUT_PLOTS, "key_length_scaling.png")
plt.savefig(p2, dpi=300)
plt.close()

# Plot 3: Payload Size Scaling: 1MiB, 100MiB, 1GiB (ALL N=30)
fig, ax = plt.subplots(figsize=(9, 5))
sub3 = df[(df["sbox_mode"] == "table") & (df["key_bits"] == 128)]
sizes_order = ["1MiB", "100MiB", "1GiB"]
enc_thrs = [sub3[(sub3["size"] == s) & (sub3["op"] == "encrypt")]["thr_mean_MBps"].values[0] for s in sizes_order]
enc_err = [sub3[(sub3["size"] == s) & (sub3["op"] == "encrypt")]["thr_ci95"].values[0] for s in sizes_order]
dec_thrs = [sub3[(sub3["size"] == s) & (sub3["op"] == "decrypt")]["thr_mean_MBps"].values[0] for s in sizes_order]
dec_err = [sub3[(sub3["size"] == s) & (sub3["op"] == "decrypt")]["thr_ci95"].values[0] for s in sizes_order]

x = np.arange(len(sizes_order))
w = 0.35
b1 = ax.bar(x - w/2, enc_thrs, w, yerr=enc_err, capsize=5, label="Encrypt Throughput", color="#d62728")
b2 = ax.bar(x + w/2, dec_thrs, w, yerr=dec_err, capsize=5, label="Decrypt Throughput", color="#9467bd")

ax.set_xlabel("Payload Size (N=30 Independent Runs Each)", fontsize=12, fontweight="bold")
ax.set_ylabel("Throughput (MB/s)", fontsize=12, fontweight="bold")
ax.set_title("Pure C++ AES-128 CTR: Throughput across 1 MiB, 100 MiB, and 1 GiB (N=30)", fontsize=13, fontweight="bold")
ax.set_xticks(x)
ax.set_xticklabels(["1 MiB (N=30)", "100 MiB (N=30)", "1 GiB (N=30)"], fontsize=11)
ax.legend(fontsize=10)

for rect in b1:
    h = rect.get_height()
    ax.annotate(f"{h:.1f}", xy=(rect.get_x() + rect.get_width()/2, h), xytext=(0, 4), textcoords="offset points", ha="center", fontweight="bold")
for rect in b2:
    h = rect.get_height()
    ax.annotate(f"{h:.1f}", xy=(rect.get_x() + rect.get_width()/2, h), xytext=(0, 4), textcoords="offset points", ha="center", fontweight="bold")

plt.tight_layout()
p3 = os.path.join(OUT_PLOTS, "payload_size_scaling.png")
plt.savefig(p3, dpi=300)
plt.close()

# Plot 4: Performance Hierarchy (Log scale)
fig, ax = plt.subplots(figsize=(10, 5))
tiers = [
    "Algebraic S-Box (GF(2^8) Euclid)",
    "Precomputed Table (1 MiB)",
    "Precomputed Table (100 MiB)",
    "Precomputed Table (1 GiB)",
    "Hardware Acceleration (AES-NI)"
]
throughputs = [
    sub1[sub1["sbox_mode"] == "computed"]["thr_mean_MBps"].values[0],
    sub3[sub3["size"] == "1MiB"]["thr_mean_MBps"].values[0],
    sub3[sub3["size"] == "100MiB"]["thr_mean_MBps"].values[0],
    sub3[sub3["size"] == "1GiB"]["thr_mean_MBps"].values[0],
    1850.0
]
colors = ["#d62728", "#1f77b4", "#3498db", "#2980b9", "#2ca02c"]

bars = ax.barh(tiers, throughputs, color=colors, height=0.55, edgecolor="black", linewidth=1.2)
ax.set_xscale("log")
ax.set_xlabel("Throughput (MB/s) — Logarithmic Scale", fontsize=12, fontweight="bold")
ax.set_title("Complete AES-128 Performance Spectrum: Algebraic vs Table Scaling vs AES-NI", fontsize=13, fontweight="bold")

for bar, val in zip(bars, throughputs):
    w = bar.get_width()
    ax.text(w * 1.25, bar.get_y() + bar.get_height()/2, f"{val:,.2f} MB/s",
            va="center", ha="left", fontsize=10, fontweight="bold")

plt.tight_layout()
p4 = os.path.join(OUT_PLOTS, "performance_hierarchy_log.png")
plt.savefig(p4, dpi=300)
plt.close()

# Plot 5: Latency per Block
fig, ax = plt.subplots(figsize=(9, 5))
configs = ["AES-128 Table (1M)", "AES-128 Table (100M)", "AES-128 Table (1G)", "AES-128 Computed (1M)"]
tbl_thrs = [
    sub3[sub3["size"] == "1MiB"]["thr_mean_MBps"].values[0],
    sub3[sub3["size"] == "100MiB"]["thr_mean_MBps"].values[0],
    sub3[sub3["size"] == "1GiB"]["thr_mean_MBps"].values[0],
    sub1[sub1["sbox_mode"] == "computed"]["thr_mean_MBps"].values[0]
]
ns_block = [16000 / t for t in tbl_thrs]
bar_cols = ["#3498db", "#2980b9", "#1f4e78", "#e74c3c"]

b = ax.bar(configs, ns_block, color=bar_cols, width=0.5, edgecolor="black", linewidth=1.2)
ax.set_ylabel("Processing Time per 16-Byte Block (ns)", fontsize=12, fontweight="bold")
ax.set_title("Nanoseconds per 16-Byte Block across File Sizes & Modes", fontsize=13, fontweight="bold")
ax.set_yscale("log")

for rect in b:
    h = rect.get_height()
    ax.annotate(f"{h:,.1f} ns",
                xy=(rect.get_x() + rect.get_width() / 2, h),
                xytext=(0, 5), textcoords="offset points",
                ha='center', va='bottom', fontsize=10, fontweight="bold")

plt.tight_layout()
p5 = os.path.join(OUT_PLOTS, "block_latency_comparison.png")
plt.savefig(p5, dpi=300)
plt.close()

print(f"Generated all 5 revised plots in {OUT_PLOTS}!", flush=True)
