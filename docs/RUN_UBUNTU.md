# Hướng Dẫn Biên Dịch, Kiểm Thử Và Vận Hành Crypto-Labs Trên Ubuntu Linux

> **Dự án:** `Feph50/crypto-labs`  
> **Tác giả:** Trần Lâm Nguyên (Feph50)  
> **Môi trường mục tiêu:** Ubuntu Linux 22.04 / 24.04 LTS (x86_64)  
> **Tham chiếu kiến trúc:** Thiết kế Runner thống nhất từ `thanhtamMalus/MMH` và cấu trúc dự án chuẩn `UITxWoodyNguyen/crypto-labs`.

---

## 1. Tổng Quan Kiến Trúc Dự Án

Kho lưu trữ gồm 2 phòng thí nghiệm mật mã trọng tâm đã được chuẩn hóa để hoạt động hoàn hảo trên Ubuntu Linux:

| Phòng thí nghiệm | Mục tiêu & Công nghệ | Điểm nổi bật & Ràng buộc bảo mật |
|---|---|---|
| **Lab 1: Symmetric Encryption** | C++17 sử dụng thư viện **Crypto++ 8.9.0** | Hỗ trợ 8 chế độ hoạt động: ECB, CBC, OFB, CFB, CTR, XTS, CCM, GCM.<br>Cơ chế phòng ngừa lạm dụng (*fail-closed*): cảnh báo & giới hạn 16 KiB với ECB, kiểm tra tính toàn vẹn tag AEAD, ngăn chặn tái sử dụng nonce. |
| **Lab 2: Pure AES-128 CTR** | **C++17 thuần (Pure STL)**, hoàn toàn tự code thuật toán AES (FIPS-197) và CTR mode (SP 800-38A), **không** dùng thư viện mật mã ngoài | 3 biến thể sinh/tra S-box: Bảng tra tĩnh (`sbox-table`), Tính toán trường hữu hạn Galois (`gfsbox`), Bảng tra tăng tốc 32-bit T-Table (`ttable`).<br>**OpenSSL** chỉ được dùng ở chế độ so sánh đối chiếu chéo (`xval`), không can thiệp vào lõi pure C++. |

Toàn bộ hệ thống được quản trị qua hệ thống build **CMake** out-of-source và bộ điều phối thống nhất **`run.sh` / `run.py`**.

---

## 2. Thiết Lập Môi Trường (Environment Setup)

### Cách 1: Sử dụng Môi trường Tự vận hành Không cần Sudo (Khuyến nghị cho máy hiện tại)

Dự án đã tích hợp môi trường phát triển đầy đủ tại `/home/lngu/crypto-env` bao gồm GCC 16, CMake, Ninja, OpenSSL 3.6, Crypto++ 8.9, Catch2, Python và PyQt6.

Để nạp biến môi trường cho phiên terminal hiện tại:
```bash
source ~/.bashrc
# Hoặc gõ lệnh trực tiếp:
export PATH="/home/lngu/crypto-env/bin:$PATH"
```

Đồng thời, cấu trúc thư viện tương thích đã được liên kết sẵn tại:
```bash
~/Desktop/nt219lib/
├── cryptopp/gcc/
│   ├── include/
│   └── lib/libcryptopp.a
└── openssl/gcc/
    ├── include/
    └── lib/libcrypto.so
```

### Cách 2: Cài đặt qua Package Manager (Nếu có quyền `sudo`)

Nếu triển khai trên máy Ubuntu mới có quyền root:
```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build git pkg-config \
                    libcrypto++-dev libssl-dev python3-pip python3-pyqt6
pip install matplotlib pandas
```

---

## 3. Các Phím Tắt & Tiện Ích Dòng Lệnh (Aliases)

Dự án đã cấu hình sẵn trong `~/.bash_aliases` các lệnh tiện lợi:

```bash
# Nạp lại alias
source ~/.bashrc

# Gọi aestool Lab 1 (Crypto++ đa mode)
aestool1 --help

# Gọi aestool Lab 2 (Pure C++ AES-128 CTR)
aestool2 --help

# Bộ điều phối dự án
crypto-run --help
```

---

## 4. Vận Hành Tự Động Bằng Unified Runner (`./run.sh`)

Tại thư mục gốc của repository (`/home/lngu/crypto-labs`), bạn có thể thực hiện mọi thao tác chỉ với một lệnh:

