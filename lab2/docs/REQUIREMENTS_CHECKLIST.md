# Lab 2 — Đối chiếu yêu cầu (đủ / thiếu / việc còn lại)

Đối chiếu với đề Lab 2 trong `Cryptography_Labs.docx` (tóm tắt ở `docs/FINAL_LABS_REQUIREMENTS.md`) và các quy tắc chung của đề. Trạng thái được kiểm tra bằng cách đọc
code và **chạy thật** trên Windows (MinGW-w64 GCC 16.2, MSVC 19.51): `ctest` 4/4, KAT 31/31, cross-validation 1722 ca, 0 lỗi. Ubuntu chỉ dùng **Ubuntu 24.04 cài trực tiếp**
(GCC 13.3); log WSL2 cũ đã bị xóa. Hiện Ubuntu có số đo benchmark và `env_ubuntu.txt`; log KAT/xval/ctest/negative của Ubuntu đang chờ chạy (`LINUX_RUNBOOK.md` mục 7).

Ký hiệu: ✅ đạt · 🟡 đạt một phần / cần bạn làm · ❌ chưa có.

## 1. Ràng buộc của đề Lab 2

| Yêu cầu | Trạng thái | Ghi chú / bằng chứng |
|---|:---:|---|
| Không dùng thư viện mật mã ngoài (OpenSSL, Crypto++, libsodium) trong lõi và công cụ nộp | ✅ | `aestool` chỉ link STL và API hệ điều hành. Đã kiểm: import DLL của `aestool.exe` không có `libcrypto`; `ldd aestool` trên Linux (static) không có thư viện nào |
| OpenSSL / Crypto++ chỉ để cross-validation và so sánh hiệu năng | ✅ | Tách thành `aestool_ref` (build khi máy có thư viện). `aestool` không có `xval` và `bench --ref` |
| Số ngẫu nhiên (key, IV, payload benchmark) | ✅ | CSPRNG của hệ điều hành (`BCryptGenRandom` / `getrandom`), không `rand()`/`std::random` (đã grep). Đề Lab 2 cấm OpenSSL nên không dùng `RAND_bytes` ở `aestool` |
| AES-NI chỉ trong phần bonus | ✅ | Backend `aesni` chỉ chạy khi chọn `--sbox aesni`; mặc định là `table` |

## 2. Thuật toán và phạm vi

| Yêu cầu | Trạng thái | Ghi chú |
|---|:---:|---|
| AES-128 theo FIPS-197: AddRoundKey, SubBytes (tra bảng), ShiftRows, MixColumns, KeyExpansion; block 128 bit, key 128 bit, 10 vòng | ✅ | `src/aes128.cpp`; KAT FIPS-197 Appendix B, C.1; round key A.1 trong unit test |
| CTR theo SP 800-38A, IV 128 bit (nonce + counter) | ✅ | `src/ctr.*`; KAT SP 800-38A F.5.1 |
| Ghi rõ endianness counter và hành vi tràn | ✅ | Big-endian 128 bit; không cho wrap, báo lỗi exit 5 và không để lại file dở (`ctr.hpp`, README mục 2.2) |
| Độ dài bất kỳ, block cuối ngắn, không padding | ✅ | KAT các độ dài 0, 1, 15, 17, 37, 63 byte |
| Thảo luận tái dùng IV là thảm họa | 🟡 | Có trong README mục 8; cần có trong báo cáo (viết sau khi có số liệu) |
| Bonus AES-192/256 (+10) | ✅ | Lịch khóa đầy đủ; KAT FIPS-197 C.2/C.3, SP 800-38A F.5.3/F.5.5; xval với OpenSSL và Crypto++ |
| Bonus AES-XTS (+5) | ✅ | Hai khóa, nhân tweak GF(2^128), ciphertext stealing; IEEE 1619 vector 1, 2, 3, 15; xval với OpenSSL |
| Bonus AES-NI (+5) | ✅ | Phát hiện CPU lúc chạy, so khớp với phần mềm; đo hiệu năng trong benchmark |

## 3. CLI và đầu vào/ra

