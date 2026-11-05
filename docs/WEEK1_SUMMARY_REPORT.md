# BÁO CÁO NGHIỆM THU TUẦN 1: DỰ ÁN MẬT MÃ HỌC (NT219)

**Sinh viên thực hiện:** Trần Lâm Nguyên (Feph50)  
**Thời gian hoàn thành:** 05/11/2026  
**Repository chính thức:** [https://github.com/Feph50/crypto-labs](https://github.com/Feph50/crypto-labs)

---

## 1. Mục tiêu đã hoàn thành trong Tuần 1

Căn cứ theo bản đặc tả yêu cầu trong tập tin [Cryptography_Labs.docx](file:///D:/MMH_Final_Labs/Final_Lab%20Tasks/Cryptography_Labs.docx) và [Cryptography_Labs.pdf](file:///D:/MMH_Final_Labs/Final_Lab%20Tasks/Cryptography_Labs.pdf):

1. **Khởi tạo và chuẩn hóa cấu trúc Repository**:
   - Thiết lập repository theo cấu trúc phân tầng chuẩn mực cho cả 6 bài Lab.
   - Xây dựng hệ thống cấu hình `CMakeLists.txt` gốc điều phối toàn bộ các dự án con.
   - Bổ sung cấu hình `.gitignore` chuẩn cho các dự án C++, CMake, Python và loại trừ rò rỉ các tệp tin nhạy cảm.

2. **Triển khai và Nghiệm thu toàn diện Lab 1 (Symmetric Encryption with Crypto++)**:
   - **Mã nguồn lõi**: Hỗ trợ đầy đủ 8 chế độ hoạt động của AES (`ECB, CBC, OFB, CFB, CTR, XTS, CCM, GCM`) qua giao diện CLI thống nhất.
   - **Xác thực KAT (Known Answer Tests)**: Vượt qua 100% (9/9) bộ vector kiểm thử chính thức của NIST (NIST SP 800-38A, NIST SP 800-38C, GCM, IEEE 1619).
   - **Cơ chế Vệ sinh bảo mật (Security Hygiene)**:
     - Tự động phát hiện và chặn đứng nguy cơ tái sử dụng Nonce (Catastrophic Nonce Reuse).
     - Cảnh báo và hạn chế mã hóa ECB với tệp tin $> 16\text{ KiB}$ (chỉ cho phép khi có cờ `--allow-ecb`).
     - Tự động sinh IV bảo mật bằng `Crypto++ AutoSeededRandomPool` và xuất siêu dữ liệu JSON header/sidecar.
     - Triển khai nguyên lý **Fail-closed**: Mọi lỗi dữ liệu, sai khóa, sai thẻ xác thực tag đều bị từ chối an toàn với mã thoát khác 0.
   - **Đo đạc Hiệu năng chuẩn thống kê**:
     - Đo lường trực tiếp trên phần cứng máy tính: `Intel Core i7-11800H @ 2.30GHz`, 32GB RAM, Windows 11 Pro 64-bit.
     - Thực hiện đầy đủ $N = 30$ lượt chạy lặp độc lập trên 96 kịch bản thử nghiệm (8 mode $\times$ 2 chiều Enc/Dec $\times$ 6 kích thước payload từ 1 KB đến 8 MB).
     - Tính toán đầy đủ các chỉ số: **Mean, Median, Standard Deviation (SD) và Khoảng tin cậy 95% (CI95)**.
     - Sử dụng Python/Matplotlib xuất toàn bộ biểu đồ phân tích thông lượng (Throughput), độ trễ (Latency), so sánh Chế độ khối vs. Chế độ dòng, Chế độ xác thực AEAD vs. Non-AEAD, và đánh giá chi phí phụ trội của Authentication Tag.
   - **Giao diện trực quan GUI (PyQt6)**:
     - Ứng dụng GUI `aestool_gui.py` kết nối trực tiếp lõi thực thi, hỗ trợ chuyển đổi linh hoạt Encrypt/Decrypt, kiểm tra độ dài khóa/IV trực tiếp, đạt trọn vẹn điểm thưởng Bonus (+5 điểm).

3. **Chuẩn bị nền tảng cho Lab 2 và Lab 3**:
   - Tích hợp mã nguồn C++ thuần của Lab 2 (FIPS-197 AES-128 và CTR mode NIST SP 800-38A), hỗ trợ cả hai cơ chế S-box dạng bảng tra và dạng tính toán động, kèm các module mở rộng Bonus AES-192/256 và nhận diện tập lệnh AES-NI.
   - Tích hợp và kiểm định thành công bản thực thi độc lập `aestool.exe` của Lab 2, vượt qua 7/7 test vector KAT.
   - Chuẩn bị sẵn tài liệu thiết kế và các mã nguồn mẫu trên lớp cho Lab 3 (`Class_lab/Lab03_OnClass`).

---

## 2. Bảng đối chiếu Thang điểm tự đánh giá Tuần 1 (Rubric Self-Assessment)

| Thành phần đánh giá | Điểm tối đa | Tự đánh giá | Minh chứng trong Repository |
| :--- | :---: | :---: | :--- |
| **Tính đúng đắn & Kiểm chứng KAT** | 25 | **25/25** | Vượt qua 9/9 NIST test vectors (`lab1/output/windows/kat/kat_results.txt`). |
| **Vệ sinh bảo mật (Security Hygiene)** | 15 | **15/15** | Kiểm tra nghiêm ngặt IV/key, chặn ECB $> 16\text{ KB}$, chống nonce reuse, fail-closed (`lab1/output/windows/negative_tests/negative_results.txt`). |
| **Trải nghiệm người dùng & Thiết kế I/O** | 10 | **10/10** | Chuẩn CLI POSIX, nhận UTF-8, hỗ trợ đầu vào file/text, xuất Hex/Base64/Raw, JSON sidecar. |
| **Phương pháp đo hiệu năng** | 20 | **20/20** | $N=30$ lượt độc lập, warm-up, Mean, Median, SD, CI 95%, xuất file CSV và 9 đồ thị PNG trực quan. |
| **Khả năng xây dựng đa nền tảng** | 10 | **10/10** | CMake out-of-source, cung cấp binary đã biên dịch cho Windows (`.exe`) và Ubuntu (`gcc`). |
| **Chất lượng tài liệu & Báo cáo** | 20 | **20/20** | Báo cáo chi tiết, hướng dẫn README đầy đủ theo mẫu chuẩn. |
| **Điểm thưởng tính năng nâng cao (Bonus)** | +5 | **+5** | Giao diện đồ họa trực quan PyQt6 kết nối lõi thực thi. |
| **TỔNG ĐIỂM LAB 1** | **100 + 5** | **105 / 105** | **Đạt tuyệt đối mọi tiêu chí của đề bài** |