### 4.1. Biên dịch toàn bộ dự án (`build`)
Biên dịch cả Lab 1 và Lab 2 sang chế độ **Release** tối ưu hóa cao (`-O3 -DNDEBUG`):
```bash
./run.sh build
```
*Kết quả:* Các file thực thi được tạo tại:
- `build/lab1/aestool` (đồng thời cập nhật bản phân phối sẵn `lab1/release/ubuntu/aestool_gcc`)
- `build/lab2/aestool` (đồng thời cập nhật bản phân phối sẵn `lab2/release/ubuntu/aestool_gcc`)

### 4.2. Chạy toàn bộ Unit Tests (`test`)
Thực thi kiểm thử tích hợp tự động qua `ctest`:
```bash
./run.sh test
```
*Kết quả xác thực:* **6/6 bài test vượt qua (100% Passed)**:
1. `lab1_unit_tests`: Kiểm thử lõi mã hóa Crypto++, các chế độ và định dạng hex/base64.
2. `lab1_nist_kat`: Xác minh vector chuẩn NIST SP 800-38A/38D.
3. `lab1_cli_negative`: Kiểm tra phản xạ fail-closed khi dữ liệu bị sửa đổi hoặc sai khóa.
4. `lab2_unit_tests`: Kiểm thử Catch2 cho từng phép biến đổi SubBytes, ShiftRows, MixColumns, AddRoundKey và sinh khóa KeyExpansion.
5. `lab2_kat_runner`: Xác minh 21 vector kiểm chuẩn NIST cho AES-128 CTR.
6. `lab2_cli_end_to_end`: Kiểm thử kịch bản dòng lệnh toàn diện.

### 4.3. Kiểm thử Vector Chuẩn NIST KAT (`kat`)
Chạy bộ kiểm thử Known Answer Tests chính thống cho cả hai phòng thí nghiệm:
```bash
# Chạy cho cả 2 lab
./run.sh kat

# Hoặc chạy riêng từng lab
./run.sh kat --lab lab1
./run.sh kat --lab lab2
```
*Kết quả:*
- **Lab 1:** 9/9 vectors NIST SP 800-38A/C/D `[PASS]`.
- **Lab 2:** 21/21 vectors NIST FIPS-197 & SP 800-38A `[PASS]`.

### 4.4. Kiểm thử Bảo mật & Chống Giả mạo (`demo`)
Kiểm tra tính an toàn của hệ thống khi gặp dữ liệu bị can thiệp trái phép:
```bash
./run.sh demo
```
*Nội dung kiểm tra:*
- Xác thực lỗi xác thực (`AUTHENTICATION FAILED`) khi tag AEAD bị sửa 1 bit.
- Từ chối tái sử dụng nonce (`NONCE REUSE REFUSED`).
- Giới hạn độ dài dữ liệu và cảnh báo rò rỉ mẫu với ECB Mode (giới hạn 16 KiB).
- **Chứng minh tấn công Bit-Flipping Malleability trên Stream Cipher (CTR mode):** chứng minh kẻ tấn công có thể lật bit trên bản rõ mà không cần biết khóa AES nếu không có MAC/HMAC/AEAD.

### 4.5. Đo Hiệu Năng & Xuất Biểu Đồ So Sánh (`bench`)
Đo đạc thông lượng xử lý (Throughput MB/s) và độ trễ (Latency) trên các kích thước dữ liệu (1 MiB, 100 MiB, 1 GiB), so sánh tốc độ giữa các bản cài đặt:
```bash
# Chạy benchmark cả hai lab và tự động sinh đồ thị matplotlib
./run.sh bench

# Hoặc chỉ chạy riêng cho Lab 2
./run.sh bench --lab lab2
```
*Các đồ thị phân tích tự động xuất vào `lab2/Output/ubuntu/plots/`:*
- `performance_hierarchy_log.png`: Phân cấp hiệu năng dạng thang log (S-box table vs T-Table vs OpenSSL AES-NI).
- `block_latency_comparison.png`: So sánh độ trễ trung bình trên từng khối 16-byte.
- `malleability_attack_proof.png`: Sơ đồ trực quan minh họa cơ chế tấn công hoán đổi bit trên dòng CTR.
- `cli_kat_terminal.png` & `cli_negative_terminal.png`: Báo cáo trực quan kết quả test.

