# Lab 1: Symmetric Encryption with Crypto++

## 1. Overview & Objectives

Implement and evaluate AES symmetric encryption and decryption using the **Crypto++ (8.9+)** library across 8 operating modes (ECB, CBC, CFB, OFB, CTR, XTS, CCM, GCM). The project enforces rigorous cryptographic security hygiene (fail-closed architecture, nonce-reuse prevention, IV length validation, ECB usage restriction), validates mathematically against NIST Known Answer Tests (KAT), and conducts comprehensive cross-platform performance benchmarking between Windows and Linux (Ubuntu).

### Topics Covered
- **AES block cipher** with 128, 192, and 256-bit key sizes (plus 512-bit for XTS dual-key).
- **Block & Stream cipher modes**: ECB, CBC, CFB, OFB, CTR, XTS.
- **Authenticated Encryption with Associated Data (AEAD)**: GCM (Galois/Counter Mode) and CCM (Counter with CBC-MAC).
- **Security Misuse Resistance**: Nonce registry tracking with SHA-256, ECB size thresholds (>16 KiB blocked), and fail-closed error handling.
- **Verification & Testing**: NIST SP 800-38A, SP 800-38C, GCM, and IEEE 1619 KAT vectors, plus negative test matrices.
- **Performance Benchmarking**: Latency, throughput, confidence intervals (CI 95%), and linear cost models for tag overhead.

---

## 2. Supported Modes

| Mode | Type | IV / Nonce Length | Auth Tag Length | Security Status & Use Case |
| :--- | :---: | :---: | :---: | :--- |
| **ECB** | Block | None (0 B) | None | **Insecure** (leaks patterns); blocked for inputs > 16 KiB without `--allow-ecb` |
| **CBC** | Block | 16 bytes | None | Standard legacy block mode; requires PKCS#7 padding |
| **CFB** | Stream | 16 bytes | None | Self-synchronizing stream mode (CFB-128) |
| **OFB** | Stream | 16 bytes | None | Synchronous keystream generation; error non-propagating |
| **CTR** | Stream | 16 bytes | None | Counter mode; highly parallelizable; **catastrophic if nonce reused** |
| **XTS** | Storage | 16 bytes (Tweak) | None | IEEE 1619 disk encryption (requires 256 or 512-bit dual key); no integrity |
| **GCM** | AEAD | 12 bytes (NIST) | 16 bytes | **Recommended**; single-pass AEAD with hardware PCLMULQDQ acceleration |
| **CCM** | AEAD | 7--13 bytes (12 B def) | 4, 8, 16 bytes | Two-pass AEAD (CTR + CBC-MAC); constrained / embedded networking |

---

## 3. Source Structure

```
lab1/
├── CMakeLists.txt              # Cross-platform CMake configuration
├── require.md                  # Lab 1 formal specification and requirements
├── README.md                   # Unified project documentation and run guide
├── kat/
│   └── vectors.json            # 29 NIST / IEEE Known Answer Test vectors
├── lab-guide/
│   ├── windows_guide.md        # Dedicated Windows execution instructions
│   └── ubuntu_guide.md         # Dedicated Ubuntu execution instructions
├── src/
│   ├── cryptopp_all.hpp        # Unified header loading Crypto++ modules
│   ├── aescore.hpp             # Decoupled core encryption interface API
│   ├── aescore.cpp             # AES 8 modes, AEAD, Nonce registry, File/Hex/Base64 I/O
│   └── main.cpp                # CLI driver, KAT runner, Benchmark engine
├── tests/
│   ├── test_core.cpp           # Catch2 unit tests (8 test cases, negative tests)
│   └── cli_negative.cmake      # CMake integration script for CLI negative testing
├── scripts/
│   ├── build_windows.bat       # Automated build script for Windows (MSVC/GCC/Clang)
│   ├── build_linux.sh          # Automated build script for Linux (GCC/Clang)
│   └── plot_bench.py           # Python data processing and chart generation script
├── release/
│   ├── ubuntu/                 # Prebuilt Ubuntu binaries
│   └── windows/                # Prebuilt Windows binaries
├── output/
│   ├── windows/                # Test logs, CSVs, and plots on Windows
│   ├── ubuntu/                 # Test logs, CSVs, and plots on Linux (Ubuntu)
│   └── compare/                # Cross-platform comparison graphs and summary CSVs
│       └── mode_cmp/           # In-depth 4 core comparison plots and datasets
└── docs/
    ├── report_outline.md       # Report structural outline
    ├── report.tex              # Complete academic report in LaTeX
    └── lab1-main-report.md     # Full comprehensive technical report (Markdown)
```

