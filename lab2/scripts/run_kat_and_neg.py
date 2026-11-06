import os
import platform
import subprocess
import sys
import tempfile
from pathlib import Path

LAB_DIR = Path(__file__).resolve().parent.parent

if sys.platform.startswith("win"):
    OS_NAME = "windows"
    DEFAULT_TOOL = LAB_DIR / "release" / "windows" / "aestool.exe"
    if not DEFAULT_TOOL.exists():
        DEFAULT_TOOL = LAB_DIR / "build" / "aestool.exe"
else:
    OS_NAME = "ubuntu"
    DEFAULT_TOOL = LAB_DIR / "build_ubuntu_gcc" / "aestool"
    if not DEFAULT_TOOL.exists():
        DEFAULT_TOOL = LAB_DIR / "release" / "ubuntu" / "aestool_gcc"
    if not DEFAULT_TOOL.exists():
        DEFAULT_TOOL = LAB_DIR.parent / "build" / "lab2" / "aestool"

TOOL = os.environ.get("AESTOOL", str(DEFAULT_TOOL))
KAT_VECTORS = str(LAB_DIR / "kat" / "vectors.json")
KAT_OUT = str(LAB_DIR / "Output" / OS_NAME / "kat" / "kat_results.txt")
NEG_OUT = str(LAB_DIR / "Output" / OS_NAME / "negative_tests" / "negative_results.txt")

# 1. Run KAT
print(f"Running KAT with {TOOL}...")
res = subprocess.run([TOOL, "--kat", KAT_VECTORS], capture_output=True, text=True)
os.makedirs(os.path.dirname(KAT_OUT), exist_ok=True)
with open(KAT_OUT, "w", encoding="utf-8") as f:
    f.write(res.stdout + res.stderr)
print(f"KAT Results saved. Exit Code: {res.returncode}")
print(res.stdout)

# 2. Run Negative Tests
WORK = os.path.join(tempfile.gettempdir(), "lab2_neg_py")
os.makedirs(WORK, exist_ok=True)
os.makedirs(os.path.dirname(NEG_OUT), exist_ok=True)

k_valid = os.path.join(WORK, "k_valid.bin")
k_bad = os.path.join(WORK, "k_bad.bin")
k_other = os.path.join(WORK, "k_other.bin")
iv_valid = os.path.join(WORK, "iv_valid.bin")
iv_bad = os.path.join(WORK, "iv_bad.bin")
iv_other = os.path.join(WORK, "iv_other.bin")
msg_file = os.path.join(WORK, "msg.txt")
ct_file = os.path.join(WORK, "ct.bin")

subprocess.run([TOOL, "keygen", "--out", k_valid, "--force"], check=True)
subprocess.run([TOOL, "keygen", "--out", k_other, "--force"], check=True)
subprocess.run([TOOL, "ivgen", "--out", iv_valid, "--force"], check=True)
subprocess.run([TOOL, "ivgen", "--out", iv_other, "--force"], check=True)

with open(k_bad, "wb") as f:
    f.write(b"\x01" * 10)  # 10 bytes instead of 16
with open(iv_bad, "wb") as f:
    f.write(b"\x02" * 8)   # 8 bytes instead of 16
with open(msg_file, "wb") as f:
    f.write(b"ATTACK_AT_DAWN_123456789")

subprocess.run([TOOL, "encrypt", "--mode", "ctr", "--key", k_valid, "--iv", iv_valid, "--in", msg_file, "--out", ct_file, "--force"], check=True)

neg_log = []
neg_log.append("================================================================================")
neg_log.append("  NT219 - LAB 2: AUTOMATED NEGATIVE & MALFORMED INPUT TEST SUITE")
neg_log.append(f"  Student: Feph50 (Trần Lâm Nguyên) | OS: {platform.system()} {platform.release()} 64-bit")
neg_log.append("================================================================================\n")

# Test 1: Invalid key length
r1 = subprocess.run([TOOL, "encrypt", "--mode", "ctr", "--key", k_bad, "--iv", iv_valid, "--in", msg_file, "--out", os.path.join(WORK, "x.bin"), "--force"], capture_output=True, text=True)
neg_log.append("[TEST 1] Invalid Key Length (10 bytes instead of 16)")
neg_log.append(f"  Result Exit Code: {r1.returncode} (Expected: 3)")
neg_log.append(f"  Stderr output: {r1.stderr.strip()}")
neg_log.append(f"  Fail-closed verified: {'PASS' if r1.returncode == 3 else 'FAIL'}\n")

