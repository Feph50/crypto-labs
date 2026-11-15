# Lab 1 — Đối chiếu yêu cầu (đủ / thiếu / việc còn lại)

Đối chiếu với `docs/FINAL_LABS_REQUIREMENTS.md` (nguồn: `Cryptography_Labs.docx`). Trạng thái được kiểm tra bằng cách đọc code và **chạy thật**
trên MSVC 19.51, MinGW-w64 GCC 16.2 và Ubuntu 26.04 (WSL2, GCC 15.2): `ctest` 3/3, KAT 29/29 trên cả ba.

Ký hiệu: ✅ đạt · 🟡 đạt một phần / cần bạn làm · ❌ chưa có.

## 1. Ràng buộc và build

| Yêu cầu | Trạng thái | Ghi chú / bằng chứng |
|---|:---:|---|
| Chỉ dùng Crypto++ cho nguyên thủy mật mã, **không** OpenSSL/libsodium | ✅ | Đã **bỏ** `find_package(OpenSSL)` và mọi liên kết OpenSSL khỏi `CMakeLists.txt`, script build và README |
| Dùng CMake, out-of-source (`mkdir build && cd build && cmake .. && cmake --build .`) | ✅ | Đã chạy đúng 4 lệnh trên Ubuntu và MinGW64 với gói hệ thống; MSVC cần thêm toolchain vcpkg (xem README mục 5) |
| Biên dịch Ubuntu LTS + Windows MSVC + MinGW64 | ✅ | Cả ba đã build và test từ bản sao sạch |
| **Không phải sửa đường dẫn** | ✅ | `cmake/FindCryptoPP.cmake` tự tìm (package config → pkg-config → prefix chuẩn → `CRYPTOPP_ROOT_DIR`); bỏ đường dẫn cứng `D:/UIT/...` |
| Tránh nhầm thư viện giữa toolchain (MSVC có `msys64` trong PATH, WSL có PATH Windows) | ✅ | Đã gặp và sửa thật (MSVC lấy header MinGW; Linux lấy `/mnt/c/msys64`) |
| Build scripts | ✅ | `scripts/build_linux.sh`, `scripts/build_windows.bat` viết lại, không đường dẫn cứng |
| README: phụ thuộc kèm phiên bản, cài đặt Win/Linux, lệnh build, ví dụ CLI, hạn chế | ✅ | `README.md` mục 4, 5, 8, 11 |
| Binary Windows + Linux | ✅ | `release/windows/aestool_msvc.exe`, `aestool_mingw.exe`, `release/ubuntu/aestool_gcc` (static), build từ code hiện tại |

## 2. Phạm vi Lab 1

| Yêu cầu | Trạng thái | Ghi chú |
|---|:---:|---|
| 8 mode `--mode ecb\|cbc\|ofb\|cfb\|ctr\|xts\|ccm\|gcm` | ✅ | |
| `--aead`, `--aad FILE`, `--aad-text`, kiểm tra tag, fail closed | ✅ | |
| ECB: `WARNING`, chặn > 16 KiB, `--allow-ecb` | ✅ | Đã thêm cảnh báo cả khi giải mã |
| Ép đúng độ dài IV/nonce, từ chối sai độ dài | ✅ | |
| IV thiếu → sinh bằng `AutoSeededRandomPool`, lưu header/sidecar JSON | ✅ | `<out>.json` gồm `alg`, `mode`, `iv`, `aad`, `tag`, `tag_len` |
| Chống tái dùng nonce CTR/CCM/GCM, có tài liệu logic | ✅ | Registry SHA-256(key‖mode‖IV); mô tả ở README mục 10 và báo cáo 3.4 |
| `--key-hex`, `--key` (raw hoặc hex có header) | ✅ | Đã thêm test cho cả hai định dạng và file hỏng |
| `--iv`/`--nonce` nhận file **hoặc** hex | ✅ | |
| Màn hình hex, file raw, `--encode hex\|base64\|raw` | ✅ | |
| UTF-8 vào/ra | ✅ | Đọc argv UTF-8 trên Windows (đã thử `--text "xin chào"`) |
| Chuẩn CLI chung: `--threads`, `--kat`, `--verbose` | ✅ | `--threads` dùng cho `bench` |

## 3. Kiểm chứng và kiểm thử