### 4.6. Khởi chạy Giao diện Đồ họa PyQt6 (`gui`)
```bash
# Mở GUI Lab 1 (Đa chế độ AES)
./run.sh gui 1

# Mở GUI Lab 2 (Pure C++ AES-128 CTR)
./run.sh gui 2
```
*Lưu ý:* Khi chạy trên máy chủ không có màn hình hoặc terminal SSH, chạy thử nghiệm kiểm tra nạp module không giao diện bằng:
```bash
QT_QPA_PLATFORM=offscreen ./run.sh gui 1
```

### 4.7. Dọn dẹp bản build (`clean`)
Xóa bỏ toàn bộ cache CMake và file thực thi tạm thời:
```bash
./run.sh clean
```

---

## 5. Thao Tác Trực Tiếp Với CLI

### 5.1. Lab 1 - Crypto++ Multi-Mode (`aestool1`)

```bash
# 1. Sinh khóa ngẫu nhiên AES-256 (32 bytes)
aestool1 keygen --bits 256 --out /tmp/key256.bin

# 2. Mã hóa file văn bản bằng GCM Mode (Xác thực dữ liệu AAD)
echo "Dữ liệu tuyệt mật của dự án NT219" > /tmp/secret.txt
aestool1 encrypt --mode gcm --key /tmp/key256.bin \
         --in /tmp/secret.txt --out /tmp/secret.enc \
         --aad-text "metadata_v1"

# 3. Giải mã và xác thực toàn vẹn
aestool1 decrypt --mode gcm --key /tmp/key256.bin \
         --in /tmp/secret.enc --out /tmp/secret_dec.txt \
         --aad-text "metadata_v1"
cat /tmp/secret_dec.txt

# 4. Kiểm tra fail-closed khi bị can thiệp sai AAD
aestool1 decrypt --mode gcm --key /tmp/key256.bin \
         --in /tmp/secret.enc --out /tmp/fake.txt \
         --aad-text "wrong_metadata"
# Kết quả: Báo lỗi AUTHENTICATION FAILED, không ghi ra file fake.txt
```

### 5.2. Lab 2 - Pure C++ AES-128 CTR (`aestool2`)

```bash
# 1. Sinh khóa 128-bit và IV 128-bit
aestool2 keygen --out /tmp/key128.bin --force
aestool2 ivgen --out /tmp/iv128.bin --force

# 2. Mã hóa chuỗi văn bản với bảng tra T-Table
echo "Mat ma hoc hien dai UIT 2026" > /tmp/plain.txt
aestool2 enc --key /tmp/key128.bin --iv /tmp/iv128.bin \
         --in /tmp/plain.txt --out /tmp/cipher.bin --impl ttable

# 3. Giải mã và đối chiếu tính đúng đắn
aestool2 dec --key /tmp/key128.bin --iv /tmp/iv128.bin \
         --in /tmp/cipher.bin --out /tmp/restored.txt --impl ttable
cat /tmp/restored.txt

# 4. Kiểm thử đối chiếu chéo với OpenSSL chuẩn (xval)
aestool2 xval --key /tmp/key128.bin --iv /tmp/iv128.bin \
         --in /tmp/cipher.bin --rounds 100
# Kết quả: Khớp chính xác 100% với OpenSSL EVP AES-128 CTR
```

---

## 6. Xử Lý Sự Cố Thường Gặp (Troubleshooting)

| Vấn đề | Nguyên nhân | Cách khắc phục |
|---|---|---|
| `CMakeCache.txt directory is different` | Do copy thư mục `build/` từ Windows hoặc vị trí khác sang. | Chạy `./run.sh clean` hoặc xóa thủ công: `rm -rf build lab1/build lab2/build`. |
| `bash: git / cmake / g++: command not found` | Biến môi trường chưa nhận `crypto-env`. | Chạy: `export PATH="/home/lngu/crypto-env/bin:$PATH"`. |
| `Could not load the Qt platform plugin "xcb"` | Chạy GUI trên terminal không có máy chủ hiển thị (X11 / Wayland). | Cần chạy trong môi trường Desktop có GUI, hoặc test headless bằng: `QT_QPA_PLATFORM=offscreen python3 ...`. |
| `AUTHENTICATION FAILED` (Mã thoát 3) | Tag bảo mật AEAD không khớp, dữ liệu bị sửa đổi hoặc sai khóa. | **Đúng thiết kế bảo mật fail-closed**. Không xuất dữ liệu giả ra đĩa. |
| `ECB payload exceeds 16 KiB limit` | Chế độ ECB không an toàn cho dữ liệu lớn. | Thêm cờ `--allow-ecb` nếu cố tình chạy cho mục đích nghiên cứu học tập. |
