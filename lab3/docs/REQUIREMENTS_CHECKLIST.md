# Lab 3 — Đối chiếu yêu cầu (đủ / thiếu / việc còn lại)

Đối chiếu với đề Lab 3 trong `Cryptography_Labs.docx` và các quy tắc chung của đề. Trạng thái được kiểm tra bằng cách đọc code và **chạy thật** trên Windows (MinGW-w64 GCC 16.2, MSVC 19.51):
`ctest` 3/3, KAT 124/124, unit test 45 case (855 assertion), CLI end-to-end 38 kiểm tra. **Ubuntu 24.04 chưa chạy** (xem `UBUNTU_FINISH.md`).

Ký hiệu: ✅ đạt · 🟡 đạt một phần / cần bạn làm · ❌ chưa có · ➖ không làm (có chủ ý).

## 1. Thuật toán và định dạng (rubric: OAEP 20, hybrid 20, khóa và định dạng 15)

| Yêu cầu | Trạng thái | Ghi chú / bằng chứng |
|---|:---:|---|
| RSA-3072 với OAEP SHA-256, MGF1-SHA-256 | ✅ | `src/rsa_oaep.*`; KAT Wycheproof 37/37 |
| RSA-4096 để so sánh | ✅ | unit test `[slow]` và benchmark |
| OAEP label tùy chọn | ✅ | `--label` / `--label-hex`; test đúng/sai/thiếu label |
| Giới hạn `mLen ≤ k − 2hLen − 2` (318 / 446 byte) | ✅ | `max_plaintext_len`; test biên 318/319 và 446/447 |
| Mã hóa lai: khóa AES-256 ngẫu nhiên, AES-GCM, khóa bọc bằng OAEP, envelope có cấu trúc | ✅ | `src/hybrid.*`, `src/envelope.*` |
| AES-256-GCM, IV 96 bit, tag 128 bit, giải mã có xác thực, từ chối bản mã bị sửa | ✅ | KAT NIST 75 ca mã hóa + 12 ca phải thất bại |
| Header JSON đúng trường của đề (`mode`, `rsa_modulus`, `hash`, `wrapped_key`, `iv`, `tag`) | ✅ | `docs/DESIGN.md` mục 4 |
| Tự chuyển hybrid khi vượt giới hạn | ✅ | test `auto mode` và e2e |
| Tag kiểm bằng thư viện (constant-time) | ✅ | Crypto++ `DecryptAndVerify` |
| `keygen`: PEM + DER + metadata JSON `{creation_time, modulus_bits, hash}` | ✅ | `test_keyio`, e2e; OpenSSL đọc được PEM do `rsatool` sinh |
| Khóa PEM / DER cho cả hai chiều | ✅ | tự nhận theo nội dung |
| Ciphertext raw / hex / base64 | ✅ | `--encode`; test round-trip và mã hóa hỏng |
| Chống mã hóa cỡ khóa không hỗ trợ | ✅ | khóa 2048 bit bị từ chối (exit 2); `keygen --bits 2048` exit 1 |

## 2. Kiểm thử (rubric: negative test 15)

| Yêu cầu | Trạng thái | Ghi chú |
|---|:---:|---|
| Sửa bản mã RSA → giải mã thất bại | ✅ | `negative_*` mục B |
| Sửa bản mã AES-GCM → lỗi tag | ✅ | mục C |
| Sai khóa riêng → thất bại | ✅ | mục E (direct và hybrid) |
| Sai label OAEP → thất bại | ✅ | mục F |
| Sửa header envelope → thất bại | ✅ | mục G (iv, wrapped_key, modulus, trường thêm, header_len, magic, cắt cụt) |
| Kiểm tra ciphertext hỏng, label sai, cỡ khóa không hỗ trợ, mã hóa sai | ✅ | mục H, I, J |
| Thông báo lỗi rõ, không rò rỉ; fail closed khi phân tích | ✅ | một thông báo chung `Error: decryption failed.`; không để lại tệp đầu ra |
| Unit test tự động; `ctest` pass trên Windows và Linux | 🟡 | Windows: 3/3 (MinGW và MSVC). Ubuntu: chờ chạy |
| KAT (`--kat vectors.json`, PASS/FAIL từng ca) từ nguồn chính thức | ✅ | Wycheproof (OAEP) và NIST CAVP (GCM); `kat/SOURCES.md` |
| Đối chiếu chéo với thư viện khác | ✅ | OpenSSL giải mã bản mã Crypto++ (có label); `docs/TEST_EVIDENCE.md` mục K |
| Log bằng chứng theo hệ điều hành | 🟡 | Windows (MinGW, MSVC): đủ kat, ctest, negative, env. Ubuntu: chờ chạy |