| Yêu cầu | Trạng thái | Ghi chú |
|---|:---:|---|
| KAT NIST SP 800-38A: CBC, CFB, OFB, CTR | ✅ | AES-128/192/256 |
| KAT NIST GCM, NIST CCM | ✅ | GCM TC10/14/15/16 (và TC2/4), CCM Example 1 và 3 (tag 4, 8), tag 16 cross-check |
| KAT XTS (IEEE 1619) | ✅ | Vector 1, 2, 3, 15 (ciphertext stealing) + XTS-AES-256 cross-check |
| `--kat vectors.json`, JSON, PASS/FAIL từng test, tổng kết | ✅ | 29 vector, **29 passed** |
| Negative: sai key, sai IV, ciphertext sửa (AEAD / non-AEAD), tag sai, IV sai độ dài | ✅ | Thêm: **tag/IV bị sửa trong header**, AAD sai, header hỏng, key file hỏng, XTS ngắn, KAT hỏng |
| Unit test Catch2, `ctest` pass Windows + Linux | ✅ | 3 test ctest: unit, KAT, CLI negative |
| CI | ❌ (tùy chọn) | Chưa có workflow |

## 4. Hiệu năng

| Yêu cầu | Trạng thái | Ghi chú |
|---|:---:|---|
| Protocol: warm-up 1–2 s mỗi thuật toán, khối ~1000 thao tác, 30–100 lần | ✅ (code) | `bench`: warm-up **riêng cho từng mode/chiều**, `--ops`, `--budget-mb`, cảnh báo nếu `--runs` ngoài 30–100 |
| Payload 1 KB … 8 MB; thông lượng + độ trễ | ✅ | |
| Mean, median, SD, CI 95% | ✅ | Bảng 3b trong báo cáo (từ CSV) và script `scripts/csv_to_stats_table.py` |
| So sánh Windows/Linux, stream/block, AEAD/non-AEAD, overhead tag | ✅ | Báo cáo mục 5.5 |
| **Số liệu trong `output/` và biểu đồ** | 🟡 | **Cần bạn chạy lại bản đầy đủ** (số cũ tạo trước khi chuẩn hóa `bench`); lệnh mẫu: `aestool bench --runs 30 --out bench.csv`, rồi `python scripts/plot_bench.py` và `python ../scripts/csv_to_stats_table.py` |
| Bảng phần cứng/OS, governor | 🟡 | Điền theo máy chạy lại |

## 5. Báo cáo

| Yêu cầu | Trạng thái | Ghi chú |
|---|:---:|---|
| Threat model, misuse, known attacks, secure defaults, fail-closed, limitations | ✅ | Mục 3.1–3.6 (**3.6 Secure Defaults mới**), 6.1–6.4 (limitations mở rộng) |
| Phân tích mode / cài đặt / đa nền tảng (đề Lab 1) | ✅ | Mục 6.1–6.3 |
| Lessons learned, cross-platform comparison, kết luận, tham khảo | ✅ | Mục 7, 8, phụ lục P5 (thêm 2 bài học về PATH/đường dẫn) |
| Khai báo AI/công cụ, cam kết liêm chính | 🟡 | Đã cập nhật phạm vi hỗ trợ của AI; **bạn cần đọc lại và xác nhận** |
| Bảng tự chấm điểm | 🟡 | Có; cập nhật điểm sau khi chạy lại benchmark |
| File PDF/DOCX duy nhất cho cả 6 lab | ❌ | Chưa gộp (làm sau cho cả 6 lab) |

## 6. Bonus (+5): export thư viện + GUI

| Yêu cầu | Trạng thái | Ghi chú |
|---|:---:|---|
| Export core thành `.dll` / `.so` / `.a` / `.lib` | 🟡 | Có thư viện **tĩnh** (`libaescore.a` / `aescore.lib`); chưa có `.dll`/`.so` với API C |
| GUI gọi **thư viện đã biên dịch**, không nhân bản logic | 🟡 | GUI PyQt6 có và không chứa mã mật mã, nhưng gọi file `aestool` qua `QProcess`, chưa nạp thư viện |

## 7. Việc còn lại cho Lab 1

1. **Chạy lại benchmark đầy đủ** trên Windows và Ubuntu, cập nhật CSV, biểu đồ, Bảng 3/3b trong báo cáo (bạn đã nói sẽ tự chạy).
2. **Đọc và xác nhận** khai báo AI trong báo cáo.
3. Chạy thử Clang trên Windows nếu muốn giữ dòng Clang trong ma trận build.
4. (Tùy chọn) Bonus: làm `.dll`/`.so` + API C và cho GUI gọi bằng `ctypes`; thêm CI.
5. Xuất báo cáo PDF/DOCX gộp cho cả 6 lab.

## 8. Lệnh kiểm tra nhanh

```bash
# Ubuntu / MSYS2 MinGW64
cd lab1 && mkdir build && cd build && cmake .. && cmake --build . && ctest --output-on-failure && ./aestool --kat ../kat/vectors.json
```
```bat
:: MSVC (x64 Native Tools Command Prompt), vcpkg đã cài cryptopp:x64-windows-static
set VCPKG_ROOT=C:\vcpkg
scripts\build_windows.bat msvc
```