---

## 4. Dependencies & Prerequisites

### Dependency list (with versions)

| Dependency | Required / tested version | Purpose |
| :--- | :--- | :--- |
| CMake | >= 3.20 (tested 4.2 - 4.4) | Build system (out-of-source builds) |
| C++ compiler | C++17. Tested: MSVC 19.51 (Visual Studio 2026), MinGW-w64 GCC 16.2 (MSYS2), GCC 15.2 (Ubuntu 26.04), Clang | Compilation |
| **Crypto++** | **8.9.0** (>= 8.9 recommended) | **The only cryptographic library used** (AES modes, `AutoSeededRandomPool`, SHA-256) |
| Catch2 | v3 (tested 3.5.3 and distro 3.x) | Unit tests. Used if installed, otherwise CMake downloads v3.5.3 once |
| Python | 3.10+ with `matplotlib`, `pandas`, `numpy` | Optional: benchmark plots |
| PyQt6 | 6.x | Optional: GUI (`gui/aestool_gui.py`) |

**No OpenSSL, libsodium or any other cryptographic library is used or linked** (Lab 1 rule).

### Installation

CMake finds Crypto++ automatically (CMake package config -> pkg-config -> standard prefixes -> `CRYPTOPP_ROOT_DIR`);
**no file has to be edited**.

#### Ubuntu / Debian (LTS)
```bash
sudo apt update
sudo apt install -y build-essential cmake libcrypto++-dev catch2 pkg-config
# optional (plots / GUI):  sudo apt install -y python3 python3-pip && pip3 install matplotlib pandas numpy PyQt6
```

#### Windows - MinGW64 (MSYS2 "MINGW64" shell)
```bash
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
                   mingw-w64-x86_64-crypto++ mingw-w64-x86_64-catch
```
Put `C:\msys64\mingw64\bin` first in `PATH`.

#### Windows - MSVC (Visual Studio 2022/2026 or Build Tools, workload "Desktop development with C++")
```bat
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat -disableMetrics
C:\vcpkg\vcpkg.exe install cryptopp:x64-windows-static
```
Then build from the "x64 Native Tools Command Prompt for VS" with the vcpkg toolchain (see section 5).

#### Any OS - self-built Crypto++
Install Crypto++ to a prefix and pass it: `cmake .. -DCRYPTOPP_ROOT_DIR=/path/to/prefix` (headers in `include/`, library in `lib/`),
or pass the files directly: `-DCryptoPP_LIBRARY=<file> -DCryptoPP_INCLUDE_DIR=<dir>`.

---

## 5. Building the Project

### Standard build (all platforms) - out-of-source

```bash
cd lab1
mkdir build && cd build
cmake ..
cmake --build .
ctest --output-on-failure       # unit tests + NIST KAT + CLI negative tests
./aestool --kat ../kat/vectors.json
```
This works unchanged on **Ubuntu LTS** (system packages above) and on **Windows MinGW64** (MSYS2 shell; add `-G Ninja` if
`mingw32-make` is not installed). On Windows **MSVC** pass the vcpkg toolchain and the static runtime:

```bat
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ^
         -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build .
ctest -C Release --output-on-failure
```
(Omit `-G Ninja` to use the Visual Studio generator; then run `ctest -C Release`.)

### Helper scripts (same steps, nothing to edit)

