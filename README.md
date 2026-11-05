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
| **Lab 2** | **AES-128 C++ thuần (CTR Mode)**<br>FIPS-197 AES thuần không thư viện ngoài, CTR mode, S-box bảng tra & tính toán, Bonus AES-192/256 & AES-NI. | **C++ STL thuần túy** | 🟢 **Hoàn thành 100%** (18/18 KATs + Bench + Bonus + GUI) |
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

### A. Biên dịch trên Windows (MSVC / MinGW64)
```powershell
# Biên dịch toàn bộ các lab
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Hoặc chạy trực tiếp binary độc lập trong release/:
.\lab1\release\windows\aestool.exe --help
.\lab2\release\windows\aestool.exe --help
```

### B. Khởi chạy Giao diện Trực quan (PyQt6 GUI)
```powershell
# Giao diện Lab 1 (Crypto++ Multi-mode AES)
python lab1/gui/aestool_gui.py

# Giao diện Lab 2 (Pure C++ AES-128 CTR)
python lab2/gui/aestool_gui.py
```

---

## 4. Báo cáo Thực thi Tuần 1 (Week 1 Milestones)

- **9/9 NIST KATs Pass**: ECB, CBC, CFB, OFB, CTR, GCM (1 & 2), CCM, XTS.
- **Fail-closed & Misuse Prevention**: Giới hạn ECB $16\text{ KiB}$, phát hiện và triệt tiêu tái sử dụng Nonce, bắt buộc cờ `--aead`.
- **Benchmark $N=30$**: Đo đạc trên CPU Intel Core i7-11800H @ 2.30GHz, xuất 9 biểu đồ PNG tại `lab1/output/windows/plots/`.
- Báo cáo chi tiết: [`docs/WEEK1_SUMMARY_REPORT.md`](file:///D:/MMH_Final_Labs/Final_Lab%20Tasks/NT219-Crypto-Labs/docs/WEEK1_SUMMARY_REPORT.md).

---

## 5. Báo cáo Thực thi Tuần 2 (Week 2 Milestones — Lab 2)

### ✅ Lõi AES-128 C++ thuần & Chế độ Counter (FIPS-197 & NIST SP 800-38A)
- Tự cài đặt 100% bằng C++17 thuần: `KeyExpansion`, `SubBytes`, `ShiftRows`, `MixColumns`, `AddRoundKey`.
- Không sử dụng thư viện mật mã ngoại vi; đối chiếu chéo độc lập với OpenSSL cho kết quả trùng khớp hoàn hảo (0 bit sai khác).
- Chế độ CTR: Bộ đếm Big-Endian 128 bit tăng tuần tự với cơ chế chống tràn, không dùng padding, xử lý chính xác các khối dữ liệu lẻ.

### ✅ 18/18 NIST Known Answer Tests (KAT) Hoàn thành
- Toàn bộ vector kiểm thử FIPS-197 (Appendix B & C) và NIST SP 800-38A (CTR-AES128) đạt 100% PASS.
- Bằng chứng thực nghiệm: [`lab2/Output/windows/kat/kat_results.txt`](file:///D:/MMH_Final_Labs/Final_Lab%20Tasks/NT219-Crypto-Labs/lab2/Output/windows/kat/kat_results.txt).

### ✅ Kiểm thử Âm tính & Thực nghiệm Tấn công Malleability
- **Fail-closed:** Bắt lỗi độ dài khóa sai (< 16 byte), IV sai, file không tồn tại với exit code tương ứng (2, 3), không rò rỉ dữ liệu.
- **Tấn công tính uốn dẻo (Bit-flipping):** Lật bit byte đầu tiên trên bản mã làm thay đổi trực tiếp ký tự tương ứng trên bản rõ (`'A'` $\to$ `'a'`) mà không bị phát hiện, chứng minh tính cấp thiết của cơ chế MAC / AEAD.
- Bằng chứng thực nghiệm: [`lab2/Output/windows/negative_tests/negative_results.txt`](file:///D:/MMH_Final_Labs/Final_Lab%20Tasks/NT219-Crypto-Labs/lab2/Output/windows/negative_tests/negative_results.txt).

### ✅ Ma trận Hiệu năng Thực tế trên CPU Intel Core i7-11800H ($N=30$, CI 95%)
- Dữ liệu CSV: [`lab2/Output/windows/bench/bench_lab2_windows.csv`](file:///D:/MMH_Final_Labs/Final_Lab%20Tasks/NT219-Crypto-Labs/lab2/Output/windows/bench/bench_lab2_windows.csv).
- **Table S-Box vs Computed S-Box:** Table đạt **$27.36\text{ MB/s}$**, Computed đạt **$1.09\text{ MB/s}$** (chậm hơn ~25x do chi phí tính toán nghịch đảo trường Galois tại chỗ).
- **Ảnh hưởng độ dài khóa:** AES-128 ($27.36\text{ MB/s}$) $\to$ AES-192 ($21.66\text{ MB/s}$) $\to$ AES-256 ($18.68\text{ MB/s}$), phản ánh chính xác quy luật tỷ lệ nghịch với số vòng biến đổi (10 : 12 : 14).
- Biểu đồ phân tích lưu tại: `lab2/Output/windows/plots/`.

### ✅ Tính năng Thưởng (Bonus Features — +15 điểm)
- **Bonus A (+10 điểm):** Hỗ trợ đầy đủ AES-192 (12 vòng) và AES-256 (14 vòng) với lịch khóa mở rộng chuẩn FIPS-197.
- **Bonus C (+5 điểm):** Nhận diện thời gian thực tập lệnh tăng tốc phần cứng AES-NI qua CPUID Leaf 1 (ECX bit 25).
- Báo cáo học thuật 8 phần toàn diện: [`lab2/docs/lab2-main-report.md`](file:///D:/MMH_Final_Labs/Final_Lab%20Tasks/NT219-Crypto-Labs/lab2/docs/lab2-main-report.md).

---

## 6. Kế hoạch Tuần tiếp theo (Roadmap)
- [x] **Tuần 1**: Khởi tạo repo, triển khai và hoàn tất trọn vẹn Lab 1 (Crypto++ Multi-mode AES).
- [x] **Tuần 2**: Triển khai và nghiệm thu toàn diện Lab 2 (Pure C++ AES-128 CTR, 18 KATs, Benchmark $N=30$, Bonus A & C).
- [ ] **Tuần 3**: Hoàn thiện công cụ mã hóa lai Lab 3 (`rsatool`: RSA-3072/4096 OAEP SHA-256 + AES-256-GCM envelope).
- [ ] **Tuần 4**: Bộ công cụ băm Lab 4 (`hashtool`), demo va chạm MD5 (`hashclash`) và Length-extension attack.
- [ ] **Tuần 5**: Chữ ký số cổ điển Lab 5 (`sigtool`: ECDSA deterministic RFC 6979 + RSA-PSS).
- [ ] **Tuần 6**: Mật mã hậu lượng tử Lab 6 (`pqtool`: ML-DSA + ML-KEM), tổng hợp báo cáo 60 trang và đóng gói `MSV.All6labs.rar`.