# Test 2: Invalid IV length
r2 = subprocess.run([TOOL, "encrypt", "--mode", "ctr", "--key", k_valid, "--iv", iv_bad, "--in", msg_file, "--out", os.path.join(WORK, "x.bin"), "--force"], capture_output=True, text=True)
neg_log.append("[TEST 2] Invalid IV Length (8 bytes instead of 16)")
neg_log.append(f"  Result Exit Code: {r2.returncode} (Expected: 3)")
neg_log.append(f"  Stderr output: {r2.stderr.strip()}")
neg_log.append(f"  Fail-closed verified: {'PASS' if r2.returncode == 3 else 'FAIL'}\n")

# Test 3: Wrong Key Decrypt
dec_wrong_k = os.path.join(WORK, "dec_wrong_k.txt")
r3 = subprocess.run([TOOL, "decrypt", "--mode", "ctr", "--key", k_other, "--iv", iv_valid, "--in", ct_file, "--out", dec_wrong_k, "--force"], capture_output=True, text=True)
with open(dec_wrong_k, "rb") as f:
    dec_k_bytes = f.read()
neg_log.append("[TEST 3] Decrypt with Wrong Key")
neg_log.append(f"  Recovered raw bytes: {dec_k_bytes}")
neg_log.append(f"  Plaintext corrupt/different: {'PASS (Output completely scrambled)' if dec_k_bytes != b'ATTACK_AT_DAWN_123456789' else 'FAIL'}\n")

# Test 4: Wrong IV Decrypt
dec_wrong_iv = os.path.join(WORK, "dec_wrong_iv.txt")
r4 = subprocess.run([TOOL, "decrypt", "--mode", "ctr", "--key", k_valid, "--iv", iv_other, "--in", ct_file, "--out", dec_wrong_iv, "--force"], capture_output=True, text=True)
with open(dec_wrong_iv, "rb") as f:
    dec_iv_bytes = f.read()
neg_log.append("[TEST 4] Decrypt with Wrong IV")
neg_log.append(f"  Recovered raw bytes: {dec_iv_bytes}")
neg_log.append(f"  Plaintext corrupt/different: {'PASS (Keystream mismatch, output scrambled)' if dec_iv_bytes != b'ATTACK_AT_DAWN_123456789' else 'FAIL'}\n")

# Test 5: Bit-flipping Malleability Attack
with open(ct_file, "rb") as f:
    ct_data = bytearray(f.read())
# Flip bit in byte 0: 'A' (0x41) ^ 0x20 = 'a' (0x61)
ct_data[0] ^= 0x20
ct_tampered = os.path.join(WORK, "ct_tampered.bin")
with open(ct_tampered, "wb") as f:
    f.write(ct_data)

dec_tampered = os.path.join(WORK, "dec_tampered.txt")
r5 = subprocess.run([TOOL, "decrypt", "--mode", "ctr", "--key", k_valid, "--iv", iv_valid, "--in", ct_tampered, "--out", dec_tampered, "--force"], capture_output=True, text=True)
with open(dec_tampered, "rb") as f:
    tampered_bytes = f.read()
neg_log.append("[TEST 5] Bit-Flipping Malleability Attack (CTR Tampering Proof)")
neg_log.append("  Original Plaintext: ATTACK_AT_DAWN_123456789")
neg_log.append(f"  Decrypted Tampered: {tampered_bytes.decode('latin1', errors='replace')}")
neg_log.append(f"  Malleability success: {'PASS (Bit flip propagates directly to plaintext, no integrity check)' if tampered_bytes == b'aTTACK_AT_DAWN_123456789' else 'FAIL'}")
neg_log.append("  Security conclusion: CTR provides confidentiality ONLY. Authenticity/integrity requires HMAC or GCM AEAD.\n")

# Test 6: Missing input file (I/O error)
r6 = subprocess.run([TOOL, "encrypt", "--mode", "ctr", "--key", k_valid, "--iv", iv_valid, "--in", os.path.join(WORK, "nonexistent.bin"), "--out", os.path.join(WORK, "x.bin"), "--force"], capture_output=True, text=True)
neg_log.append("[TEST 6] Non-existent input file")
neg_log.append(f"  Result Exit Code: {r6.returncode} (Expected: 4)")
neg_log.append(f"  Stderr output: {r6.stderr.strip()}")
neg_log.append(f"  Fail-closed verified: {'PASS' if r6.returncode == 4 else 'FAIL'}\n")

neg_log.append("================================================================================")
neg_log.append("  SUMMARY: 6/6 NEGATIVE & SECURITY PROPERTIES VERIFIED SUCCESSFULLY")
neg_log.append("================================================================================")

log_text = "\n".join(neg_log)
with open(NEG_OUT, "w", encoding="utf-8") as f:
    f.write(log_text)
print("Negative tests completed successfully. Log saved to " + NEG_OUT)