## 3. Build và đóng gói (rubric: build và tài liệu 5)

| Yêu cầu | Trạng thái | Ghi chú |
|---|:---:|---|
| CMake, out-of-source, 4 lệnh | ✅ | đã chạy trên MinGW và MSVC; Ubuntu chờ chạy |
| Không phải sửa đường dẫn | ✅ | không có đường dẫn cứng; thư viện tự tìm |
| README: phụ thuộc kèm phiên bản, cài đặt, build, ví dụ CLI, hạn chế | ✅ | `README.md` |
| Binary Windows và Linux | 🟡 | Windows: `release/windows/` (MinGW, MSVC, tĩnh). Ubuntu: dựng trên 24.04 (chờ chạy) |
| CLI chuẩn chung (`--in/--text --out --encode --verbose`...) | ✅ | `--threads` không áp dụng cho `rsatool` (không có tác vụ song song) |
| Ngẫu nhiên an toàn, không `rand()` | ✅ | `AutoSeededRandomPool` (đã grep) |

## 4. Hiệu năng (rubric: 15)

| Yêu cầu | Trạng thái | Ghi chú |
|---|:---:|---|
| Thời gian sinh khóa, mã hóa, giải mã RSA; so sánh 3072 và 4096 | ✅ (công cụ) · 🟡 (số liệu) | `rsabench --suites keygen,rsa`; **bạn chạy** theo `BENCHMARK_RUNBOOK.md` |
| Thông lượng AES-GCM; hybrid 1 KiB / 1 MiB / 100 MiB | ✅ (công cụ) · 🟡 (số liệu) | `--suites aesgcm,hybrid` |
| Protocol: warm-up, khối thao tác, 30 lần lặp, mean/median/SD/CI95 | ✅ | `rsabench` + `scripts/analyze_bench.py`; độ lệch (keygen 1 khóa mỗi lần lặp, khối ~1,5 s) ghi rõ |
| Bảng phần cứng/OS, biểu đồ có thanh sai số | ✅ (công cụ) | `bench_env.txt`, `env_*.txt`, 5 biểu đồ |
| Phân tích (giải mã so với mã hóa RSA, cỡ khóa, ưu thế hybrid, vì sao đối xứng áp đảo, bộ nhớ và CPU) | ❌ | viết trong báo cáo sau khi có số liệu |

## 5. Báo cáo (rubric: 10)

| Yêu cầu | Trạng thái | Ghi chú |
|---|:---:|---|
| Thảo luận bắt buộc: vì sao OAEP an toàn (IND-CCA2), vì sao PKCS#1 v1.5 không an toàn, giới hạn `mLen`, vì sao RSA không mã hóa file lớn trực tiếp | ❌ | chưa viết |
| Phần bảo mật chung (threat model, misuse, secure defaults, fail-closed, limitations), so sánh Windows và Ubuntu, bài học | ❌ | chưa viết |
| PDF/DOCX duy nhất cho cả 6 lab (≥ 60 trang) | ❌ | làm sau khi xong các lab |

## 6. Bonus

| Hạng mục | Trạng thái |
|---|:---:|
| Manual OAEP (+5) | ➖ không làm theo yêu cầu |
| Phân tích bảo mật hybrid, TLS, forward secrecy (+10) | ➖ không làm theo yêu cầu |

## 7. Việc còn lại

1. **Bạn, trên Windows**: chạy 4 lệnh benchmark (`BENCHMARK_RUNBOOK.md` mục 2).
2. **Bạn, trên Ubuntu 24.04**: làm theo `UBUNTU_FINISH.md` (build, log, binary tĩnh, benchmark).
3. **Mình**: biểu đồ, bảng thống kê, báo cáo Lab 3 và PDF.

## 8. Hạn chế đã biết

- Tính constant-time của OAEP và so sánh tag là của Crypto++, chưa được kiểm chứng riêng.
- Header envelope được phân tích trước RSA nên thời gian từ chối một envelope sai định dạng ngắn hơn thời gian từ chối một envelope sai OAEP.
- Hybrid đọc cả tệp vào RAM (1 GiB tối đa); chưa có chế độ streaming.
- Hash SHA-256 của `kat/vectors.json` khác nhau giữa Windows (CRLF do git) và Ubuntu (LF) dù nội dung giống nhau.
- Windows Clang chưa được kiểm chứng.
