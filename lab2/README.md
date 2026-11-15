# Lab 2 — Tự cài đặt AES-128 và CTR bằng C++17

`aestool` là chương trình dòng lệnh cài đặt **AES-128 từ đầu bằng C++** theo
FIPS-197 và chế độ **CTR** theo NIST SP 800-38A.

**Ràng buộc của đề: không dùng thư viện mật mã ngoài.** Vì vậy dự án có hai chương trình:

| Chương trình | Liên kết với | Dùng để |
|---|---|---|
| `aestool` (**bản nộp**) | Chỉ C++ STL và API hệ điều hành (`BCryptGenRandom` / `getrandom`) | Toàn bộ chức năng của Lab: mã hóa/giải mã, `keygen`, KAT, benchmark phần cài đặt tự viết |
| `aestool_ref` | `aestool` + OpenSSL + Crypto++ | **Chỉ** cross-validation (`xval`) và so sánh hiệu năng (`bench --ref`). Chỉ được build khi máy có OpenSSL hoặc Crypto++ |

Số ngẫu nhiên (key, IV, dữ liệu benchmark) lấy trực tiếp từ CSPRNG của hệ điều hành, không dùng `rand()`/`std::random`.

> **Cảnh báo bảo mật:** CTR chỉ cung cấp tính bí mật, không cung cấp tính toàn
> vẹn/xác thực. Không dùng Lab 2 như một giao thức bảo mật hoàn chỉnh trong
> production. Không bao giờ tái sử dụng cặp `(key, IV)`.

## Mục lục

