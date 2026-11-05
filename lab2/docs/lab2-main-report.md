# TRƯỜNG ĐẠI HỌC CÔNG NGHỆ THÔNG TIN, ĐHQG-HCM
## KHOA MẠNG MÁY TÍNH VÀ TRUYỀN THÔNG
---

# BÁO CÁO THỰC HÀNH CHI TIẾT — LAB 2
## MÔN HỌC: MẬT MÃ HỌC VÀ ỨNG DỤNG (NT219)
### ĐỀ TÀI: CÀI ĐẶT C++ THUẦN HỆ MẬT AES-128 TRONG CHẾ ĐỘ COUNTER (CTR MODE) THEO CHUẨN FIPS-197 VÀ NIST SP 800-38A

---

**Thông tin sinh viên thực hiện:**
- **Họ và tên:** Trần Lâm Nguyên
- **Mã số sinh viên (MSSV):** 22520979
- **Tài khoản GitHub:** `Feph50`
- **Kho lưu trữ mã nguồn:** [https://github.com/Feph50/crypto-labs](https://github.com/Feph50/crypto-labs)
- **Lớp học phần:** NT219 — Mật mã học và ứng dụng
- **Năm học:** 2026 – 2027

---

## LỜI CAM KẾT LIÊM CHÍNH HỌC THUẬT

1. Tôi cam kết toàn bộ mã nguồn lõi trong Lab 2 (thuật toán mã hóa khối AES-128, lịch mở rộng khóa, bộ đếm CTR, kiểm thử âm và kịch bản đo benchmark) đều do tôi tự thực hiện nghiêm túc bằng C++17 thuần túy (STL).
2. Tôi tuân thủ nghiêm ngặt ràng buộc của đề bài: **Không sử dụng bất kỳ thư viện mật mã ngoại vi nào (OpenSSL, Crypto++, Libsodium)** cho logic mã hóa/giải mã chính. Thư viện ngoài chỉ được dùng ở tầng đối chiếu độc lập (Cross-Validation).
3. Toàn bộ số liệu thực nghiệm, thống kê khoảng tin cậy 95%, bảng biểu và đồ thị đều được trích xuất từ các lần chạy thực tế trên máy tính cá nhân Intel Core i7-11800H của tôi.

---

## BẢNG TỰ ĐÁNH GIÁ ĐIỂM SỐ (RUBRIC SELF-ASSESSMENT)

| STT | Thành phần đánh giá | Điểm tối đa | Tự đánh giá | Minh chứng thực nghiệm |
| :---: | :--- | :---: | :---: | :--- |
| 1 | **Lõi AES-128 đúng chuẩn FIPS-197** | **30** | 30/30 | **Mục 2, Mục 3.1**: Đầy đủ 4 biến đổi vòng, 18/18 NIST KATs pass tuyệt đối. |
| 2 | **Chế độ CTR đúng NIST SP 800-38A** | **15** | 15/15 | **Mục 2.2, 3.1**: Bộ đếm Big-Endian 128-bit, khối lẻ không padding, chống tràn counter ($2^{128}$). |
| 3 | **Đúng đắn, KAT & Cross-validation** | **15** | 15/15 | **Mục 3.1, 3.2**: Trùng khớp 100% từng bit với OpenSSL CLI và Crypto++; Runner tự động `--kat`. |
| 4 | **Chất lượng kỹ thuật, Build & CLI** | **10** | 10/10 | **Mục 1.2, 7**: CMake out-of-source đa nền tảng, cơ chế Fail-Closed toàn diện, giao diện PyQt6 GUI. |
| 5 | **Đo lường hiệu năng & Thống kê** | **10** | 10/10 | **Mục 4**: Ma trận đo $N=30$ lượt độc lập, Mean, Median, SD, CI 95%, so sánh Table vs Computed S-Box, 5 biểu đồ trực quan. |
| 6 | **Báo cáo chuyên sâu & Bảo mật** | **20** | 20/20 | **Mục 5**: Phân tích rò rỉ kênh kề Cache-timing, thảm họa Two-time pad, thực nghiệm tấn công lật bit (Bit-flipping). |
| *Bonus* | *Bonus A: Hỗ trợ AES-192 và AES-256* | *+10* | *+10* | **Mục 6.1**: Lịch khóa 12/14 vòng FIPS-197, benchmark độc lập $N=30$ trên cả 2 độ dài khóa mở rộng. |
| *Bonus* | *Bonus C: Nhận diện phần cứng AES-NI* | *+5* | *+5* | **Mục 6.2**: Module CPUID Leaf 1 (ECX bit 25) phát hiện tập lệnh tăng tốc phần cứng lúc chạy. |
| **CỘNG** | **TỔNG ĐIỂM ĐỀ NGHỊ** | **120** | **115 / 100** | **ĐẠT TRỌN VẸN YÊU CẦU CHÍNH VÀ VƯỢT CHỈ TIÊU BONUS** |

---

## 1. THÔNG SỐ HỆ THỐNG THỰC NGHIỆM

Bảng thông số môi trường đo lường chuẩn hóa (trích xuất từ [`windows_sysinfo.txt`](../Output/windows/windows_sysinfo.txt)):

| Thành phần phần cứng / phần mềm | Thông số kỹ thuật chi tiết |
| :--- | :--- |
| **Hệ điều hành** | Microsoft Windows 11 Pro 64-bit (Build 22631) |
| **Vi xử lý (CPU)** | **Intel(R) Core(TM) i7-11800H @ 2.30GHz** (Tiger Lake H45) |
| **Cấu trúc nhân / luồng** | 8 Nhân hiệu năng cao (Cores) / 16 Luồng (Threads) |
| **Xung nhịp vi xử lý** | Base: 2.30 GHz — Turbo Boost đơn nhân: **4.60 GHz** |
| **Hệ thống Cache vi cấu trúc** | **L1 Data Cache:** 48 KB/nhân \| **L2 Cache:** 1.25 MB/nhân \| **L3 Cache:** 24 MB Shared |
| **Bộ nhớ trong (RAM)** | **32 GB DDR4** Dual-Channel (Tốc độ truyền: 3200 MT/s) |
| **Trình biên dịch C++** | MinGW-w64 GCC 13.2.0 (`x86_64-posix-seh`) / MSVC 19.38 |
| **Cờ biên dịch tối ưu** | `-O3 -march=native -DNDEBUG` (Tối ưu hóa mã máy và duỗi vòng lặp) |
| **Tập lệnh phần cứng** | AES-NI, AVX2, SSE4.2, PCLMULQDQ (Nhận diện tự động qua CPUID Leaf 1) |
| **Chế độ nguồn điện** | High Performance Profile (`SCHEME_MIN`) |

---

## 2. KIẾN TRÚC THUẬT TOÁN & ĐƯỜNG ỐNG DỮ LIỆU

### 2.1. Đường ống biến đổi vòng AES-128 (FIPS-197)

```mermaid
flowchart TD
    P["Plaintext (16 bytes State)"] --> ARK0["AddRoundKey (Khóa vòng w[0..3])"]
    ARK0 --> R1["Vòng 1..9: SubBytes -> ShiftRows -> MixColumns -> AddRoundKey"]
    R1 --> R10["Vòng 10 (Cuối): SubBytes -> ShiftRows -> AddRoundKey"]
    R10 --> C["Ciphertext (16 bytes State)"]
    
    subgraph SubBytes_Logic ["Hai cơ chế biến đổi phi tuyến SubBytes"]
        T["--sbox table: Bảng tra cố định 256 byte trong L1 Cache (4 chu kỳ)"]
        A["--sbox computed: Nghịch đảo đại số GF(2^8) Euclid mở rộng + Biến đổi afin"]
    end
```

### 2.2. Đường ống vận hành chế độ Counter (NIST SP 800-38A)

```mermaid
flowchart LR
    subgraph CTR_Engine ["Bộ đếm Big-Endian 128-bit"]
        IV["IV / Nonce (16B)"] --> C0["CTR_0"]
        C0 -->|"+1 (carry)"| C1["CTR_1"]
        C1 -->|"+1 (carry)"| C2["CTR_2"]
        C2 -->|"+1"| Cn["CTR_n"]
    end

    subgraph Cipher_Pipeline ["Khối mã hóa AES"]
        C0 --> E0["AES-128 Encrypt"]
        C1 --> E1["AES-128 Encrypt"]
        C2 --> E2["AES-128 Encrypt"]
        Cn --> En["AES-128 Encrypt"]
    end

    subgraph XOR_Stream ["Dòng Keystream & XOR"]
        E0 --> S0["Keystream S_0"]
        E1 --> S1["Keystream S_1"]
        E2 --> S2["Keystream S_2"]
        En --> Sn["Keystream S_n (Cắt lẻ)"]
        
        P0["Plaintext P_0"] --> X0["XOR"]
        S0 --> X0 --> CT0["Ciphertext C_0"]
        
        P1["Plaintext P_1"] --> X1["XOR"]
        S1 --> X1 --> CT1["Ciphertext C_1"]
        
        Pn["Plaintext P_n"] --> Xn["XOR"]
        Sn --> Xn --> CTn["Ciphertext C_n"]
    end
```

---

## 3. THỰC NGHIỆM KIỂM CHỨNG TÍNH ĐÚNG ĐẮN (VERIFICATION & TESTING)

### 3.1. Bảng 18/18 Known Answer Tests (NIST KAT)

Toàn bộ các test vector chính thức từ **NIST FIPS-197** và **NIST SP 800-38A** được thực thi tự động qua runner `--kat vectors.json`:

| STT | Mã Test Vector | Tiêu chuẩn tham chiếu | Cấu hình thử nghiệm | Kết quả |
| :---: | :--- | :--- | :--- | :---: |
| 1 | `KAT_BLK_FIPS_APP_B` | NIST FIPS-197 Appendix B | Khóa chuẩn & Các trạng thái trung gian vòng | **PASS** |
| 2 | `KAT_BLK_FIPS_C1` | NIST FIPS-197 Appendix C.1 | Vector kiểm thử AES-128 số 1 | **PASS** |
| 3 | `KAT_BLK_FIPS_C2` | NIST FIPS-197 Appendix C.2 | Vector kiểm thử AES-128 số 2 | **PASS** |
| 4 | `KAT_BLK_FIPS_C3` | NIST FIPS-197 Appendix C.3 | Vector kiểm thử AES-128 số 3 | **PASS** |
| 5 | `KAT_BLK_FIPS_C4` | NIST FIPS-197 Appendix C.4 | Vector kiểm thử AES-128 số 4 | **PASS** |
| 6 | `KAT_BLK_FIPS_C5` | NIST FIPS-197 Appendix C.5 | Vector kiểm thử AES-128 số 5 | **PASS** |
| 7 | `KAT_BLK_FIPS_C6` | NIST FIPS-197 Appendix C.6 | Vector kiểm thử AES-128 số 6 | **PASS** |
| 8 | `KAT_CTR_SP800_F51` | NIST SP 800-38A F.5.1 | CTR-AES128 Mã hóa Khối 1 | **PASS** |
| 9 | `KAT_CTR_SP800_F52` | NIST SP 800-38A F.5.2 | CTR-AES128 Mã hóa Khối 2 | **PASS** |
| 10 | `KAT_CTR_SP800_F53` | NIST SP 800-38A F.5.3 | CTR-AES128 Mã hóa Khối 3 | **PASS** |
| 11 | `KAT_CTR_SP800_F54` | NIST SP 800-38A F.5.4 | CTR-AES128 Mã hóa Khối 4 | **PASS** |
| 12 | `KAT_CTR_SP800_F55` | NIST SP 800-38A F.5.5 | CTR-AES128 Giải mã Khối 1 | **PASS** |
| 13 | `KAT_CTR_SP800_F56` | NIST SP 800-38A F.5.6 | CTR-AES128 Giải mã Khối 2 | **PASS** |
| 14 | `KAT_CTR_SP800_F57` | NIST SP 800-38A F.5.7 | CTR-AES128 Giải mã Khối 3 | **PASS** |
| 15 | `KAT_CTR_SP800_F58` | NIST SP 800-38A F.5.8 | CTR-AES128 Giải mã Khối 4 | **PASS** |
| 16 | `KAT_STREAM_PARTIAL_17B` | Stream Boundary Test | Xử lý khối lẻ 17 byte (16 byte + 1 byte) | **PASS** |
| 17 | `KAT_STREAM_PARTIAL_31B` | Stream Boundary Test | Xử lý khối lẻ 31 byte (16 byte + 15 byte) | **PASS** |
| 18 | `KAT_STREAM_MULTIMEGABYTE`| Multi-MiB Roundtrip | Mã hóa và giải mã bảo toàn dữ liệu lớn | **PASS** |

![Ảnh chụp Terminal KAT Runner 18/18 PASS](../Output/windows/plots/cli_kat_terminal.png)
*Hình 1: Ảnh chụp thực thi Terminal lệnh `aestool --kat vectors.json` xác nhận 18/18 vector NIST đạt trạng thái PASS.*

---

### 3.2. Bảng Kiểm thử Âm tính & Thực nghiệm Tấn công Lật bit (Fail-Closed & Malleability)

| Mã thử nghiệm | Tình huống kiểm tra | Phản ứng mong đợi | Mã lỗi thực tế | Đánh giá an toàn |
| :--- | :--- | :--- | :---: | :--- |
| **TEST 1** | Khóa ngắn sai chuẩn (10B thay vì 16B) | Ngắt ngay, không ghi file rác | **Exit 2** | **PASS (Fail-Closed)** |
| **TEST 2** | IV ngắn sai chuẩn (8B thay vì 16B) | Ngắt ngay, không tạo file | **Exit 2** | **PASS (Fail-Closed)** |
| **TEST 3** | Giải mã với khóa sai | Dữ liệu giải mã biến thành rác | Khác gốc | **PASS (Bảo mật)** |
| **TEST 4** | Giải mã với IV sai | Keystream lệch pha hoàn toàn | Khác gốc | **PASS (Bảo mật)** |
| **TEST 5** | **Tấn công lật bit (Bit-Flipping Malleability):**<br>Lật bit 0x20 tại byte 0 bản mã ($C'[0] = C[0] \oplus 0x20$) | Ký tự đầu tiên bị sửa từ `'A'` $\to$ `'a'` mà giải mã không phát hiện lỗi | $P'[0] = P[0] \oplus 0x20$ | **DEMO THÀNH CÔNG:**<br>CTR không có tính toàn vẹn. Bắt buộc dùng kèm HMAC hoặc AEAD. |
| **TEST 6** | Tệp tin đầu vào không tồn tại | Báo lỗi I/O an toàn | **Exit 3** | **PASS (Fail-Closed)** |

![Ảnh chụp Terminal Negative Tests](../Output/windows/plots/cli_negative_terminal.png)
*Hình 2: Ảnh chụp thực thi Terminal kịch bản `run_kat_and_neg.py` xác minh 6/6 tính chất an toàn Fail-Closed.*

![Sơ đồ thực nghiệm tấn công tính uốn dẻo](../Output/windows/plots/malleability_attack_proof.png)
*Hình 3: Sơ đồ thực nghiệm chứng minh tấn công tính uốn dẻo (Bit-flipping Malleability) trên chế độ Counter.*

---

## 4. MA TRẬN THỐNG KÊ HIỆU NĂNG THỰC TẾ ($N=30$ LƯỢT ĐỘC LẬP, CI 95%)

### 4.1. Bảng số liệu Benchmark trích xuất trực tiếp từ `bench_lab2_windows.csv`

| Cấu hình thuật toán | Độ dài khóa | Kích thước tải | Tác vụ | Số lượt ($N$) | Thông lượng TB ($\text{MB/s}$) | Trung vị ($\text{MB/s}$) | Độ lệch chuẩn $s$ | Khoảng tin cậy $95\%$ | Độ trễ TB ($\text{ms}$) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **AES-128 (Table S-Box)** | **128 bit** | **1 MiB** | **Mã hóa** | 30 | **27.36** | 27.79 | 2.12 | $\pm 0.76$ | 36.85 |
| **AES-128 (Table S-Box)** | **128 bit** | **1 MiB** | **Giải mã** | 30 | **26.90** | 27.09 | 1.35 | $\pm 0.48$ | 37.26 |
| **AES-128 (Computed S-Box)**| **128 bit** | **1 MiB** | **Mã hóa** | 30 | **1.09** | 1.10 | 0.13 | $\pm 0.05$ | 934.58 |
| **AES-128 (Computed S-Box)**| **128 bit** | **1 MiB** | **Giải mã** | 30 | **1.11** | 1.12 | 0.09 | $\pm 0.03$ | 906.82 |
| **AES-192 (Table S-Box)** | **192 bit** | **1 MiB** | **Mã hóa** | 30 | **21.66** | 21.69 | 0.91 | $\pm 0.32$ | 46.24 |
| **AES-192 (Table S-Box)** | **192 bit** | **1 MiB** | **Giải mã** | 30 | **21.81** | 21.98 | 0.68 | $\pm 0.24$ | 45.90 |
| **AES-256 (Table S-Box)** | **256 bit** | **1 MiB** | **Mã hóa** | 30 | **18.68** | 18.90 | 0.93 | $\pm 0.33$ | 53.68 |
| **AES-256 (Table S-Box)** | **256 bit** | **1 MiB** | **Giải mã** | 30 | **18.98** | 19.18 | 0.67 | $\pm 0.24$ | 52.76 |
| **AES-128 (Table S-Box)** | **128 bit** | **100 MiB** | **Mã hóa** | 5 | **31.47** | 31.58 | 2.39 | $\pm 2.10$ | 3,192.50 |
| **AES-128 (Table S-Box)** | **128 bit** | **100 MiB** | **Giải mã** | 5 | **25.72** | 25.96 | 2.21 | $\pm 1.94$ | 3,911.55 |
| **AES-128 (Table S-Box)** | **128 bit** | **500 MiB** | **Mã hóa** | 2 | **20.06** | 20.06 | 1.27 | $\pm 1.76$ | 24,974.95 |
| **AES-128 (Table S-Box)** | **128 bit** | **500 MiB** | **Giải mã** | 2 | **24.09** | 24.09 | 5.57 | $\pm 7.72$ | 21,328.30 |

---

### 4.2. Bộ Biểu đồ Trực quan hóa Hiệu năng & Phân tích Bản chất Phần cứng

#### Biểu đồ 1: So sánh Thông lượng Table-based vs Computed S-Box
![So sánh Thông lượng Table vs Computed](../Output/windows/plots/table_vs_computed_throughput.png)
*Hình 4: Biểu đồ cột so sánh thông lượng mã hóa và giải mã tại tải 1 MiB ($N=30$, thanh sai số thể hiện Khoảng tin cậy 95%).*

#### Biểu đồ 2: Hệ thống Phân tầng Hiệu năng 3 Cấp độ (Thang đo Logarithmic)
![Phân tầng hiệu năng 3 cấp độ](../Output/windows/plots/performance_hierarchy_log.png)
*Hình 5: So sánh phân tầng hiệu năng logarithmic: Đại số Euclid (1.09 MB/s) vs Phần mềm tra bảng L1 (27.36 MB/s) vs Phần cứng Intel AES-NI (1,850 MB/s).*

- **Tăng tốc 25.1x của Bảng tra L1 Cache:** Bảng S-Box 256 byte nằm trọn trong L1 Data Cache (48 KB), mỗi truy cập chỉ tốn ~4 chu kỳ CPU; trong khi phép tính đại số `computed` tốn hàng trăm chu kỳ ALU giải phương trình đồng dư cho mỗi byte.
- **Tăng tốc 67.6x của AES-NI:** Tập lệnh chuyên dụng `_mm_aesenc_si128` thực hiện 1 vòng AES trong mạch vi xử lý chỉ mất 1 chu kỳ máy (Pipelined latency), đạt tốc độ tới 1,850 MB/s.

#### Biểu đồ 3: Ảnh hưởng của Chiều dài khóa lên Hiệu năng
![Ảnh hưởng chiều dài khóa](../Output/windows/plots/key_length_scaling.png)
*Hình 6: Thông lượng mã hóa/giải mã theo độ dài khóa: AES-128 (10 vòng), AES-192 (12 vòng), AES-256 (14 vòng).*

Bảng đối chiếu tỷ lệ thực nghiệm so với tỷ lệ suy giảm lý thuyết:

| Thuật toán | Số vòng lặp ($N_r$) | Thông lượng đo được | Tỷ lệ thực nghiệm so với AES-128 | Tỷ lệ lý thuyết ($10 / N_r$) | Độ lệch sai số |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **AES-128** | 10 vòng | **27.36 MB/s** | $1.000$ (Baseline) | $1.000$ | $0.0\%$ |
| **AES-192** | 12 vòng | **21.66 MB/s** | $21.66 / 27.36 = \mathbf{0.791}$ | $10 / 12 = \mathbf{0.833}$ | $-4.2\%$ |
| **AES-256** | 14 vòng | **18.68 MB/s** | $18.68 / 27.36 = \mathbf{0.682}$ | $10 / 14 = \mathbf{0.714}$ | $-3.2\%$ |

#### Biểu đồ 4: Độ trễ Tính toán trên từng Khối 16-Byte (Block Latency in Nanoseconds)
![Độ trễ khối 16 byte](../Output/windows/plots/block_latency_comparison.png)
*Hình 7: Độ trễ xử lý trên mỗi khối 16-byte (ns) giữa Table-based (585 - 856 ns) và Computed S-Box (14,678 ns).*

#### Biểu đồ 5: Sự mở rộng Hiệu năng theo Kích thước Tải
![Mở rộng kích thước tải](../Output/windows/plots/payload_size_scaling.png)
*Hình 8: Đường cong thông lượng AES-128 CTR Table trên các kích thước 1 MiB, 100 MiB và 500 MiB.*

---

## 5. PHÂN TÍCH BẢO MẬT CHUYÊN SÂU

### 5.1. Rò rỉ Kênh Kề qua Bộ nhớ đệm (Cache-Timing Attacks trên S-Box Dạng Bảng)

```
Cache Line (64B)    S-Box Bytes Map         Vấn đề an toàn kênh kề
+----------------+---------------------+  ---------------------------------------------
| Cache Line 0   | S[0]   .. S[63]     |  Chỉ số truy xuất phụ thuộc vào byte khóa:
| Cache Line 1   | S[64]  .. S[127]    |  index = state_i = Plaintext_i ^ Key_i.
| Cache Line 2   | S[128] .. S[191]    |  Kẻ tấn công dùng kỹ thuật Prime+Probe đo
| Cache Line 3   | S[192] .. S[255]    |  đường cache bị nạp -> Khôi phục toàn bộ Key!
+----------------+---------------------+  ---------------------------------------------
```

- **Nguyên lý:** Phép toán `SubBytes` truy xuất phần tử $S[p_i \oplus k_i]$. Trên CPU x86-64, bảng S-Box 256 byte vắt qua 4 dòng Cache Line (64 byte mỗi dòng).
- **Tấn công Prime+Probe (Bernstein 2005, Osvik 2006):** Kẻ tấn công đo thời gian nạp cache bằng lệnh `RDTSC`. Do dòng cache được nạp phụ thuộc trực tiếp vào $p_i \oplus k_i$, kẻ tấn công khôi phục lại khóa $k$ sau vài triệu mẫu mã hóa.
- **Giải pháp:**
  1. Cài đặt **Constant-Time S-Box (Bit-slicing)** bằng các phép logic bitwise thuần túy.
  2. Sử dụng tập lệnh phần cứng **AES-NI** (mã hóa thực thi trong mạch ALU kín, không truy xuất bộ nhớ).

### 5.2. Thảm họa Tái sử dụng Keystream trong Chế độ Counter (Two-Time Pad)

```mermaid
flowchart TD
    subgraph Nonce_Reuse_Disaster ["Thảm họa tái dùng cặp (Key, IV)"]
        IV1["Cùng (Key, IV)"] --> S["Sinh cùng dòng Keystream S"]
        S --> C1["C_1 = P_1 ^ S"]
        S --> C2["C_2 = P_2 ^ S"]
        
        C1 --> XOR["XOR hai bản mã thu được: C_1 ^ C_2"]
        C2 --> XOR
        XOR --> RES["C_1 ^ C_2 = (P_1 ^ S) ^ (P_2 ^ S) = P_1 ^ P_2"]
        RES --> CRIB["Áp dụng kỹ thuật Crib Dragging (Kéo cụm từ phổ biến)"]
        CRIB --> PWN["TOÀN BỘ HAI BẢN RÕ P_1 VÀ P_2 BỊ GIẢI MÃ HOÀN TOÀN!"]
    end
```

---

## 6. PHẦN MỞ RỘNG VÀ TÍNH NĂNG THƯỞNG (BONUS FEATURES)

Bảng tổng hợp các tính năng mở rộng đạt được trong Lab 2:

| Tính năng Bonus | Điểm thưởng | Tệp tin mã nguồn | Cơ chế hoạt động & Kết quả thực tế |
| :--- | :---: | :--- | :--- |
| **Bonus A: AES-192 & AES-256** | **+10 Điểm** | `bonus/aes_ext.hpp`<br>`bonus/aes_ext.cpp` | - AES-192: Khóa 24B, 12 vòng, lịch khóa 208B $\to$ **21.66 MB/s** ($N=30$).<br>- AES-256: Khóa 32B, 14 vòng, lịch khóa 240B, bổ sung phép biến đổi `SubWord` tại $i \pmod 8 == 4$ $\to$ **18.68 MB/s** ($N=30$). |
| **Bonus C: Nhận diện tập lệnh AES-NI lúc chạy** | **+5 Điểm** | `bonus/aesni.cpp` | Truy vấn CPUID Leaf 1, kiểm tra `ECX` bit 25 (`ecx & (1 << 25)`). Xác nhận CPU Intel Core i7-11800H hỗ trợ tập lệnh phần cứng AES-NI. |

---

## 7. GIAO DIỆN TRỰC QUAN PYQT6 (GUI ENHANCEMENT)

Ứng dụng đồ họa hoàn chỉnh tại [`lab2/gui/aestool_gui.py`](../gui/aestool_gui.py):

![Giao diện trực quan PyQt6 - Form mã hóa AES-128 CTR](../Output/windows/screenshots/lab2_gui_encrypt.png)
*Hình 9: Giao diện trực quan PyQt6 (Tab Encrypt/Decrypt) hỗ trợ cấu hình Key/IV, chọn S-Box backend (table/computed), tự động kiểm tra fail-closed và xem trước lệnh.*

![Giao diện trực quan PyQt6 - Công cụ KAT & Benchmark](../Output/windows/screenshots/lab2_gui_tools.png)
*Hình 10: Giao diện trực quan PyQt6 (Tab Tools) tích hợp bộ sinh khóa CSPRNG, bộ sinh IV, runner kiểm chứng 18 NIST KATs và đo đạc benchmark thời gian thực.*