```bash
scripts/build_linux.sh gcc [--clean] [--install-deps]        # Ubuntu / Debian: configure, build, ctest, KAT
```
```bat
scripts\build_windows.bat msvc                                 REM set VCPKG_ROOT=C:\vcpkg first (MSVC route)
scripts\build_windows.bat gcc                                  REM MinGW64, PATH must contain C:\msys64\mingw64\bin
scripts\build_windows.bat clean
```
Environment overrides: `CRYPTOPP_ROOT_DIR` (self-built Crypto++), `VCPKG_ROOT`, `CMAKE_ARGS` (extra CMake arguments).

### Build options

| CMake option | Default | Meaning |
| :--- | :---: | :--- |
| `BUILD_TESTS` | ON | Build the Catch2 unit tests and register `ctest` tests |
| `AESTOOL_STATIC_RUNTIME` | OFF | Link statically (`-static`, static Crypto++) so the binary needs no DLL/.so. Used for the release binaries |
| `AESTOOL_WARNINGS_AS_ERRORS` | OFF | Developer option: `-Werror` / `/WX` |
| `CRYPTOPP_ROOT_DIR` | - | Install prefix of a self-built Crypto++ |

### Prebuilt binaries

Release binaries (built from this source with `-DAESTOOL_STATIC_RUNTIME=ON`, or `/MT` for MSVC) are in `release/`:

| OS | Binary | Built with |
| :--- | :--- | :--- |
| Ubuntu / Linux x86-64 | `release/ubuntu/aestool_gcc` | GCC 15.2, fully static |
| Windows (MSVC) | `release/windows/aestool_msvc.exe` | MSVC 19.51, `/MT`, vcpkg `x64-windows-static` |
| Windows (MinGW64) | `release/windows/aestool_mingw.exe` | GCC 16.2 (MSYS2), `-static` |

```bash
chmod +x release/ubuntu/aestool_gcc          # (if the executable bit was lost when unpacking the archive)
./release/ubuntu/aestool_gcc --kat kat/vectors.json
```
```powershell
.\release\windows\aestool_msvc.exe --kat kat\vectors.json
```
Run these from the `lab1` root so that relative paths such as `kat/vectors.json` resolve.

### Crypto core as a library (optional bonus, partial)

The cryptographic core is built as a **static library** (`libaescore.a` / `aescore.lib`, target `aescore`; installed with
`cmake --install`) and is separate from the CLI (`src/main.cpp`). A shared library (`.dll` / `.so`) with a C API is **not**
provided, and the PyQt6 GUI (`gui/aestool_gui.py`) drives the compiled `aestool` executable (it contains no cryptographic code)
instead of loading the library.

---

## 6. Windows Execution & Verification Guide (PowerShell)

All commands should be executed from the `lab1` root directory.

### Step 0: Directory Preparation
```powershell
# Create required output directory structure
mkdir -Force output\windows\key_bin
mkdir -Force output\windows\neg_results
mkdir -Force output\windows\benchmarks
```

### Step 1: Known Answer Tests (NIST KAT)
Execute the 29 KAT vectors (NIST SP 800-38A/C, GCM, IEEE 1619) and save logs:
```powershell
.\release\windows\aestool_mingw.exe --kat kat\vectors.json > output\windows\kat_windows_release.txt 2>&1
```

### Step 2: Automated Unit Tests (Catch2 via CTest)
```powershell
cd build_windows_gcc
ctest --output-on-failure > ..\output\windows\test_windows_gcc.txt 2>&1
cd ..
```

### Step 3: Prepare Test Data for Negative Testing
Generate keys, IVs, plaintexts, and valid ciphertexts for tamper testing:
```powershell
# Helper function for CSPRNG random generation
function New-RandomFile {
    param([string]$Path, [int]$Bytes)
    $buf = New-Object byte[] $Bytes
    [System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($buf)
    [System.IO.File]::WriteAllBytes($Path, $buf)
}

New-RandomFile "output\windows\key_bin\key.bin"       32
New-RandomFile "output\windows\key_bin\key_wrong.bin" 32
New-RandomFile "output\windows\key_bin\iv.bin"        16
New-RandomFile "output\windows\key_bin\iv_wrong.bin"  16
New-RandomFile "output\windows\key_bin\short_iv.bin"  8

# Create sample plaintext
"Hello World plaintext for testing negative cases" | Out-File -Encoding ascii output\windows\key_bin\plain.txt

# Encrypt to create valid reference ciphertexts (CBC & GCM)
.\release\windows\aestool_mingw.exe encrypt --mode cbc --key output\windows\key_bin\key.bin --in output\windows\key_bin\plain.txt --out output\windows\key_bin\ct.bin
.\release\windows\aestool_mingw.exe encrypt --mode gcm --aead --key output\windows\key_bin\key.bin --in output\windows\key_bin\plain.txt --out output\windows\key_bin\ct_gcm.bin
```

