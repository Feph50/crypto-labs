# BÁO CÁO TỔNG KẾT TUẦN 2 — LAB 2: PURE C++ AES-128 IN CTR MODE

**Học viên:** Trần Lâm Nguyên (GitHub: `Feph50`)  
**Môn học:** Mật mã học và ứng dụng (NT219)  
**Thời gian thực hiện:** Tuần 2  
**Nền tảng kiểm thử:** Windows 11 Pro 64-bit | Intel(R) Core(TM) i7-11800H @ 2.30GHz | 32 GB RAM

---

## 1. MỤC TIÊU TUẦN 2
Hoàn thành toàn diện **Lab 2: Pure C++ AES-128 in CTR mode**, tuân thủ nghiêm ngặt FIPS-197 và NIST SP 800-38A, bao gồm:
1. Triển khai hoàn toàn bằng C++ thuần, không dùng thư viện ngoài cho nhân mã hóa.
2. Kiểm chứng tính đúng đắn qua 18 NIST Known Answer Tests (KAT) và đối chiếu với OpenSSL.
3. Bộ kiểm thử âm tính chứng minh cơ chế fail-closed và thực nghiệm tấn công tính uốn dẻo (Bit-flipping Malleability Attack).
4. Ma trận Benchmark đo đạc hiệu năng với $N=30$ runs (CI 95%) cho các cấu hình Table vs Computed S-Box, độ dài khóa 128/192/256 bit, kích thước tải 1 MiB, 100 MiB, 500 MiB.
5. Triển khai các tính năng Bonus: AES-192/256 và nhận diện phần cứng AES-NI lúc chạy.
6. Báo cáo học thuật toàn diện (`lab2/docs/lab2-main-report.md`) và giao diện PyQt6 GUI.

---

## 2. KẾT QUẢ ĐẠT ĐƯỢC

### 2.1. Kiểm thử NIST KAT (18/18 PASS)
- FIPS-197 Appendix B & C (Khối AES-128): 7/7 PASS.
- NIST SP 800-38A (CTR-AES128 mã hóa & giải mã): 8/8 PASS.
- Kiểm thử luồng byte lẻ (17 bytes, 31 bytes) và kiểm tra bảo toàn nhiều Megabyte: 3/3 PASS.
- Bằng chứng lưu tại: [`lab2/Output/windows/kat/kat_results.txt`](file:///D:/MMH_Final_Labs/Final_Lab%20Tasks/NT219-Crypto-Labs/lab2/Output/windows/kat/kat_results.txt).

### 2.2. Kiểm thử âm tính & An toàn (6/6 PASS)
- **Sai độ dài khóa / IV:** Báo lỗi tham số, thoát ngay với Exit Code 2 (Fail-closed).
- **Tệp không tồn tại:** Báo lỗi I/O an toàn, thoát với Exit Code 3, không sinh file rác.
- **Thực nghiệm tính uốn dẻo:** Lật bit 0x20 ở ciphertext làm biến đổi chính xác ký tự tương ứng ở plaintext từ `'A'` thành `'a'` mà không bị phát hiện, chứng minh trực quan lý do chế độ CTR bắt buộc phải đi kèm MAC / AEAD (AES-GCM).
- Bằng chứng lưu tại: [`lab2/Output/windows/negative_tests/negative_results.txt`](file:///D:/MMH_Final_Labs/Final_Lab%20Tasks/NT219-Crypto-Labs/lab2/Output/windows/negative_tests/negative_results.txt).

### 2.3. Đo lường hiệu năng thực tế ($N=30$, CI 95%)
- Dữ liệu CSV: [`lab2/Output/windows/bench/bench_lab2_windows.csv`](file:///D:/MMH_Final_Labs/Final_Lab%20Tasks/NT219-Crypto-Labs/lab2/Output/windows/bench/bench_lab2_windows.csv).
- Biểu đồ trực quan:
  1. `table_vs_computed_throughput.png`: Minh họa chênh lệch ~25x giữa Table S-Box (27.36 MB/s) và Computed S-Box (1.09 MB/s).
  2. `key_length_scaling.png`: So sánh thông lượng AES-128 (27.36 MB/s) vs AES-192 (21.66 MB/s) vs AES-256 (18.68 MB/s), tỷ lệ suy giảm đúng theo số vòng lặp 10:12:14.
  3. `payload_size_scaling.png`: Khảo sát sự thay đổi thông lượng trên các kích thước 1 MiB, 100 MiB (31.47 MB/s), 500 MiB (20.06 - 24.09 MB/s).

### 2.4. Tính năng Bonus hoàn thành (+15 điểm)
- **Bonus A (+10 điểm):** AES-192 (12 rounds) & AES-256 (14 rounds) full key expansion và CTR streaming encryption.
- **Bonus C (+5 điểm):** Nhận diện tập lệnh tăng tốc phần cứng AES-NI lúc chạy thông qua CPUID Leaf 1 (ECX bit 25).

---

## 3. TỔNG KẾT ĐIỂM TỰ ĐÁNH GIÁ
- **Điểm lõi:** 100 / 100 điểm.
- **Điểm thưởng (Bonus):** +15 / 20 điểm.
- **Tổng điểm Tuần 2:** **115 / 100 điểm**.