1. [Mục tiêu và phạm vi](#1-mục-tiêu-và-phạm-vi)
2. [Tính năng](#2-tính-năng)
3. [Cấu trúc dự án](#3-cấu-trúc-dự-án)
4. [Yêu cầu môi trường](#4-yêu-cầu-môi-trường)
5. [Biên dịch](#5-biên-dịch)
6. [Sử dụng nhanh](#6-sử-dụng-nhanh)
7. [Tham chiếu CLI](#7-tham-chiếu-cli)
8. [Đặc tả AES-CTR](#8-đặc-tả-aes-ctr)
9. [Kiểm thử và kiểm chứng](#9-kiểm-thử-và-kiểm-chứng)
10. [Benchmark và báo cáo](#10-benchmark-và-báo-cáo)
11. [Phân tích bảo mật](#11-phân-tích-bảo-mật)
12. [Giới hạn và lỗi thường gặp](#12-giới-hạn-và-lỗi-thường-gặp)
13. [Checklist hoàn thành/nộp bài](#13-checklist-hoàn-thànhnộp-bài)

---

## 1. Mục tiêu và phạm vi

Lab này hiện thực các yêu cầu chính của Lab 2:

- AES-128, khóa 128 bit, block 128 bit, 10 vòng theo FIPS-197.
- Các phép biến đổi `AddRoundKey`, `SubBytes`, `ShiftRows`, `MixColumns`,
  `KeyExpansion` và inverse cipher.
- CTR cho dữ liệu có độ dài tùy ý, không padding, hỗ trợ streaming.
- Ba backend S-box để phân tích đánh đổi hiệu năng/bộ nhớ/cache:
  `table`, `ttable`, `computed`.
- KAT từ FIPS-197 và NIST SP 800-38A, kiểm chứng với OpenSSL, unit test và
  kiểm thử CLI end-to-end.
- Đo throughput, latency và thống kê mean/median/stddev/95% CI.
- Kiểm tra input lỗi theo nguyên tắc **fail closed**.

AES-192/256, XTS và AES-NI là các hạng mục bonus và **đã được triển khai** (xem mục 2.4).

## 2. Tính năng

### 2.1. AES và backend S-box

| Backend | Bảng tra | Đặc điểm |
|---|---:|---|
| `table` (mặc định) | S-box 256 B và inverse S-box 256 B | Byte-oriented, dễ đối chiếu FIPS-197; có truy cập bộ nhớ phụ thuộc dữ liệu |
| `ttable` | 4 bảng, mỗi bảng 1 KiB | Nhanh hơn trong một số máy; là mục tiêu kinh điển của cache-timing attack |
| `aesni` | Không có bảng | Bonus: tập lệnh AES-NI, phát hiện CPU lúc chạy, không rò cache-timing |
| `computed` | Không lưu S-box | Tính `S(x) = affine(x^254)` trong GF(2^8), giảm secret-indexed lookup nhưng chậm hơn đáng kể |

### 2.2. CTR

- IV là 16 byte, xem như `nonce || counter`.
- Counter là **toàn bộ block 128 bit theo big-endian**; byte thứ 15 là byte
  ít quan trọng nhất và được tăng trước.
- Không padding; block cuối được cắt theo đúng số byte plaintext.
- IV tự sinh dùng CSPRNG của hệ điều hành (`BCryptGenRandom` trên Windows, `getrandom(2)` trên Linux): 12 byte nonce ngẫu nhiên và 4 byte
  counter ban đầu bằng zero.
- Khi bỏ `--iv` lúc encrypt, IV được lưu ở `<output>.iv`; khi decrypt, IV
  được đọc từ `<input>.iv`.
- Counter không được wrap. Nếu cần tạo block sau `FF FF ... FF`, chương trình
  dừng với exit code `5` và không tạo output không hợp lệ.

### 2.3. I/O và an toàn lỗi

- Key AES-128 bắt buộc đúng 16 byte; IV bắt buộc đúng 16 byte.
- File mặc định là raw binary; output trên màn hình mặc định là hex khi
  encrypt.
- Có hỗ trợ hex/base64 cho input và output.
- Không ghi đè file nếu chưa có `--force`.
- Key/IV sai độ dài, file không tồn tại, mode không hỗ trợ, JSON KAT hỏng và
  counter overflow đều bị từ chối, không tiếp tục với giá trị mặc định.
- Windows nhận đối số UTF-16 rồi chuyển sang UTF-8; dữ liệu `--text` có thể
  chứa Unicode.

### 2.4. Bonus (đã triển khai)

| Bonus | Điểm | Triển khai | Kiểm chứng |
|---|---|---|---|
| **AES-192/256** | +10 | `Aes` trong `src/aes128.*`: key 16/24/32 byte chọn Nk = 4/6/8 và Nr = 10/12/14; lịch khóa FIPS-197 §5.2 đầy đủ, gồm `SubWord` phụ cho AES-256 khi `i mod 8 == 4`. Mọi backend (table, computed, ttable, aesni) dùng chung. | KAT FIPS-197 C.2/C.3, SP 800-38A F.1.3/F.1.5/F.5.3/F.5.5; round key A.2/A.3 trong unit test; cross-validation với OpenSSL |
| **AES-XTS** | +5 | `src/xts.*`: XTS-AES-128 (khóa 32 B) và XTS-AES-256 (khóa 64 B), hai khóa K1/K2, nhân tweak trong GF(2^128) bằng đa thức `x^128+x^7+x^2+x+1` (little-endian, không rẽ nhánh), ciphertext stealing cho độ dài không chia hết 16. | Vector IEEE 1619-2007 số 1, 2, 3 và 15 (ciphertext stealing); cross-validation với OpenSSL XTS cho mọi độ dài; unit test round-trip độ dài 16..130 |
| **AES-NI** | +5 | `src/aesni.*`: `AESENC/AESDEC` (+ `AESIMC` cho khóa giải mã), phát hiện CPU lúc chạy (CPUID.01H:ECX bit 25; `__cpuid` trên MSVC, `__builtin_cpu_supports` trên GCC/Clang). Lịch khóa vẫn là mã tự cài; AES-NI chỉ chạy các vòng. | So khớp với backend phần mềm trên khóa/khối ngẫu nhiên (128/192/256); KAT chạy trên cả backend `aesni` khi CPU hỗ trợ; CPU không hỗ trợ thì báo lỗi rõ (exit 3) |

Cách dùng:

```bash
aestool keygen --bits 256 --out k256.bin                 # AES-256 (cũng có 128, 192)
aestool encrypt --key k256.bin --in msg.bin --out ct.bin # key 16/24/32 byte -> AES-128/192/256-CTR
aestool keygen --bits xts512 --out kx.bin                # 64 byte = XTS-AES-256 (xts256 = XTS-AES-128)
aestool encrypt --mode xts --key kx.bin --in sector.bin --out sector.enc   # tweak ngẫu nhiên -> sector.enc.iv
aestool decrypt --mode xts --key kx.bin --in sector.enc --out sector.dec
aestool encrypt --key k256.bin --in msg.bin --out ct.bin --sbox aesni      # dùng AES-NI
aestool bench --backends table,ttable,aesni --sizes 1MiB
```

Quy tắc XTS của công cụ: `--iv`/`--iv-hex` là tweak 16 byte (số hiệu data unit); không có thì sinh ngẫu nhiên và lưu `<out>.iv`.
Input 16 byte đến 16 MiB (giới hạn 2^20 block của một data unit); K1 phải khác K2; không có `--nonce` và không có `--aead`.

**Mã hóa ổ đĩa và rủi ro của XTS:** tweak gắn mỗi block với vị trí (sector, chỉ số block), nên cùng nội dung ở hai vị trí cho hai ciphertext
khác nhau và ghi lại đúng chỗ cũ vẫn giải mã được. Nhưng XTS **không có tính toàn vẹn**: kẻ ghi được vào đĩa có thể làm hỏng một block
16 byte thành rác khó đoán, hoán đổi hoặc phát lại cả sector, và cùng plaintext tại cùng vị trí dưới cùng khóa luôn cho cùng ciphertext
(rò rỉ việc có thay đổi hay không). Chỉ dùng cho lưu trữ; không dùng cho thông điệp mạng, hãy dùng AEAD.

**AES-NI và constant-time:** `AESENC/AESDEC` thực hiện SubBytes trong phần cứng, không có truy cập bộ nhớ hay nhánh phụ thuộc dữ liệu bí mật,
nên không bị tấn công cache-timing như S-box bảng tra hoặc T-table. Backend `computed` cũng không có bảng tra nhưng chậm hơn nhiều.
Các phần còn lại của chương trình (I/O, XOR, parsing) chưa được chứng minh constant-time.

## 3. Cấu trúc dự án

```text
lab2/
├── CMakeLists.txt              # CMake: aes_pure + aestool (không thư viện ngoài); aestool_ref (tùy chọn)
├── README.md                   # Tài liệu này
├── src/
│   ├── aes128.hpp/.cpp         # AES-128 (lõi) + AES-192/256 (bonus), key schedule, các backend
│   ├── aesni.hpp/.cpp          # Bonus: backend AES-NI
│   ├── xts.hpp/.cpp            # Bonus: XTS-AES (IEEE 1619)
│   ├── aes_tables.hpp          # S-box và inverse S-box
│   ├── ctr.hpp/.cpp            # CTR memory API và streaming file API
│   ├── errors.hpp              # Error và exit code
│   ├── json.hpp/.cpp           # Parser JSON tối thiểu cho KAT
│   ├── kat.hpp/.cpp            # KAT runner
│   ├── xval.hpp/.cpp           # Cross-validation với OpenSSL / Crypto++ (chỉ có trong aestool_ref)
│   ├── bench.hpp/.cpp          # Benchmark encrypt/decrypt
│   ├── stats.hpp/.cpp          # Mean, median, stddev, confidence interval
│   ├── util.hpp/.cpp           # File, encoding, CSPRNG và tiện ích
│   └── main.cpp                # CLI aestool
├── tests/
│   ├── test_aes.cpp            # Catch2 unit tests
│   └── cli_e2e.cmake           # CLI integration test
├── kat/
│   └── vectors.json            # FIPS-197, AESAVS và SP 800-38A vectors
├── scripts/
│   ├── build_linux.sh          # Build/test/KAT trên Linux
│   ├── build_windows.bat       # Build/test/KAT trên Windows
│   ├── build.sh                # Wrapper build Linux
│   ├── build.bat               # Wrapper build Windows
│   └── plot_bench.py           # Vẽ đồ thị (có error bar) từ các CSV của `bench`
├── docs/
│   ├── REQUIREMENTS_CHECKLIST.md   # Đối chiếu yêu cầu đề / rules chung
│   ├── BENCHMARK_RUNBOOK.md        # Lệnh chạy benchmark Windows và Linux (cùng cấu hình)
│   ├── LINUX_RUNBOOK.md            # Bản rút gọn: chỉ các lệnh cần chạy trên Linux
│   └── TEST_EVIDENCE.md            # Lệnh sinh log KAT / ctest / negative / env
├── gui/
│   └── aestool_gui.py          # GUI tùy chọn
├── release/
│   ├── ubuntu/                 # aestool_gcc, aestool_ref_gcc (static)
│   └── windows/                # aestool_*.exe, aestool_ref_*.exe (static)
└── output/                     # Log, CSV và biểu đồ theo từng hệ điều hành (output/windows, output/ubuntu)
```

Tất cả lệnh bên dưới được chạy từ thư mục gốc `lab2/`, tức thư mục chứa
`CMakeLists.txt`.

## 4. Yêu cầu môi trường

### Bắt buộc (để build `aestool`)

- CMake **>= 3.20**.
- Compiler hỗ trợ C++17: GCC, Clang, MSVC hoặc MinGW-w64.
- Catch2 v3 để chạy unit test. CMake ưu tiên bản đã cài; nếu không có,
  `FetchContent` sẽ tải Catch2 v3.5.3 trong lúc configure (cần mạng một lần).
- Python 3 và `matplotlib` nếu cần vẽ biểu đồ.

### Tùy chọn (chỉ để build `aestool_ref`: cross-validation và so sánh hiệu năng)

- OpenSSL **>= 1.1** (`libssl-dev`) và/hoặc Crypto++ **>= 8.x** (`libcrypto++-dev`).
- CMake tự tìm, **không phải sửa đường dẫn nào**. Nếu thư viện nằm ở chỗ khác: `-DOPENSSL_ROOT_DIR=<prefix>`,
  `-DCRYPTOPP_ROOT_DIR=<prefix>`. Không tìm thấy thì CMake chỉ báo "aestool_ref is not built", `aestool` vẫn build bình thường.
- `-DLAB2_BUILD_REFERENCE=OFF` tắt hẳn phần này.

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install -y build-essential cmake git catch2 pkg-config
sudo apt install -y libssl-dev libcrypto++-dev      # tùy chọn: aestool_ref
# Chỉ cần nếu muốn vẽ biểu đồ:
sudo apt install -y python3 python3-pip
python3 -m pip install --user matplotlib
```

### Windows

Cài CMake, Git và một trong các toolchain sau:

- MSVC: Visual Studio Build Tools, workload **Desktop development with C++**,
  sau đó mở **x64 Native Tools Command Prompt for VS 2022**.
- MinGW-w64: dùng MSYS2 MinGW64/UCRT64, bảo đảm `g++.exe` và
  `mingw32-make.exe` có trong PATH.
- Clang: LLVM/Clang và generator phù hợp.

Cài thư viện tham chiếu trên Windows (cả hai cách dưới đây đã được build và chạy `ctest` thật: 4/4 pass):

```bash
# MinGW64: MSYS2 MINGW64 shell (không cần đường dẫn nào)
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
      mingw-w64-x86_64-catch mingw-w64-x86_64-openssl mingw-w64-x86_64-crypto++
mkdir build && cd build && cmake .. -G Ninja && cmake --build . && ctest --output-on-failure
```
```bat
:: MSVC: cài vcpkg một lần (bản static, khớp /MT), rồi build trong "x64 Native Tools Command Prompt for VS"
git clone https://github.com/microsoft/vcpkg.git D:\vcpkg
D:\vcpkg\bootstrap-vcpkg.bat -disableMetrics
D:\vcpkg\vcpkg.exe install openssl:x64-windows-static cryptopp:x64-windows-static
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=cl ^
      -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
      -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
cmake --build . && ctest --output-on-failure
```
Với MSVC + Ninja luôn truyền `-DCMAKE_BUILD_TYPE=Release` (mặc định của môi trường Developer Prompt có thể là Debug, lúc đó link nhầm thư viện vcpkg bản debug).
Đường dẫn `D:\vcpkg` chỉ là ví dụ nơi bạn đặt vcpkg, không nằm trong CMakeLists.

### Phiên bản đã kiểm chứng

| Thành phần | Yêu cầu / đã dùng |
|---|---|
| CMake | >= 3.20 (đã chạy: MinGW MSYS2, MSVC 19.51, Ubuntu 26.04 / CMake 4.2) |
| C++ | C++17 (lõi AES/CTR chỉ dùng STL; OpenSSL và Crypto++ chỉ nằm trong `aestool_ref`) |
| OpenSSL | >= 1.1 (đã chạy: 3.6.4 MinGW, vcpkg MSVC, 3.5.5 Ubuntu), tùy chọn |
| Crypto++ | >= 8.x (đã chạy: 8.9.0), tùy chọn |
| Catch2 | v3 (tự tải v3.5.3 nếu thiếu) |
| Python / matplotlib | 3.x / tùy chọn, chỉ để vẽ biểu đồ |

## 5. Biên dịch

### 5.1. Linux — script khuyến nghị

```bash
cd lab2
scripts/build_linux.sh gcc
scripts/build_linux.sh clang
```

Script thực hiện configure, build, `ctest` và KAT. Xóa build hiện tại trước
khi build lại:

```bash
scripts/build_linux.sh gcc --clean
```

### 5.2. Linux — CMake thủ công

```bash
cd lab2
cmake -S . -B build_ubuntu_gcc -DCMAKE_BUILD_TYPE=Release
cmake --build build_ubuntu_gcc -j"$(nproc)"
ctest --test-dir build_ubuntu_gcc --output-on-failure
```

Với Clang:

```bash
cmake -S . -B build_ubuntu_clang \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++
cmake --build build_ubuntu_clang -j"$(nproc)"
ctest --test-dir build_ubuntu_clang --output-on-failure
```

Tùy chọn hữu ích:

```bash
cmake -S . -B build_ubuntu_gcc \
  -DBUILD_TESTS=OFF \
  -DLAB2_WERROR=OFF
```

`LAB2_WERROR` mặc định **tắt** (để compiler mới hơn vẫn build được); bật `-DLAB2_WERROR=ON` khi phát triển.
`-DLAB2_STATIC_RUNTIME=ON` link tĩnh để binary phát hành không cần DLL/.so thêm (dùng cho `release/`).

### 5.3. Windows — script khuyến nghị

Chạy từ **Command Prompt**, không phải PowerShell:

```bat
cd lab2
set VCPKG_ROOT=D:\vcpkg
scripts\build_windows.bat msvc
scripts\build_windows.bat gcc
scripts\build_windows.bat clang
```

`VCPKG_ROOT` chỉ cần cho MSVC (script tự dùng toolchain vcpkg và `/MT`).

Script tự chọn thư mục `build_windows_msvc`, `build_windows_gcc` hoặc
`build_windows_clang`, chạy test và KAT. Xóa build:

```bat
scripts\build_windows.bat msvc clean
```

### 5.4. Windows — CMake thủ công

MSVC (chạy trong x64 Native Tools Command Prompt):

```bat
cd lab2
cmake -S . -B build_windows_msvc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=cl ^
  -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
cmake --build build_windows_msvc
ctest --test-dir build_windows_msvc --output-on-failure
```

MinGW-w64 (MSYS2, `C:\msys64\mingw64\bin` đứng đầu PATH, không cần cờ nào khác):

```bat
cmake -S . -B build_windows_gcc -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build_windows_gcc
ctest --test-dir build_windows_gcc --output-on-failure
```

Binary thường nằm ở:

```text
MSVC:  build_windows_msvc\aestool.exe        (và aestool_ref.exe)
MinGW: build_windows_gcc\aestool.exe         (và aestool_ref.exe)
Linux: build_ubuntu_gcc/aestool              (và aestool_ref)
```

### 5.5. Binary dựng sẵn

Nếu có binary trong `release/`, không cần build lại để thử CLI:

```bash
cd lab2
./release/ubuntu/aestool_gcc --help
./release/ubuntu/aestool_gcc --kat kat/vectors.json
```

```powershell
cd lab2
Get-ChildItem release\windows
& .\release\windows\aestool_msvc.exe --help
```

## 6. Sử dụng nhanh

### 6.1. Tạo key, encrypt và decrypt bằng IV sidecar

```bash
cd lab2
AESTOOL=./build_ubuntu_gcc/aestool
$AESTOOL keygen --out work-key.bin
$AESTOOL encrypt --mode ctr --key work-key.bin \
  --in message.bin --out ciphertext.bin
# Sinh ciphertext.bin.iv; decrypt tự đọc sidecar này
$AESTOOL decrypt --mode ctr --key work-key.bin \
  --in ciphertext.bin --out recovered.bin
cmp message.bin recovered.bin
```

PowerShell:

```powershell
$AESTOOL = ".\build_windows_msvc\Release\aestool.exe"
& $AESTOOL keygen --out work\key.bin
& $AESTOOL encrypt --key work\key.bin --in work\message.bin --out work\ct.bin
& $AESTOOL decrypt --key work\key.bin --in work\ct.bin --out work\recovered.bin
fc /b work\message.bin work\recovered.bin
```

Tạo IV riêng và truyền rõ ràng:

```bash
$AESTOOL ivgen --out iv.bin
$AESTOOL encrypt --key work-key.bin --iv iv.bin \
  --in message.bin --out ciphertext.bin
$AESTOOL decrypt --key work-key.bin --iv iv.bin \
  --in ciphertext.bin --out recovered.bin
```

Khi truyền IV thủ công, chương trình cảnh báo vì người dùng phải tự bảo đảm
không tái sử dụng IV với cùng key.

### 6.2. Dữ liệu text và encoding

```bash
# Key hex phải có đúng 32 ký tự hex = 16 byte
$AESTOOL encrypt \
  --key-hex 2b7e151628aed2a6abf7158809cf4f3c \
  --text "xin chào"

# Đọc plaintext hex và ghi ciphertext base64
$AESTOOL encrypt --key work-key.bin \
  --in message.hex --in-encode hex \
  --out ciphertext.b64 --encode base64
```

Không dùng `--key-hex` cho secret production vì key có thể xuất hiện trong
process listing hoặc shell history; ưu tiên `--key FILE`.

## 7. Tham chiếu CLI

Xem toàn bộ usage bằng:

```bash
aestool --help
```

### Lệnh

| Lệnh | Mục đích |
|---|---|
| `encrypt` / `decrypt` | CTR transform; encrypt và decrypt dùng cùng phép XOR keystream |
| `keygen` | Sinh key bằng CSPRNG của hệ điều hành: `--bits 128\|192\|256` (mặc định 128) hoặc `xts256\|xts512` |
| `ivgen` | Sinh IV 16 byte: 12 byte nonce + 4 byte counter zero |
| `--kat FILE` | Chạy Known Answer Tests, in PASS/FAIL từng case và tổng kết |
| `xval` | So sánh kết quả với OpenSSL / Crypto++ (**chỉ có ở `aestool_ref`**) |
| `bench` | Benchmark các backend tự cài; thêm `--ref openssl\|cryptopp\|all` (chỉ `aestool_ref`) để so sánh với thư viện |

### Tùy chọn chính

| Tùy chọn | Quy tắc |
|---|---|
| `--mode ctr\|xts` | `ctr` (mặc định) hoặc `xts` (bonus) |
| `--key FILE` / `--key-hex HEX` | Chọn đúng một. CTR: 16/24/32 byte (AES-128/192/256); XTS: 32/64 byte |
| `--iv FILE\|HEX` / `--iv-hex HEX` | IV đúng 16 byte: file raw hoặc 32 ký tự hex (`--iv` tự nhận file nếu tồn tại, ngược lại coi là hex) |
| `--nonce HEX` | Nonce 96-bit (24 hex); IV = nonce ‖ counter 32-bit bằng 0. Không dùng cùng `--iv`/`--iv-hex` (exit 2); sai độ dài: exit 3 |
| `--aead` | Bị từ chối (exit 2): CTR không có thẻ xác thực |
| `--threads N` | Chỉ cho `bench`: N worker chạy đồng thời (mỗi worker một buffer riêng), MB/s là tổng của mọi worker; mặc định 1, hợp lệ 1..256 |
| `--in FILE` / `--text STR` | Chọn đúng một input |
| `--in-encode raw\|hex\|base64` | Encoding của input |
| `--out FILE` | File output; mặc định raw binary |
| `--encode raw\|hex\|base64` | Encoding output |
| `--sbox table\|ttable\|computed\|aesni` | Chọn backend AES (`aesni` chỉ khi CPU hỗ trợ) |
| `--force` | Cho phép ghi đè output |
| `--quiet` / `--verbose` | Giảm/tăng thông tin chẩn đoán |

### Exit code

| Mã | Ý nghĩa |
|---:|---|
| `0` | Thành công |
| `1` | Lỗi nội bộ |
| `2` | Sai cú pháp/usage |
| `3` | Input không hợp lệ |
| `4` | Lỗi I/O hoặc output đã tồn tại |
| `5` | Counter overflow |
| `6` | KAT hoặc cross-validation thất bại |

### KAT và cross-validation

```bash
$AESTOOL --kat kat/vectors.json
$AESTOOL --kat kat/vectors.json --verbose
# cross-validation: dùng bản có thư viện tham chiếu
AESREF=./build_ubuntu_gcc/aestool_ref
$AESREF xval --ref openssl --cases 200
$AESREF xval --ref all --cases 200            # OpenSSL và Crypto++
```

`--kat` trả exit code `6` nếu có vector fail. `xval` dùng các input ngẫu
nhiên độc lập để so sánh AES/CTR thuần với reference.

## 8. Đặc tả AES-CTR

Với key `K`, IV `C_0` và plaintext block `P_i`:

```text
S_i = AES_K(C_i)
C_i = C_0 + i  (modulo 2^128, big-endian)
Ciphertext_i = P_i XOR S_i
```

Trong implementation này phép cộng modulo không được phép wrap khi yêu cầu
block mới vượt quá `FF...FF`; thay vào đó ném `CounterOverflow`. Đây là
chủ ý để tránh tái sử dụng keystream do wrap-around.

CTR không cần padding. Với plaintext 23 byte, chỉ 23 byte đầu của keystream
block cuối được XOR. Encrypt và decrypt là cùng một phép toán:

```text
P_i = Ciphertext_i XOR S_i
```

### IV và two-time pad

Nếu cùng `(key, IV)` được dùng cho hai plaintext:

```text
C1 XOR C2 = (P1 XOR S) XOR (P2 XOR S) = P1 XOR P2
```

Keystream bị lộ theo cách này có thể giúp khôi phục thông tin của cả hai
message. CTR cũng không phát hiện ciphertext bị sửa: sửa một bit ciphertext
sẽ sửa đúng bit tương ứng trong plaintext và chương trình vẫn có thể exit `0`.
Ứng dụng thực tế phải dùng AEAD (ví dụ AES-GCM) hoặc CTR kết hợp MAC với
quy trình xác thực trước khi sử dụng plaintext.

## 9. Kiểm thử và kiểm chứng

### 9.1. `ctest`

```bash
ctest --test-dir build_ubuntu_gcc --output-on-failure
```

Các nhóm test gồm:

- AES-128 key schedule và vector FIPS-197.
- S-box/inverse S-box và cả ba backend.
- CTR vector SP 800-38A, block cuối không đủ, chunking và carry.
- Counter overflow, key/IV sai, file lỗi và overwrite protection.
- Wrong key/IV, ciphertext tamper và hiện tượng CTR malleability.
- Hex/base64, JSON KAT, streaming file và CLI end-to-end.

### 9.2. Negative tests tối thiểu

```bash
# Sai key và sai IV phải cho plaintext sai
$AESTOOL keygen --out work/key-wrong.bin
$AESTOOL decrypt --key work/key-wrong.bin \
  --in ciphertext.bin --out wrong-key.bin --force

$AESTOOL decrypt --key work-key.bin \
  --iv-hex 00000000000000000000000000000001 \
  --in ciphertext.bin --out wrong-iv.bin --force

# Key/IV sai độ dài: kỳ vọng exit 3
$AESTOOL encrypt --key-hex 0011 --text hi
$AESTOOL decrypt --key work-key.bin --iv-hex 00 \
  --in ciphertext.bin --out invalid.bin

# IV toàn FF và input cần block kế tiếp: kỳ vọng exit 5
$AESTOOL encrypt --key work-key.bin \
  --iv-hex ffffffffffffffffffffffffffffffff \
  --text "counter overflow"
```

Không xem “decrypt sai key không báo lỗi” là bug của CTR: vì CTR không có
authentication, mọi 16-byte key đều là một keystream hợp lệ.

### 9.3. Log kết quả

Log bằng chứng được lưu ở `output/<os>/` (`kat_*`, `xval_*`, `ctest_*`, `negative_*`, `env_*`), lệnh sinh chúng nằm ở
[docs/TEST_EVIDENCE.md](docs/TEST_EVIDENCE.md).

## 10. Benchmark và báo cáo

### 10.1. Chạy benchmark

Benchmark dùng `aestool_ref` để có cột so sánh với OpenSSL và Crypto++ (`aestool` cho kết quả của riêng phần tự cài).
Chạy cùng các lệnh này, cùng tham số, trên Windows và Linux; mỗi lệnh ghi một CSV. Chạy **tuần tự**, không chạy song song hai benchmark.

```bash
cd lab2
AESREF=./build_ubuntu_gcc/aestool_ref        # Windows MinGW: build_windows_gcc\aestool_ref.exe ; MSVC: build_windows_msvc\aestool_ref.exe
OUT=output/ubuntu/bench; mkdir -p $OUT

# (1) Yêu cầu của đề: AES-128, 1 MiB / 100 MiB / 1 GiB, encrypt + decrypt, so với OpenSSL và Crypto++
$AESREF bench --bits 128 --sizes 1MiB --backends table --ref all --runs 30 --csv $OUT/bench_aes128_1MiB.csv
$AESREF bench --bits 128 --sizes 100MiB --backends table --ref all --runs 30 --csv $OUT/bench_aes128_100MiB.csv
$AESREF bench --bits 128 --sizes 1GiB --backends table --ref all --runs 30 --csv $OUT/bench_aes128_1GiB.csv

# (2) Table S-box so với computed S-box, cùng cấu hình: 4 KiB x 1000 thao tác (đúng protocol)
$AESREF bench --bits 128 --sizes 4KiB --backends table,computed --ref none --runs 30 --csv $OUT/bench_sbox_4KiB.csv

# (3) computed ở 1 MiB: chậm (~1 MB/s) nên chỉ 2 thao tác mỗi khối (sai lệch so với protocol, ghi trong báo cáo)
$AESREF bench --bits 128 --sizes 1MiB --backends computed --ops 2 --ref none --runs 30 --csv $OUT/bench_computed_1MiB.csv
$AESREF bench --bits 128 --sizes 100MiB --backends computed --ops 1 --ref none --runs 30 --csv $OUT/bench_computed_100MiB.csv   # ~1,7 giờ

# (4) Bonus AES-192 / AES-256
$AESREF bench --bits 192 --sizes 1MiB --backends table,ttable,aesni --ref all --runs 30 --csv $OUT/bench_aes192.csv
$AESREF bench --bits 256 --sizes 1MiB --backends table,ttable,aesni --ref all --runs 30 --csv $OUT/bench_aes256.csv
```

Protocol chuẩn (áp dụng cho mọi cấu hình): warm-up 1,5 s, rồi lặp N = 30-100 lần một khối ~1000 thao tác,
sau đó tính mean/median/SD/CI 95%. Với payload lớn, số thao tác mỗi khối giảm theo ngân sách dữ liệu
(`min(1000, 256 MiB / kích thước)`, đổi bằng `--budget-mb`; `--ops 1000` ép đúng 1000). Số thao tác thực tế ghi ở cột `ops_per_run`.
Tool in cảnh báo nếu `--runs` nằm ngoài 30-100. Backend `computed` chạy khoảng 1 MB/s (chậm hơn `table` ~70 lần),
nên 1000 thao tác chỉ khả thi ở payload nhỏ (lệnh 2); ở 1 MiB dùng `--ops 2` (lệnh 3).

`--bits 192|256` chỉ đo AES-CTR của bonus; OpenSSL/Crypto++ cũng được so sánh cho các độ dài khóa này.
1 GiB cần ~1 GiB RAM trống (một buffer, mã hóa tại chỗ); bỏ `1GiB` khỏi `--sizes` nếu không đủ (đề cho phép "nếu khả thi").

### 10.2. Thống kê và đồ thị

Tool ghi mean, median, standard deviation và 95% confidence interval. Tùy
chọn quan trọng:

| Tùy chọn | Ý nghĩa |
|---|---|
| `--sizes` | Ví dụ `1MiB,100MiB,1GiB` |
| `--runs` | Số lần chạy độc lập, chuẩn 30-100 (cảnh báo nếu ngoài khoảng) |
| `--backends` | `table,ttable,computed,aesni` |
| `--ref` | `all`, `openssl`, `cryptopp` hoặc `none` (chỉ `aestool_ref` có thư viện tham chiếu) |
| `--dir` | `enc`, `dec` hoặc `both` |
| `--ops` | Số operation mỗi run; mặc định tự chọn `min(1000, budget/size)`; `1000` để bám literal đề |
| `--budget-mb` | Ngân sách dữ liệu mỗi khối (mặc định 256) |
| `--bits` | `128`, `192` hoặc `256`: độ dài khóa AES-CTR được đo |
| `--threads` | Số worker song song, MB/s là tổng |
| `--warmup` | Thời gian warm-up |
| `--target-sec` | Tùy chọn: chọn số thao tác để mỗi run dài xấp xỉ giá trị này (mặc định tắt) |
| `--csv` | Đường dẫn CSV |

Vẽ đồ thị:

```bash
python3 scripts/plot_bench.py output/ubuntu/bench --out output/ubuntu/plots
```

Bảng thống kê đầy đủ (Mean, Median, SD, ±CI95 cho thông lượng và độ trễ) để dán vào báo cáo, sinh thẳng từ CSV:

```bash
python3 ../scripts/csv_to_stats_table.py output/ubuntu/bench/bench_aes128.csv --dir enc > output/ubuntu/stats_aes128.md
```

Trong báo cáo cần ghi:

- CPU, số core/thread, RAM, OS, compiler, CMake, OpenSSL và compile flags.
- Governor Linux ở `performance`; Windows power plan ở `High performance`.
- Số run, warm-up, payload size, backend, hướng encrypt/decrypt và reference.
- Bảng throughput MB/s, latency và error bar/95% CI.
- Phân tích `table`/`ttable`/`computed`: bộ nhớ, cache, branch prediction,
  secret-dependent index và đánh đổi hiệu năng–rò rỉ thông tin.
- So sánh implementation thuần với OpenSSL; key, IV và payload benchmark đều lấy từ CSPRNG của hệ điều hành, không có PRNG seed cố định.

## 11. Phân tích bảo mật

README/report phải nêu rõ các điểm sau:

1. **Table S-box và T-table:** lookup theo byte bí mật có thể làm lộ pattern
   cache; đối thủ đo timing/cache có thể suy luận key.
2. **Computed S-box:** loại bỏ bảng lookup bí mật trong lõi S-box nhưng không
   tự động chứng minh toàn bộ chương trình constant-time; I/O và XOR vẫn cần
   được đánh giá.
3. **CTR không xác thực:** ciphertext tamper không bị phát hiện; cần MAC hoặc
   AEAD, ưu tiên dùng AES-GCM trong ứng dụng mới.
4. **Nonce/IV uniqueness:** random IV không thay thế cho nonce registry; các
   process độc lập vẫn có thể tái sử dụng IV thủ công.
5. **AES-NI:** implementation phần cứng thường nhanh hơn và giảm rủi ro
   lookup-table timing, nhưng phải kiểm tra CPU/runtime và đo riêng.
6. **Fail closed:** input sai phải dừng trước khi tạo output; counter overflow
   không được wrap; output tồn tại không bị ghi đè nếu thiếu `--force`.
7. **Threat model:** phân biệt đối thủ đọc file, sửa ciphertext, đo cache/timing
   và người dùng vô tình reuse IV; nêu rõ giới hạn của demo offline này.

## 12. Giới hạn và lỗi thường gặp

- Hỗ trợ CTR (AES-128/192/256) và XTS; không có AEAD. XTS không có tính toàn vẹn, một data unit tối đa 16 MiB và được xử lý trong bộ nhớ.
- Lõi `Aes128` chỉ nhận khóa 16 byte; AES-192/256 đi qua lớp `Aes` (bonus). Khóa 128/192/256 được chọn theo độ dài khóa.
- Backend `aesni` chỉ có trên CPU x86/x86-64 có AES-NI; nơi khác báo lỗi rõ và dùng backend phần mềm.
- `table` và `ttable` có secret-dependent memory access theo chủ ý để phục vụ
  phân tích lab; không coi đây là implementation chống side-channel.
- Không có nonce database giữa các lần chạy; chương trình chỉ cảnh báo khi
  IV được truyền thủ công.
- `--key-hex` có thể lộ secret trong process listing/shell history.
- Nếu decrypt không truyền `--iv`, file input phải có sidecar `<input>.iv`.
- `--out` đã tồn tại cần `--force`; với encrypt tự sinh IV, sidecar cũng được
  kiểm tra để tránh trạng thái ciphertext/IV không nhất quán.
- Windows Clang chưa được kiểm chứng; ma trận đã chạy là MSVC, MinGW-w64 GCC và Ubuntu GCC.
- `xval` và `bench --ref` chỉ hoạt động trong `aestool_ref`; `aestool` (bản nộp) cố ý không chứa mã OpenSSL/Crypto++.
- `Catch2` không tìm thấy có thể khiến configure tải dependency qua mạng; dùng
  bản Catch2 v3 cài sẵn nếu môi trường offline.

## 13. Checklist hoàn thành/nộp bài

### Code và build

- [ ] Không dùng thư viện crypto ngoài trong AES/CTR core.
- [ ] Build out-of-source bằng GCC/Clang trên Ubuntu.
- [ ] Build bằng MSVC và/hoặc MinGW-w64 trên Windows.
- [ ] `ctest --output-on-failure` pass; log được lưu.
- [ ] Có binary Linux và Windows theo yêu cầu nộp bài.

### Đúng đắn và an toàn

- [ ] FIPS-197 AES KAT pass trên backend cần kiểm tra.
- [ ] SP 800-38A CTR KAT pass.
- [ ] `xval` khớp OpenSSL.
- [ ] Test sai key, sai IV, tamper, input sai độ dài và counter overflow.
- [ ] README/report ghi rõ big-endian counter, overflow và sidecar IV.

### Hiệu năng

- [ ] Có CSV cho encrypt/decrypt ở 1 MiB, 100 MiB và 1 GiB nếu khả thi.
- [ ] Có ít nhất 30 run độc lập, warm-up và 95% CI.
- [ ] Có bảng hardware/OS/compiler/flags và biểu đồ error bar.
- [ ] So sánh `table`, `ttable`, `computed` và OpenSSL.

### Báo cáo và hồ sơ nộp

- [ ] Báo cáo theo mạch: Objective → Design & Security → Test Methodology →
  Performance → Discussion → Conclusion → Appendix.
- [ ] Có build log, test log, KAT summary, benchmark CSV/plot và negative-test
  evidence.
- [ ] Có bảng tự chấm điểm theo rubric.
- [ ] Có cam kết liêm chính học thuật và khai báo việc dùng AI/công cụ nếu có.
- [ ] Đóng gói source, CMake, README, test, binary và report theo yêu cầu môn
  học; không đưa secret thật vào repository.

