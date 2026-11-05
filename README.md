# Cryptography & Applications (NT219) — Laboratory Series (Labs 1–6)

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Ubuntu%20LTS-blue.svg)]()
[![Standards](https://img.shields.io/badge/standards-FIPS--197%20%7C%20NIST%20SP800--38A%2FC%2FD-orange.svg)]()
[![License](https://img.shields.io/badge/license-Academic%20Use-lightgrey.svg)]()

> **Tác giả:** Trần Lâm Nguyên (Feph50)  
> **Môn học:** Mật mã học và ứng dụng (NT219) — Trường Đại học Công nghệ Thông tin (UIT)  
> **Năm học:** 2026 – 2027  
> **Gói nộp bài:** `MSV.All6labs.rar` (theo đặc tả đề bài `Cryptography_Labs.pdf`)

---

## 1. Giới thiệu tổng quan dự án

Dự án này là chuỗi 6 bài thực hành chuyên sâu (Labs 1–6) về Mật mã học hiện đại và Ứng dụng an toàn thông tin, tuân thủ nghiêm ngặt các quy chuẩn kỹ thuật của **NIST**, **FIPS** và **RFC**:

| Bài Lab | Đề tài & Mục tiêu | Thư viện / Ràng buộc | Trạng thái hiện tại |
| :--- | :--- | :--- | :---: |
| **Lab 1** | **Mã hóa đối xứng với Crypto++**<br>Hỗ trợ 8 mode AES, Misuse prevention, NIST KATs, Benchmark $N=30$. | **Chỉ dùng Crypto++** | 🟢 **Hoàn thành 100%** (Full code + Test + Bench + GUI) |
| **Lab 2** | **AES-128 C++ thuần (CTR Mode)**<br>FIPS-197 AES thuần không thư viện ngoài, CTR mode, S-box bảng tra & tính toán, Bonus AES-192/256 & AES-NI. | **C++ STL thuần túy** | 🟢 **Đã tích hợp mã nguồn & Verified Binary** |
| **Lab 3** | **RSA-OAEP & Hybrid Encryption**<br>RSA-3072/4096 OAEP SHA-256 + Mã hóa phong bì lai AES-256-GCM (Envelope). | Crypto++ / OpenSSL | 🟡 **Khung cấu trúc & Module nền tảng sẵn sàng** |
| **Lab 4** | **Hashing Suite, PKI & Thực nghiệm tấn công**<br>SHA-2/3/SHAKE streaming, X.509, HTTPS, MD5 Collision (`hashclash`), Length-extension (`hashpump`). | Crypto++ / OpenSSL | ⚪ **Kế hoạch Tuần 3-4** |
| **Lab 5** | **Chữ ký số cổ điển (ECDSA, RSA-PSS)**<br>ECDSA P-256 (RFC 6979) + RSA-PSS-3072, detached signatures, batch verify. | Crypto++ / OpenSSL | ⚪ **Kế hoạch Tuần 4-5** |
| **Lab 6** | **Chữ ký & Đóng gói khóa Hậu lượng tử**<br>ML-DSA-44/65 (FIPS 204), ML-KEM-512 (FIPS 203), PQ Certificate mini-project. | OpenSSL 3.x+ / liboqs | ⚪ **Kế hoạch Tuần 5-6** |

---

## 2. Yêu cầu môi trường & Phụ thuộc (Dependencies)

### A. Danh mục thư viện & Công cụ

| Công cụ / Thư viện | Phiên bản khuyến nghị | Mục đích sử dụng |
| :--- | :--- | :--- |
| **CMake** | $\ge 3.20$ | Hệ thống xây dựng dự án đa nền tảng (Out-of-source build) |
| **Trình biên dịch C++** | C++17 trở lên | MSVC 2022 (Windows), GCC 13+ (MinGW64 / Ubuntu), Clang 14+ |
| **Crypto++** | $8.9.0+$ | Thư viện mã hóa đối xứng cho Lab 1, RSA cho Lab 3 |
| **OpenSSL** | $3.0.0+$ | Thư viện băm, PKI, kiểm tra chéo (Cross-validation) cho Lab 2, 4, 5 |
| **Python** | $3.10+$ | Giao diện GUI (`PyQt6`), Vẽ biểu đồ hiệu năng (`matplotlib`, `pandas`) |
| **Catch2** | $v3.5+$ | Khung kiểm thử đơn vị tự động (Unit Test qua `ctest`) |

### B. Hướng dẫn thiết lập môi trường

#### Trên Windows (MinGW64 hoặc Visual Studio MSVC)
1. Cài đặt Visual Studio 2022 (với workload *Desktop development with C++*) hoặc MSYS2 MinGW-w64.
2. Cài đặt Python và các thư viện hỗ trợ:
   ```powershell
   pip install PyQt6 matplotlib pandas
   ```

#### Trên Ubuntu LTS (hoặc WSL2)
```bash
sudo apt update
sudo apt install -y build-essential cmake git libcrypto++-dev libssl-dev catch2 python3-pip python3-pyqt6
pip3 install matplotlib pandas
```

---

## 3. Hướng dẫn Biên dịch (Build Instructions)

Dự án hỗ trợ chuẩn **CMake Out-of-Source Build** theo đúng đặc tả:

### A. Biên dịch trên Windows (MSVC)
```powershell
# Biên dịch toàn bộ
mkdir build_win && cd build_win
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Hoặc biên dịch độc lập Lab 1:
cd lab1
mkdir build_msvc && cd build_msvc
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### B. Biên dịch trên Windows (MinGW64)
```powershell
cd lab1
mkdir build_gcc && cd build_gcc
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### C. Biên dịch trên Linux (Ubuntu LTS)
```bash
cd lab1
mkdir build_linux && cd build_linux
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

---

## 4. Chuẩn Giao diện dòng lệnh (CLI Uniform Standard)

Mọi công cụ đều tuân theo chuẩn dòng lệnh thống nhất:

```bash
aestool <command> [--in FILE | --text "..."] [--out FILE] [--key FILE | --key-hex HEX]
                  [--iv HEX] [--nonce HEX] [--mode MODE] [--aead] [--aad FILE]
                  [--encode hex|base64|raw] [--kat vectors.json] [--verbose]
```

### Ví dụ thao tác thực tế với `aestool`:

```powershell
# 1. Sinh khóa 256-bit an toàn (CSPRNG)
./aestool keygen --bits 256 --out key.bin

# 2. Mã hóa AES-256-GCM kèm xác thực AEAD và tự sinh IV
./aestool encrypt --mode gcm --aead --key key.bin --text "Tin nhan bi mat" --out ct.bin --meta meta.json

# 3. Giải mã dữ liệu
./aestool decrypt --mode gcm --aead --key key.bin --in ct.bin --meta meta.json

# 4. Chạy kiểm chứng toàn bộ vector NIST Known Answer Tests
./aestool --kat kat/vectors.json

# 5. Đo hiệu năng chuẩn thống kê N=30 lượt
./aestool bench --runs 30 --out bench_results.csv
```

---

## 5. Báo cáo Thực thi Tuần 1 (Week 1 Milestones)

Trong Tuần 1, toàn bộ các mục tiêu cốt lõi của đề tài đã được triển khai và nghiệm thu thành công:

### ✅ Kết quả Kiểm chứng NIST KAT (Known Answer Tests)
Chạy bộ 9 test vector chính thức từ **NIST SP 800-38A, GCM McGrew, SP 800-38C (CCM) và IEEE 1619 (XTS)**:
```text
[PASS] SP800-38A F.1.1 ECB-AES128 (4 blocks)
[PASS] SP800-38A F.2.1 CBC-AES128
[PASS] SP800-38A F.3.13 CFB128-AES128
[PASS] SP800-38A F.4.1 OFB-AES128
[PASS] SP800-38A F.5.1 CTR-AES128
[PASS] GCM Test Case 2 (AES-128, 1 block zero)
[PASS] GCM Test Case 4 (AES-128, AAD)
[PASS] SP800-38C Appendix C Example 1 (CCM, Tlen=4)
[PASS] IEEE 1619 XTS-AES128 vector 1
---- KAT summary: 9 passed, 0 failed, 9 total (100% PASS)
```

### ✅ Nghiệm thu Vệ sinh bảo mật & Kiểm thử âm (Misuse Prevention / Fail-Closed)
- **Chặn và Cảnh báo ECB**: Chặn file $> 16\text{ KiB}$ trừ khi có cờ `--allow-ecb`.
- **Chống tái sử dụng Nonce (Catastrophic Nonce Reuse)**: Phát hiện và hủy thao tác ngay lập tức khi phát hiện cặp (Key, Nonce) đã từng xuất hiện.
- **Bắt buộc cờ `--aead`**: Ngăn ngừa giải mã thiếu xác thực trong mode GCM/CCM.
- **Fail-closed**: Mọi dữ liệu hỏng, sai IV length, sai Key length đều ngắt ngay lập tức với exit code $\ne 0$, không rò rỉ dữ liệu thô.

### ✅ Đo đạc Hiệu năng thực tế trên CPU Intel Core i7-11800H ($N=30$ lượt độc lập)
*Hệ điều hành: Windows 11 Pro 64-bit | RAM: 32 GB | Tập lệnh tăng tốc: Intel AES-NI*

- **Chế độ dòng (CTR Mode)**: Thông lượng đạt **$1.300\text{ MB/s} - 2.013\text{ MB/s}$**.
- **Chế độ xác thực (GCM Mode)**: Thông lượng đạt **$1.135\text{ MB/s} - 1.317\text{ MB/s}$**.
- **Chế độ khối song song (ECB/CBC giải mã)**: Đạt đỉnh thông lượng lên tới **$2.461\text{ MB/s}$**.
- Toàn bộ biểu đồ so sánh chi tiết đã được xuất tự động vào thư mục `lab1/output/windows/plots/`.

### ✅ Giao diện trực quan GUI (PyQt6) — Đạt chuẩn Bonus (+5 điểm)
Chạy ứng dụng GUI đầy đủ tính năng:
```powershell
python lab1/gui/aestool_gui.py
```

---

## 6. Kế hoạch Tuần tiếp theo (Week 2 Roadmap)
- [x] **Tuần 1**: Khởi tạo repo, triển khai và hoàn tất trọn vẹn Lab 1 (CLI, KAT, Benchmarks, GUI, Report).
- [ ] **Tuần 2**: Nghiệm thu và hoàn thiện báo cáo Lab 2 (AES-128 C++ thuần, đối chiếu OpenSSL, đo lường S-box bảng tra vs tính toán).
- [ ] **Tuần 3**: Hoàn thiện công cụ mã hóa lai Lab 3 (`rsatool`: RSA-OAEP + AES-GCM envelope).
- [ ] **Tuần 4**: Bộ công cụ băm Lab 4 (`hashtool`), demo va chạm MD5 (`hashclash`) và Length-extension attack.
- [ ] **Tuần 5**: Chữ ký số cổ điển Lab 5 (`sigtool`: ECDSA deterministic RFC 6979 + RSA-PSS).
- [ ] **Tuần 6**: Mật mã hậu lượng tử Lab 6 (`pqtool`: ML-DSA + ML-KEM), tổng hợp báo cáo 60 trang và đóng gói `MSV.All6labs.rar`.