| Yêu cầu | Trạng thái | Ghi chú |
|---|:---:|---|
| `aestool encrypt/decrypt --mode ctr --key key.bin --iv iv.bin --in … --out …` | ✅ | Chạy đúng; round-trip khớp (`negative_*.txt` mục A) |
| Key đúng 16 byte, IV đúng 16 byte (CTR); AES-192/256 là bonus | ✅ | Sai độ dài: exit 3 |
| Fail closed: key/IV sai, tràn counter, lỗi I/O | ✅ | exit 3 / 5 / 4; không ghi file khi lỗi |
| Quy tắc chung: `--in/--text`, `--out`, `--key/--key-hex`, `--iv`, `--nonce`, `--mode`, `--aead`, `--encode`, `--threads`, `--kat`, `--verbose` | ✅ | `--aead` bị từ chối (CTR không có tag); `--threads` dùng cho `bench` |
| Hex trên màn hình, raw trong file, UTF-8 | ✅ | |
| Không tìm file ngoài đường dẫn đã cho | ✅ | **Đã sửa lỗi trong phiên này**: trước đây công cụ tự tìm file ở thư mục cha, cạnh file chạy và thư mục gốc dự án, có thể lặng lẽ lấy nhầm file khóa. Có test G3 |
| Không tự ghi file | ✅ | **Đã sửa**: `--kat`, `xval`, `bench` trước đây tự ghi vào thư mục `Output/`. Giờ chỉ ghi khi có `--out` / `--csv` |

## 4. Kiểm chứng và kiểm thử

| Yêu cầu | Trạng thái | Ghi chú |
|---|:---:|---|
| KAT FIPS-197 và SP 800-38A CTR (`--kat vectors.json`, PASS/FAIL từng ca, tổng kết) | ✅ | 31/31 trên cả ba môi trường |
| Cross-validation: giống hệt Crypto++ hoặc OpenSSL | ✅ | Cả hai: 1722 ca (`xval_*.txt`) |
| Negative: sai key, sai IV, ciphertext bị sửa; giải thích vì sao CTR không phát hiện | ✅ | `negative_*.txt` mục B, C, D (lật 1 bit ciphertext thì lật đúng 1 bit plaintext); giải thích: viết trong báo cáo |
| Unit test Catch2, `ctest` pass trên Windows và Linux | ✅ | 4 test (unit 32 case / 3276 assertion, KAT, CLI end-to-end, cross-validation) |
| Log bằng chứng theo hệ điều hành | ✅ | Windows (MinGW, MSVC) và Ubuntu 24.04: kat, xval, ctest, negative, env (kết quả Ubuntu giống Windows: KAT 31/31, xval 0 lỗi, ctest 4/4, mã thoát 14/3/4/10/4/1) |
| CI | ❌ (tùy chọn) | Chưa có workflow |

## 5. Build và đóng gói

| Yêu cầu | Trạng thái | Ghi chú |
|---|:---:|---|
| CMake, out-of-source, 4 lệnh | ✅ | Đã chạy đúng `mkdir build && cd build && cmake .. && cmake --build .` trên MinGW và Ubuntu; MSVC cần thêm toolchain vcpkg |
| Ubuntu LTS, MSVC, MinGW64 | ✅ | Cả ba build sạch (kể cả `-DLAB2_WERROR=ON` trên MinGW) |
| Không phải sửa đường dẫn | ✅ | **Đã sửa**: bỏ `D:/UIT/...` và `~/Desktop/nt219lib`; OpenSSL/Crypto++ tự tìm; PATH của MSYS2/WSL không làm lẫn thư viện |
| README: phụ thuộc kèm phiên bản, cài đặt Win/Linux, build, ví dụ CLI, hạn chế | ✅ | `README.md` mục 4, 5, 6, 12 |
| Script build, binary Windows và Linux | ✅ | `scripts/build_*`; `release/windows/aestool_{mingw,msvc}.exe`, `aestool_ref_*.exe`, `release/ubuntu/aestool_gcc` (tĩnh) |

## 6. Hiệu năng