### Step 4: Negative Testing (Fail-Closed Validations)
Execute negative test cases and record outputs into `output\windows\neg_results\`:
```powershell
# 1. Test wrong key (decryption failure)
.\release\windows\aestool_mingw.exe decrypt --mode cbc --key output\windows\key_bin\key_wrong.bin --in output\windows\key_bin\ct.bin --out output\windows\key_bin\out.txt 2>&1 | Tee-Object -FilePath output\windows\neg_results\neg_key.txt

# 2. Test wrong IV
.\release\windows\aestool_mingw.exe decrypt --mode cbc --key output\windows\key_bin\key.bin --iv output\windows\key_bin\iv_wrong.bin --in output\windows\key_bin\ct.bin --out output\windows\key_bin\out.txt 2>&1 | Tee-Object -FilePath output\windows\neg_results\neg_iv.txt

# 3. Test wrong AEAD Tag (GCM authentication failure)
.\release\windows\aestool_mingw.exe decrypt --mode gcm --aead --key output\windows\key_bin\key.bin --in output\windows\key_bin\ct_gcm.bin --out output\windows\key_bin\out.txt 2>&1 | Tee-Object -FilePath output\windows\neg_results\neg_tag.txt

# 4. Test invalid IV length (rejection on entry)
.\release\windows\aestool_mingw.exe encrypt --mode cbc --key output\windows\key_bin\key.bin --iv output\windows\key_bin\short_iv.bin --in output\windows\key_bin\plain.txt --out output\windows\key_bin\ct.bin 2>&1 | Tee-Object -FilePath output\windows\neg_results\neg_ivlen.txt

# 5. Test ECB policy restriction on large payload without --allow-ecb
fsutil file createnew output\windows\key_bin\large.bin 32768
.\release\windows\aestool_mingw.exe encrypt --mode ecb --key output\windows\key_bin\key.bin --in output\windows\key_bin\large.bin --out output\windows\key_bin\ct.bin 2>&1 | Tee-Object -FilePath output\windows\neg_results\neg_ecb.txt
```

### Step 5: Performance Benchmarking & Graph Generation
Protocol (built into `aestool bench`): warm-up 1.5 s for every mode and direction, blocks of ~1000 operations (fewer for big payloads, see
`--budget-mb`), N = 30 runs per case (30-100 allowed), mean / median / SD / 95% CI. Takes about 6 minutes on one core.

```powershell
# Hardware table for the report
Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors
Get-CimInstance Win32_ComputerSystem | Select-Object @{n='RAM_GB';e={[math]::Round($_.TotalPhysicalMemory/1GB,1)}}
Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version
Get-PhysicalDisk | Select-Object FriendlyName,MediaType

# Fairness: High performance power plan, close other programs
powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c

# Benchmark (all 8 modes x Enc/Dec x 6 payload sizes). The file name must contain "gcc" or "msvc" (see --prefer below)
mkdir -Force output\windows\benchmarks
.\release\windows\aestool_mingw.exe bench --runs 30 --out output\windows\benchmarks\bench_windows_gcc.csv
.\release\windows\aestool_msvc.exe  bench --runs 30 --out output\windows\benchmarks\bench_windows_msvc.csv

