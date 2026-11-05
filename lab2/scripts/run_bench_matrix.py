import os
import sys
import time
import subprocess
import math
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

TOOL = r"D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs\lab2\release\windows\aestool.exe"
WORK = os.path.join(os.environ.get("TEMP", "C:/Temp"), "lab2_bench_work")
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
    "500MiB": 500 * 1024 * 1024
}

files = {}
for name, sz in SIZES.items():
    p = os.path.join(WORK, f"plain_{name}.bin")
    if not os.path.exists(p) or os.path.getsize(p) != sz:
        print(f"Creating {name} payload ({sz} bytes)...")
        with open(p, "wb") as f:
            chunk = os.urandom(64 * 1024)
            written = 0
            while written < sz:
                to_write = min(len(chunk), sz - written)
                f.write(chunk[:to_write])
                written += to_write
    files[name] = p

results = []

def run_test(label, sbox_mode, key_bits, key_file, size_name, runs):
    p_in = files[size_name]
    p_ct = os.path.join(WORK, f"ct_{label}_{size_name}.bin")
    p_out = os.path.join(WORK, f"out_{label}_{size_name}.bin")
    sz = SIZES[size_name]
    sz_mb = sz / (1024 * 1024)

    # Warm-up 1 run
    subprocess.run([TOOL, "encrypt", "--mode", "ctr", "--key", key_file, "--iv", IV_FILE,
                    "--in", p_in, "--out", p_ct, "--sbox", sbox_mode, "--bits", str(key_bits)], check=True)

    # Encrypt runs
    enc_times = []
    for r in range(runs):
        t0 = time.perf_counter()
        subprocess.run([TOOL, "encrypt", "--mode", "ctr", "--key", key_file, "--iv", IV_FILE,
                        "--in", p_in, "--out", p_ct, "--sbox", sbox_mode, "--bits", str(key_bits)], check=True)
        t1 = time.perf_counter()
        enc_times.append(t1 - t0)

    # Decrypt runs
    dec_times = []
    for r in range(runs):
        t0 = time.perf_counter()
        subprocess.run([TOOL, "decrypt", "--mode", "ctr", "--key", key_file, "--iv", IV_FILE,
                        "--in", p_ct, "--out", p_out, "--sbox", sbox_mode, "--bits", str(key_bits)], check=True)
        t1 = time.perf_counter()
        dec_times.append(t1 - t0)

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
        print(f"[{label:18s}] {size_name:6s} {op:7s} | Throughput: {thr_mean:7.2f} ± {ci95_thr:5.2f} MB/s | Latency: {lat_mean_ms:8.2f} ms", flush=True)

print("=== Starting Pure C++ AES Benchmark Matrix ===", flush=True)

# 1. AES-128 Table vs Computed S-Box (1MiB, N=30)
print("\n--- 1. S-Box Comparison (1MiB, N=30) ---", flush=True)
run_test("AES-128 (Table)", "table", 128, KEY128, "1MiB", 30)
run_test("AES-128 (Computed)", "computed", 128, KEY128, "1MiB", 30)

# 2. Key Size Scaling (AES-192, AES-256 Table, 1MiB, N=30)
print("\n--- 2. Key Length Comparison (Table, 1MiB, N=30) ---", flush=True)
run_test("AES-192 (Table)", "table", 192, KEY192, "1MiB", 30)
run_test("AES-256 (Table)", "table", 256, KEY256, "1MiB", 30)

# 3. Payload Size Scaling (Table AES-128, 100MiB N=5, 500MiB N=2)
print("\n--- 3. Payload Size Scaling (Table AES-128) ---", flush=True)
run_test("AES-128 (Table)", "table", 128, KEY128, "100MiB", 5)
run_test("AES-128 (Table)", "table", 128, KEY128, "500MiB", 2)

df = pd.DataFrame(results)
df.to_csv(OUT_CSV, index=False)
print(f"\n[DONE] CSV saved to {OUT_CSV}", flush=True)

# Generate Plots
plt.style.use('seaborn-v0_8-whitegrid' if 'seaborn-v0_8-whitegrid' in plt.style.available else 'default')

# Plot 1: Table vs Computed S-Box (1MiB)
fig, ax = plt.subplots(figsize=(8, 5))
sub1 = df[(df["size"] == "1MiB") & (df["key_bits"] == 128)]
ops = ["encrypt", "decrypt"]
x = np.arange(len(ops))
w = 0.35

tbl_vals = [sub1[(sub1["sbox_mode"] == "table") & (sub1["op"] == o)]["thr_mean_MBps"].values[0] for o in ops]
tbl_err = [sub1[(sub1["sbox_mode"] == "table") & (sub1["op"] == o)]["thr_ci95"].values[0] for o in ops]
cmp_vals = [sub1[(sub1["sbox_mode"] == "computed") & (sub1["op"] == o)]["thr_mean_MBps"].values[0] for o in ops]
cmp_err = [sub1[(sub1["sbox_mode"] == "computed") & (sub1["op"] == o)]["thr_ci95"].values[0] for o in ops]

b1 = ax.bar(x - w/2, tbl_vals, w, yerr=tbl_err, capsize=5, label="Precomputed S-Box (Table Lookup)", color="#1f77b4")
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

# Plot 2: Key Size Comparison (128 vs 192 vs 256)
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

# Plot 3: Payload Size Scaling (1MiB, 100MiB, 500MiB)
fig, ax = plt.subplots(figsize=(8, 5))
sub3 = df[(df["sbox_mode"] == "table") & (df["key_bits"] == 128)]
sizes_order = ["1MiB", "100MiB", "500MiB"]
enc_thrs = [sub3[(sub3["size"] == s) & (sub3["op"] == "encrypt")]["thr_mean_MBps"].values[0] for s in sizes_order]
dec_thrs = [sub3[(sub3["size"] == s) & (sub3["op"] == "decrypt")]["thr_mean_MBps"].values[0] for s in sizes_order]

ax.plot(sizes_order, enc_thrs, marker='o', linewidth=2.5, label='Encrypt Throughput', color='#d62728')
ax.plot(sizes_order, dec_thrs, marker='s', linewidth=2.5, linestyle='--', label='Decrypt Throughput', color='#9467bd')

ax.set_xlabel("Payload Size", fontsize=12, fontweight="bold")
ax.set_ylabel("Throughput (MB/s)", fontsize=12, fontweight="bold")
ax.set_title("AES-128 CTR: Throughput vs Payload Size (Table S-Box)", fontsize=13, fontweight="bold")
ax.legend(fontsize=10)
ax.grid(True, linestyle="--", alpha=0.7)

for i, txt in enumerate(enc_thrs):
    ax.annotate(f"{txt:.1f} MB/s", (sizes_order[i], txt), textcoords="offset points", xytext=(0, 8), ha='center', fontweight="bold")

plt.tight_layout()
p3 = os.path.join(OUT_PLOTS, "payload_size_scaling.png")
plt.savefig(p3, dpi=300)
plt.close()

print(f"Generated 3 plots:\n1. {p1}\n2. {p2}\n3. {p3}", flush=True)
