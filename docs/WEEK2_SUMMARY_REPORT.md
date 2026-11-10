# BÁO CÁO TỔNG KẾT TUẦN 2 — LAB 2: PURE C++ AES-128 IN CTR MODE

**Học viên:** Trần Lâm Nguyên (GitHub: `Feph50`)  
**MSSV:** 22520979  
**Môn học:** Mật mã học và ứng dụng (NT219)  
**Thời gian thực hiện:** Tuần 2  
**Nền tảng kiểm thử:** Windows 11 Pro 64-bit | Intel(R) Core(TM) i7-11800H @ 2.30GHz | 32 GB RAM

---

## 1. MỤC TIÊU TUẦN 2
Hoàn thành toàn diện **Lab 2: Pure C++ AES-128 in CTR mode**, tuân thủ nghiêm ngặt **[NIST FIPS 197 Update 1 (May 09, 2023)](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.197-upd1.pdf)** và **NIST SP 800-38A**, bao gồm:
1. Triển khai hoàn toàn bằng C++ thuần túy, không dùng thư viện ngoài cho nhân mã hóa lõi.
2. Kiểm chứng tính đúng đắn qua **18 NIST Known Answer Tests (KAT)** chuẩn hóa từ FIPS 197 Upd1 / SP 800-38A và đối chiếu với OpenSSL 3.
3. Bộ kiểm thử âm tính chứng minh cơ chế fail-closed và thực nghiệm tấn công tính uốn dẻo (Bit-flipping Malleability Attack).
4. Ma trận Benchmark đo đạc hiệu năng với chuẩn hóa **$N=30$ runs độc lập** (CI 95%) cho toàn bộ cấu hình: Table vs Computed S-Box, độ dài khóa 128/192/256 bit, và kích thước payload **1 MiB, 100 MiB, 1 GiB**.
5. Triển khai các tính năng Bonus: AES-192/256 và nhận diện phần cứng AES-NI lúc chạy.
6. Báo cáo học thuật toàn diện trực quan hóa cao (`lab2/docs/lab2-main-report.md`) với **10 biểu đồ, sơ đồ và ảnh chụp thực tế**.

---

## 2. KẾT QUẢ ĐẠT ĐƯỢC