# Plots, analyses and the Windows-vs-Ubuntu comparison (needs the Ubuntu CSV in output\ubuntu\benchmarks\, see Linux Step 5)
pip install pandas matplotlib numpy
python scripts\plot_bench.py --auto --prefer gcc
python ..\scripts\csv_to_stats_table.py output\windows\benchmarks\bench_windows_gcc.csv --size 1048576 > output\windows\stats_1MiB.md
```

---

## 7. Linux (Ubuntu) Execution & Verification Guide (Bash)

All commands should be executed from the `lab1` root directory.

### Step 0: Directory Preparation
```bash
# Create required output directory structure
mkdir -p output/ubuntu/key_bin
mkdir -p output/ubuntu/neg_results
mkdir -p output/ubuntu/benchmarks
```

### Step 1: Known Answer Tests (NIST KAT)
```bash
# KAT - GCC
./release/ubuntu/aestool_gcc --kat kat/vectors.json > output/ubuntu/kat_ubuntu_release.txt 2>&1
```

### Step 2: Automated Unit Tests (Catch2 via CTest)
```bash
# Unit Test - GCC
cd build_ubuntu_gcc
ctest --output-on-failure > ../output/ubuntu/test_ubuntu_gcc.txt 2>&1
cd ..

# Unit Test - Clang
cd build_ubuntu_clang
ctest --output-on-failure > ../output/ubuntu/test_ubuntu_clang.txt 2>&1
cd ..
```

### Step 3: Prepare Test Data for Negative Testing
```bash
# 1. Generate keys & IVs using CSPRNG (/dev/urandom)
head -c 32 /dev/urandom > output/ubuntu/key_bin/key.bin
head -c 32 /dev/urandom > output/ubuntu/key_bin/key_wrong.bin
head -c 16 /dev/urandom > output/ubuntu/key_bin/iv.bin
head -c 16 /dev/urandom > output/ubuntu/key_bin/iv_wrong.bin
head -c 8  /dev/urandom > output/ubuntu/key_bin/short_iv.bin

# 2. Generate sample plaintext
echo "Hello World plaintext for testing negative cases" > output/ubuntu/key_bin/plain.txt

# 3. Encrypt plain text to create valid reference ciphertexts (CBC & GCM)
./release/ubuntu/aestool_gcc encrypt --mode cbc --key output/ubuntu/key_bin/key.bin --in output/ubuntu/key_bin/plain.txt --out output/ubuntu/key_bin/ct.bin
./release/ubuntu/aestool_gcc encrypt --mode gcm --aead --key output/ubuntu/key_bin/key.bin --in output/ubuntu/key_bin/plain.txt --out output/ubuntu/key_bin/ct_gcm.bin
```

### Step 4: Negative Testing (Fail-Closed Validations)
```bash
# 1. Test wrong key
./release/ubuntu/aestool_gcc decrypt --mode cbc --key output/ubuntu/key_bin/key_wrong.bin --in output/ubuntu/key_bin/ct.bin --out output/ubuntu/key_bin/out.txt 2>&1 | tee output/ubuntu/neg_results/neg_key.txt

# 2. Test wrong IV
./release/ubuntu/aestool_gcc decrypt --mode cbc --key output/ubuntu/key_bin/key.bin --iv output/ubuntu/key_bin/iv_wrong.bin --in output/ubuntu/key_bin/ct.bin --out output/ubuntu/key_bin/out.txt 2>&1 | tee output/ubuntu/neg_results/neg_iv.txt

# 3. Test wrong AEAD Tag (GCM)
./release/ubuntu/aestool_gcc decrypt --mode gcm --aead --key output/ubuntu/key_bin/key.bin --in output/ubuntu/key_bin/ct_gcm.bin --out output/ubuntu/key_bin/out.txt 2>&1 | tee output/ubuntu/neg_results/neg_tag.txt

# 4. Test wrong IV length
./release/ubuntu/aestool_gcc encrypt --mode cbc --key output/ubuntu/key_bin/key.bin --iv output/ubuntu/key_bin/short_iv.bin --in output/ubuntu/key_bin/plain.txt --out output/ubuntu/key_bin/ct.bin 2>&1 | tee output/ubuntu/neg_results/neg_ivlen.txt