| Yêu cầu | Trạng thái | Ghi chú |
|---|:---:|---|
| Protocol: warm-up 1-2 s, khối ~1000 thao tác, 30-100 lần | ✅ (code) | `bench`: warm-up 1,5 s mỗi cấu hình, `ops = min(1000, 256 MiB / size)`, `--runs 30` |
| 1 MiB, 100 MiB, 1 GiB; mã hóa và giải mã | ✅ | Đã đo trên Windows và Ubuntu 24.04, 30 lần lặp (`output/<os>/bench/bench_aes128_*.csv`) |
| Mean, median, SD, CI 95%; bảng phần cứng/OS; governor | ✅ (công cụ) | `scripts/csv_to_stats_table.py`; `bench` in phần cứng và cờ build |
| Table S-box so với computed S-box | ✅ | `table` và `computed` đo ở 1 MiB và 100 MiB trên cả hai OS (báo cáo mục 4.4); `computed` lệch protocol có chủ ý (2 và 1 thao tác mỗi khối), đã ghi rõ. Không đo thêm cấu hình 4 KiB |
| So sánh với Crypto++ (và OpenSSL) | ✅ | `--ref all`, có trong các CSV |
| Phân tích bộ nhớ, cache, branch prediction | ✅ (suy luận) | Báo cáo mục 4.5; bộ nhớ lấy từ mã, cache và nhánh là **suy luận, không đo** (ghi rõ) |
| Bảng MB/s và biểu đồ có error bar | ✅ | Báo cáo mục 4.3 đến 4.6; `scripts/plot_compare.py` sinh `output/compare/*.png` |

## 7. Báo cáo

| Yêu cầu | Trạng thái | Ghi chú |
|---|:---:|---|
| Phần bảo mật: rò rỉ timing/cache của S-box dạng bảng, T-table, constant-time, tái dùng keystream (two-time pad), CTR chỉ bảo mật, cần MAC, không phải AEAD, AES-NI | ✅ | `docs/lab2-main-report.md` / `.pdf` mục 5.1 đến 5.3 |
| Threat model, misuse, secure defaults, fail-closed, limitations | ✅ | Mục 5.4 |
| So sánh Windows và Linux, bài học | ✅ | Mục 4.6, 6.2, 6.3 |
| Báo cáo Lab 2 (PDF, 14 trang) | ✅ | `docs/lab2-main-report.pdf`, sinh từ số liệu và log thật, đã gồm log Ubuntu 24.04. MSSV 25521257 đã cập nhật trên bìa |
| PDF/DOCX duy nhất cho cả 6 lab (≥ 60 trang), cam kết liêm chính, khai báo AI, bảng tự chấm | ❌ | Làm sau khi xong các lab |

## 8. Việc còn lại

1. Không còn việc chạy nào cho Lab 2.
2. Đã xác nhận: binary Windows dùng để đo là `release/windows/aestool_ref_mingw.exe` (theo lịch sử lệnh PowerShell); MSSV 25521257.
3. Báo cáo gộp 6 lab (≥ 60 trang) kèm cam kết liêm chính, khai báo AI và bảng tự chấm: làm sau khi xong các lab.
4. Tùy chọn: CI, GUI (đã có `gui/aestool_gui.py` nhưng gọi đường dẫn `Output/` cũ, chưa cập nhật), đo AES-NI/AES-192/256 chính thức.

## 9. Hạn chế đã biết (ghi trung thực trong báo cáo)

- Bản `table` và `ttable` không constant-time (đúng chủ ý của đề để phân tích rò rỉ cache). `computed` không có bảng tra phụ thuộc bí mật nhưng chưa được kiểm chứng bằng công cụ constant-time.
- Hai dòng SHA-256 của `kat/vectors.json` trong `env_windows.txt` và `env_ubuntu.txt` khác nhau chỉ do kiểu xuống dòng (CRLF/LF), nội dung giống nhau.
- Windows Clang chưa được kiểm chứng.
- Giá trị `computed` ở 1 MiB đo với 2 thao tác mỗi khối (lệch protocol có chủ ý, vì chạy ~1 MB/s).