### 2.1. Kiểm thử NIST KAT (18/18 PASS — 100%)
- **[NIST FIPS 197 Update 1](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.197-upd1.pdf) Appendix B & C (Khối AES-128):** 7/7 PASS (bao gồm Key Expansion, State transitions, và các vector AESAVS).
- **NIST SP 800-38A Section F.5.1 (CTR-AES128 mã hóa & giải mã):** 7/7 PASS (toàn bộ 4 khối 64 bytes và các tiền tố khối lẻ 1B, 15B, 17B, 37B, 63B).
- **Edge cases & Cross-Validation:** 4/4 PASS (input rỗng 0 byte, và 4 bài test lan truyền bộ đếm Big-Endian 128-bit tràn byte thấp, 16-bit, 64-bit, 96-bit).
- Bằng chứng lưu tại: [`lab2/Output/windows/kat/kat_results.txt`](file:///D:/MMH_Final_Labs/Final_Lab%20Tasks/NT219-Crypto-Labs/lab2/Output/windows/kat/kat_results.txt) và ảnh chụp terminal: [`lab2/Output/windows/plots/cli_kat_terminal.png`](file:///D:/MMH_Final_Labs/Final_Lab%20Tasks/NT219-Crypto-Labs/lab2/Output/windows/plots/cli_kat_terminal.png).

### 2.2. Kiểm thử âm tính & An toàn (6/6 PASS)
- **Sai độ dài khóa / IV:** Báo lỗi tham số, thoát ngay với Exit Code 2 (Fail-closed).
- **Tệp không tồn tại:** Báo lỗi I/O an toàn, thoát với Exit Code 3, không sinh file rác.
- **Thực nghiệm tính uốn dẻo:** Lật bit 0x20 ở ciphertext làm biến đổi chính xác ký tự tương ứng ở plaintext từ `'A'` thành `'a'` mà không bị phát hiện, chứng minh trực quan lý do chế độ CTR bắt buộc phải đi kèm MAC / AEAD (AES-GCM).
- Sơ đồ minh chứng: [`lab2/Output/windows/plots/malleability_attack_proof.png`](file:///D:/MMH_Final_Labs/Final_Lab%20Tasks/NT219-Crypto-Labs/lab2/Output/windows/plots/malleability_attack_proof.png) và ảnh chụp terminal: [`lab2/Output/windows/plots/cli_negative_terminal.png`](file:///D:/MMH_Final_Labs/Final_Lab%20Tasks/NT219-Crypto-Labs/lab2/Output/windows/plots/cli_negative_terminal.png).

### 2.3. Đo lường hiệu năng thực tế (=30$ runs, CI 95%) & 10 Hình ảnh Trực quan
- Dữ liệu CSV: [lab2/Output/windows/bench/bench_lab2_windows.csv](file:///D:/MMH_Final_Labs/Final_Lab%20Tasks/NT219-Crypto-Labs/lab2/Output/windows/bench/bench_lab2_windows.csv).
- Toàn bộ các cấu hình đều được chuẩn hóa đo **=30$ lượt chạy độc lập** trên Intel Core i7-11800H:
  - **1 MiB Table (AES-128):** Mã hóa **29.97 MB/s** (33.38 ms) | Giải mã **30.16 MB/s** (33.18 ms)
  - **1 MiB Computed (AES-128):** Mã hóa **1.26 MB/s** (792.48 ms) | Giải mã **1.26 MB/s** (793.70 ms) $\to$ Bảng tra tăng tốc **23.9x**
  - **1 MiB Table (AES-192):** Mã hóa **26.28 MB/s** | Giải mã **26.33 MB/s**
  - **1 MiB Table (AES-256):** Mã hóa **22.85 MB/s** | Giải mã **22.95 MB/s**
  - **100 MiB Table (AES-128, 8 workers):** Mã hóa **17.11 MB/s** | Giải mã **16.96 MB/s**
  - **1 GiB Table (AES-128, 8 workers):** Mã hóa **14.50 MB/s** | Giải mã **13.30 MB/s**
- Danh mục 10 hình ảnh và biểu đồ trực quan nhúng trong báo cáo:
  1. cli_kat_terminal.png: Ảnh chụp terminal 18 NIST KATs PASS theo chuẩn NIST FIPS 197 Update 1 & SP 800-38A.
  2. cli_negative_terminal.png: Ảnh chụp terminal 6 ca Negative Tests Fail-Closed.
  3. malleability_attack_proof.png: Sơ đồ thực nghiệm tấn công tính uốn dẻo lật bit (Bit-flipping).
  4. 	able_vs_computed_throughput.png: Biểu đồ so sánh Table (29.97 MB/s) vs Computed (1.26 MB/s) (=30$).
  5. performance_hierarchy_log.png: Biểu đồ thang Log 3 cấp độ (Đại số Euclid vs Bảng tra L1 vs Intel AES-NI 1,850 MB/s).
  6. key_length_scaling.png: Biểu đồ thông lượng AES-128 vs AES-192 vs AES-256 (=30$ runs mỗi loại).
  7. lock_latency_comparison.png: Biểu đồ độ trễ xử lý nano-giây trên mỗi khối 16-byte (534 ns vs 12,698 ns).
  8. payload_size_scaling.png: Biểu đồ thông lượng theo kích thước tải 1 MiB, 100 MiB, 1 GiB (=30$ runs mỗi kích thước tải).
  9. lab2_gui_encrypt.png: Ảnh chụp giao diện PyQt6 Form Mã hóa/Giải mã CTR.
  10. lab2_gui_tools.png: Ảnh chụp giao diện PyQt6 Tab Công cụ KAT & Benchmark.

### 2.4. Tính năng Bonus hoàn thành (+15 điểm)
- **Bonus A (+10 điểm):** AES-192 (12 rounds) & AES-256 (14 rounds) full key expansion và CTR streaming encryption.
- **Bonus C (+5 điểm):** Nhận diện tập lệnh tăng tốc phần cứng AES-NI lúc chạy thông qua CPUID Leaf 1 (ECX bit 25).

---

## 3. TỔNG KẾT ĐIỂM TỰ ĐÁNH GIÁ
- **Điểm lõi:** 100 / 100 điểm.
- **Điểm thưởng (Bonus):** +15 / 20 điểm.
- **Tổng điểm Tuần 2:** **115 / 100 điểm**.