# 5. Test ECB policy restriction on large payload without --allow-ecb
dd if=/dev/zero of=output/ubuntu/key_bin/large.bin bs=1K count=32
./release/ubuntu/aestool_gcc encrypt --mode ecb --key output/ubuntu/key_bin/key.bin --in output/ubuntu/key_bin/large.bin --out output/ubuntu/key_bin/ct.bin 2>&1 | tee output/ubuntu/neg_results/neg_ecb.txt
```

### Step 5: Performance Benchmarking & Graph Generation
Same protocol as on Windows (about 6 minutes).

```bash
# Hardware table for the report
lscpu | grep -E "Model name|^CPU\(s\)|Thread|Core|MHz"; free -h; lsblk -d -o NAME,ROTA,SIZE,MODEL; uname -a; lsb_release -d; g++ --version | head -1

# Fairness: performance governor (needs a real Linux machine; inside WSL there is no cpufreq)
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Benchmark (all 8 modes x Enc/Dec x 6 payload sizes)
mkdir -p output/ubuntu/benchmarks
./release/ubuntu/aestool_gcc bench --runs 30 --out output/ubuntu/benchmarks/bench_ubuntu_gcc.csv

# Plots and analyses (needs: pip3 install pandas matplotlib numpy)
python3 scripts/plot_bench.py --auto --prefer gcc
python3 ../scripts/csv_to_stats_table.py output/ubuntu/benchmarks/bench_ubuntu_gcc.csv --size 1048576 > output/ubuntu/stats_1MiB.md
```
Copy `output/ubuntu/benchmarks/bench_ubuntu_gcc.csv` next to the Windows CSV (same relative path) and run
`python scripts/plot_bench.py --auto --prefer gcc` on one machine to produce `output/compare/`.

---

## 8. CLI Command Specification & Examples

## 8.1 PyQt6 GUI

Lab 1 có GUI PyQt6 để gọi `aestool` mà không cần nhập thủ công các tham số
command-line. GUI hoạt động trong thư mục `lab1`, tự tìm executable ở các
thư mục build chuẩn (`build_ubuntu_gcc`, `build_ubuntu_clang`,
`build_windows_gcc`, `build_windows_clang` và `build_windows_msvc/Release`).

Cài PyQt6 nếu môi trường chưa có:

```bash
python3 -m pip install PyQt6
```

Khởi động GUI:

```bash
cd lab1
python3 gui/aestool_gui.py
```

GUI hỗ trợ:

- Encrypt/decrypt với ECB, CBC, CFB, OFB, CTR, XTS, CCM và GCM.
- Chọn key/IV từ file hoặc nhập key/IV dạng hex.
- AAD, authentication tag, metadata JSON, nonce database và các cờ bảo vệ
  như `--aead`, `--allow-ecb`, `--verbose`.
- Sinh key 256-bit, chạy KAT và benchmark nhanh.

GUI chỉ chạy lệnh sau khi nhấn nút tương ứng. Benchmark được ghi vào thư mục
do người dùng chọn; nên dùng `output/ubuntu/benchmarks/` hoặc
`output/windows/benchmarks/` khi chạy đúng nền tảng.

### Help & Usage
```bash
./aestool --help
```

### Key Generation
```bash
# Generate 256-bit raw binary key
./aestool keygen --bits 256 --out key.bin

# Generate 256-bit key in Hex format with AESKEY-HEX header
./aestool keygen --bits 256 --hex --out key.hex
```

### Encryption
```bash
# CBC encryption with key file and automatic IV generation (stored in document.enc.json)
./aestool encrypt --mode cbc --key key.bin --in document.pdf --out document.enc

# GCM authenticated encryption with Associated Data (AAD)
./aestool encrypt --mode gcm --aead --key key.bin --aad-text "Header-Metadata" \
                  --in document.pdf --out document.enc

# Text encryption outputting Base64 to terminal
./aestool encrypt --mode ctr --key-hex 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f \
                  --text "Secret message" --encode base64
```

### Decryption
```bash
# GCM decryption (automatically reads IV, Tag, and AAD from document.enc.json)
./aestool decrypt --mode gcm --aead --key key.bin --in document.enc --out document_dec.pdf

# CBC decryption specifying explicit IV
./aestool decrypt --mode cbc --key key.bin --iv-hex 000102030405060708090a0b0c0d0e0f \
                  --in document.enc --out document_dec.pdf
```

### Known Answer Tests Runner
```bash
./aestool --kat kat/vectors.json
```

### Performance Benchmark Runner
```bash
./aestool bench --runs 1000 --out bench_results.csv

# Protocol (default): warm-up 1.5 s per mode/direction, blocks of ~1000 operations
# (min(1000, budget/size), budget 256 MiB; --ops N forces N), N = 30 runs (30..100 recommended), mean/median/SD/95% CI.
# Extra options: [--runs 30..100] [--warmup SEC] [--ops N] [--budget-mb MiB] [--quick] (smoke test)

# Multi-threaded: N concurrent workers, MB/s is the aggregate (default 1, valid 1..256)
./aestool bench --runs 30 --threads 4 --out bench_mt.csv
```

Command-line arguments are read as UTF-8 on Windows as well (`--text "xin chào"` round-trips correctly).

Statistics table for the report (mean, median, SD and 95% CI of throughput and latency), generated straight from the CSV:

```bash
python ../scripts/csv_to_stats_table.py bench_mt.csv --size 1048576 > stats_1MiB.md
```

### Standard Exit Codes
- `0 (EXIT_OK)`: Operation succeeded.
- `1 (EXIT_USAGE)`: Command-line error, bad arguments, invalid format, or security constraint violation (ECB blocked, nonce reuse detected).
- `2 (EXIT_AUTH)`: Decryption or authentication verification failed (Integrity Tag mismatch, bad ciphertext).
- `3 (EXIT_KAT)`: One or more KAT vector checks failed.

---

## 9. Verified Build Matrix & Test Results

Clean builds with the standard commands of section 5 (latest run):

| Platform | Compiler | Crypto++ source | Build | CTest | KAT |
| :--- | :--- | :--- | :---: | :---: | :---: |
| Windows 11 | MSVC 19.51 (VS 2026), `/MT` | vcpkg `x64-windows-static` 8.9.0 | Pass | 3/3 | 29/29 |
| Windows 11 | MinGW-w64 GCC 16.2 (MSYS2) | `pacman` mingw-w64-crypto++ 8.9.0 | Pass | 3/3 | 29/29 |
| Ubuntu 26.04 (WSL2) | GCC 15.2 | `apt` libcrypto++-dev 8.9.0 | Pass | 3/3 | 29/29 |

Older logs from earlier runs (Ubuntu 22.04 GCC/Clang, Windows Clang) are kept under `output/`; they predate the extended KAT file.
The KAT file has **29 vectors**: SP 800-38A ECB/CBC/CFB128/OFB/CTR for AES-128/192/256, GCM test cases (AES-128/192/256, with AAD),
SP 800-38C CCM examples (tag 4, 8, 16), IEEE 1619 XTS vectors 1, 2, 3 and 15 (ciphertext stealing) plus an XTS-AES-256 case.
Entries labelled "cross-check" were recomputed with pyca/cryptography and are not literal NIST vectors.


---

## 10. Security & Implementation Notes

1. **Crypto++ Exclusivity:** All symmetric primitives, cipher modes, CSPRNGs (`AutoSeededRandomPool`), and hash functions (`SHA256`) strictly use Crypto++ 8.9+.
2. **Fail-Closed Design:** In AEAD decryption (GCM/CCM) or when authentication fails, no plaintext output file is written to disk, preventing plaintext leakage. Error messages remain neutral to thwart padding/error oracle attacks.
3. **Nonce-Reuse Prevention:** CTR, CCM, and GCM record hashes of used tuples $\text{SHA-256}(Key \parallel Mode \parallel IV)$ in `.aestool_nonces`. Attempting to encrypt with an existing pair immediately aborts.
4. **ECB Mitigation:** Input sizes $> 16\text{ KiB}$ are refused by default unless explicitly overridden with `--allow-ecb`.
5. **Constant-Time Verification:** AEAD tags are checked using constant-time comparison inside Crypto++'s `AuthenticatedDecryptionFilter` to mitigate timing side channels.
6. **Binary Stream Safety:** All file operations open streams with `std::ios::binary` to ensure multi-byte UTF-8 character strings and raw binary blocks remain untruncated across Windows and Linux.

### Secure defaults (summary)

| Default | Behaviour |
| :--- | :--- |
| Mode selection | No default mode: `--mode` is mandatory, so a weak mode is never chosen silently |
| AEAD | `ccm`/`gcm` refuse to run without `--aead` |
| IV / nonce | Generated with `AutoSeededRandomPool` when omitted, stored in the sidecar JSON next to the output (`<out>.json`) |
| ECB | Prints a `WARNING`, blocks inputs above 16 KiB unless `--allow-ecb` |
| Nonce reuse (CTR, CCM, GCM) | Rejected using `.aestool_nonces` (override the location with `--nonce-db`) |
| Output | A plaintext file is written only after successful decryption/authentication |
| Overwriting / key files | Keys are never printed; `keygen` warns to protect the file; `--key-hex` is accepted but discouraged (visible in shell history/process list) |
| Encoding | Screen output is hex; file output is raw binary; override with `--encode` |
| Exit codes | `0` ok, `1` input/usage error, `2` decryption/authentication failure, `3` KAT failure |

---

## 11. Known Limitations

- **Crypto++ lookup:** found automatically from system packages / vcpkg / `CMAKE_PREFIX_PATH`; for MSVC the directories derived from
  `PATH` are deliberately ignored (and for Linux too, so WSL's Windows `PATH` is not picked up). A Crypto++ in an unusual
  place needs `-DCRYPTOPP_ROOT_DIR=<prefix>`. MSVC needs the vcpkg toolchain (or `CRYPTOPP_ROOT_DIR`), it does not search `PATH`.
- **ECB:** inputs larger than 16 KiB are refused unless `--allow-ecb` is given. ECB is for demonstration only.
- **XTS:** confidentiality for storage only, no integrity; needs a 256- or 512-bit (double) key.
- **Nonce-reuse detection is local:** it relies on the `.aestool_nonces` file (or `--nonce-db`) in the
  working directory. Deleting it, using another directory, or another machine bypasses the check.
  Only hashes of (key, mode, IV) are stored, not the keys.
- **`--key-hex` exposes the key** in shell history and process listings; prefer `--key <file>`.
- **CCM:** nonce length must be 7-13 bytes and tag 4, 8 or 16 bytes.
- **No key management:** no secure key storage, and no side-channel hardening beyond what Crypto++ provides.
- **Benchmark numbers are machine-dependent** (results in `output/` come from the author's hardware).
- **Build matrix:** only the combinations in section 9 were run; Clang on Windows is untested with the latest sources.
- **Whole-file processing:** `--in` is read completely into memory (no streaming), so very large files (GiB) need that much RAM.
- **Nonce registry races:** `.aestool_nonces` has no inter-process locking; two simultaneous runs could both pass the check.
- **Library export:** only a static library (`aescore`) is built; no `.dll`/`.so` with a C API; the GUI calls the CLI executable.
- **Release binaries are x86-64 only** (Linux binary is fully static; MSVC build uses `/MT`).

---

## 12. References

- **NIST FIPS 197**: [Advanced Encryption Standard (AES)](https://csrc.nist.gov/publications/detail/fips/197/final)
- **NIST SP 800-38A**: [Recommendation for Block Cipher Modes of Operation](https://csrc.nist.gov/publications/detail/sp/800-38a/final)
- **NIST SP 800-38C**: [Recommendation for Block Cipher Modes of Operation: The CCM Mode](https://csrc.nist.gov/publications/detail/sp/800-38c/final)
- **NIST SP 800-38D**: [Recommendation for Block Cipher Modes of Operation: Galois/Counter Mode (GCM)](https://csrc.nist.gov/publications/detail/sp/800-38d/final)
- **IEEE Std 1619-2007**: [Cryptographic Protection of Data on Block-Oriented Storage Devices (XTS-AES)](https://standards.ieee.org/)
- **RFC 5116**: [An Interface and Algorithms for Authenticated Encryption](https://tools.ietf.org/html/rfc5116)
- **Crypto++ Library Documentation**: [Crypto++ AES & Wiki](https://www.cryptopp.com/wiki/AES)