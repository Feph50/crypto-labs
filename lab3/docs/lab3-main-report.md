# TRƯỜNG ĐẠI HỌC CÔNG NGHỆ THÔNG TIN, ĐHQG-HCM
## KHOA MẠNG MÁY TÍNH VÀ TRUYỀN THÔNG

# BÁO CÁO THỰC HÀNH
## MÔN HỌC: MẬT MÃ HỌC VÀ ỨNG DỤNG (NT219)

# LAB 3: RSA-OAEP VÀ MÃ HÓA LAI (ENVELOPE ENCRYPTION)
### Crypto++, RSA-3072 / RSA-4096, AES-256-GCM

**Thông tin sinh viên thực hiện:**
- **Họ và tên:** Trần Lâm Nguyên (Feph50)
- **Mã số sinh viên (MSSV):** 25521257
- **Lớp học phần:** NT219.P11.MMTT
- **Thời gian hoàn thành:** Học kỳ I - Năm học 2026–2027

<div class="pagebreak"></div>

# 1. Mục tiêu và chuẩn đầu ra (Objective & Outcomes)

**Đã xây dựng:** công cụ dòng lệnh `rsatool` (`keygen`, `encrypt`, `decrypt`, `--kat`) cài đặt **RSA-OAEP (SHA-256, MGF1-SHA-256)** với RSA-3072 và RSA-4096, và **mã hóa lai** (khóa AES-256 ngẫu nhiên, AES-256-GCM, khóa được bọc bằng RSA-OAEP) trên **Crypto++ 8.9**; công cụ đo `rsabench`; bộ kiểm thử Catch2; bộ KAT từ nguồn chính thức. Phần bonus của đề **không** được thực hiện.

| Chuẩn đầu ra của đề | Mục |
| :--- | :---: |
| Sinh và lưu trữ an toàn cặp khóa RSA | 2.3 |
| Cài RSA-OAEP mã hóa/giải mã (SHA-256) | 2.1, 3 |
| Xử lý giới hạn kích thước bản rõ và cài mã hóa lai | 2.4, 3.3 |
| Phát hiện và xử lý đúng các lỗi mật mã | 2.5, 4.3 |
| Đo hiệu năng RSA theo cỡ khóa | 5 |
| Hiểu vì sao bắt buộc phải mã hóa lai | 3.4, 5.4 |

# 2. Thiết kế và bảo mật (Design & Security)

## 2.1. Tham số

| Hạng mục | Giá trị |
| :--- | :--- |
| Thư viện mật mã | Crypto++ 8.9.0 (OpenSSL chỉ dùng để đối chiếu trong test) |
| RSA | 3072 bit (bắt buộc), 4096 bit (so sánh); e = 65537 (Crypto++ mặc định e = 17 nên phải yêu cầu rõ); cỡ khác bị từ chối |
| OAEP | hash SHA-256, MGF1 dùng SHA-256, label tùy chọn |
| Giới hạn bản rõ trực tiếp | `mLen ≤ k − 2·hLen − 2`: **318 byte** (3072), **446 byte** (4096) |
| Mã hóa lai | AES-256-GCM, IV 96 bit, tag 128 bit, khóa AES ngẫu nhiên cho mỗi lần mã hóa |
| Ngẫu nhiên | `CryptoPP::AutoSeededRandomPool`; không `rand()` / `std::random` |

## 2.2. Cấu trúc mã nguồn

| Thành phần | Tệp | Vai trò |
| :--- | :--- | :--- |
| Khóa | `src/keyio.*` | sinh khóa; PEM (tự viết, vì Crypto++ không có PEM), DER, metadata JSON |
| RSA-OAEP | `src/rsa_oaep.*` | `oaep_encrypt`, `oaep_decrypt`, `max_plaintext_len` |
| Envelope | `src/envelope.*`, `src/hybrid.*` | container, header JSON, AAD, chế độ tự chọn direct/hybrid |
| Kiểm chứng | `src/kat.*`, `tests/` | KAT từ JSON, Catch2, CLI end-to-end |
| Công cụ | `src/cli.*`, `bench/bench_main.cpp` | `rsatool`, `rsabench` |

## 2.3. Quản lý khóa và định dạng

`rsatool keygen --bits 3072 --pub pub.pem --priv priv.pem` ghi năm tệp:

| Tệp | Nội dung |
| :--- | :--- |
| `pub.pem` | khóa công khai, **SPKI** (`-----BEGIN PUBLIC KEY-----`) |
| `priv.pem` | khóa riêng, **PKCS#8** (`-----BEGIN PRIVATE KEY-----`) |
| `pub.der`, `priv.der` | cùng khóa ở dạng DER |
| `priv.meta.json` | `{"creation_time": "<UTC ISO-8601>", "modulus_bits": 3072, "hash": "SHA-256"}` |

Tệp khóa riêng được ghi với quyền 0600 trên Linux; mọi tệp được ghi qua tệp tạm rồi đổi tên, từ chối ghi đè khi thiếu `--force`. Bộ đọc PEM chặt (đúng nhãn BEGIN/END, base64 chuẩn, không header lạ, không dữ liệu sau END); nạp khóa tự nhận PEM hay DER, kiểm tính hợp lệ của khóa, từ chối byte thừa và cỡ khóa khác 3072/4096. Khóa do `rsatool` sinh được OpenSSL đọc bình thường (đã kiểm).

## 2.4. Envelope và AAD

Container dùng chung cho cả hai chế độ để `decrypt` nhận ra chế độ mà không phải đoán:

```
"RSAE" | version = 0x01 | header_len (u32, big-endian) | header (JSON UTF-8, ≤ 64 KiB) | payload
```

Header chế độ hybrid có đúng các trường của đề:

```
{"mode":"RSA-OAEP-AES-GCM","rsa_modulus":3072,"hash":"SHA-256","wrapped_key":"<base64>","iv":"<base64>","tag":"<base64>"}
```

`wrapped_key` là RSA-OAEP của khóa AES 32 byte (k byte), `iv` dài 12 byte, `tag` dài 16 byte; payload là bản mã AES-256-GCM. Chế độ direct có header `{"mode":"RSA-OAEP","rsa_modulus":3072,"hash":"SHA-256"}` và payload là bản mã RSA-OAEP (k byte).

- **Header chỉ được chấp nhận ở dạng chuẩn tắc:** sau khi phân tích JSON, header được dựng lại theo đúng thứ tự khóa, không khoảng trắng, không trường lạ rồi so từng byte với bản đọc được; khoảng trắng, đổi thứ tự, trường thêm hay khóa trùng đều bị từ chối.
- **AAD** của GCM = `"RSAE-AAD-v1|" + mode + "|" + rsa_modulus + "|" + hash + "|" + wrapped_key_b64 + "|" + iv_b64` (không gồm tag). Nhờ đó sửa bất kỳ trường nào của header đều làm xác thực thất bại.
- Mỗi lần mã hóa sinh khóa AES và IV mới, nên không có nguy cơ dùng lại IV của GCM với cùng khóa.

## 2.5. Giải mã fail closed

Thứ tự bắt buộc: (1) phân tích container nghiêm ngặt (magic, version, `header_len ≤ 65536`, JSON chuẩn tắc, mode, hash, `rsa_modulus` khớp khóa riêng, độ dài `wrapped_key`/`iv`/`tag`); (2) RSA-OAEP giải bọc khóa AES với label do người dùng truyền; (3) AES-256-GCM giải mã có xác thực **vào bộ đệm trong RAM**; (4) chỉ khi thẻ hợp lệ mới ghi tệp đầu ra (ghi nguyên tử). Tag được Crypto++ so sánh (so sánh constant-time do thư viện đảm nhiệm), mã của bài không tự so sánh tag.

| Mã thoát | Ý nghĩa |
| ---: | :--- |
| 0 | thành công |
| 1 | sai tham số, cỡ khóa không hỗ trợ khi sinh khóa |
| 2 | lỗi I/O, khóa không đọc được hoặc không hỗ trợ, tệp đầu ra đã tồn tại, KAT hỏng |
| 3 | **giải mã thất bại** |
| 4 | KAT thất bại |

Mọi lỗi giải mã (OAEP sai, sai label, sai khóa, tag sai, header sai, base64/hex hỏng, độ dài sai, cắt cụt) in **đúng một thông báo** `Error: decryption failed.` với mã thoát 3, không nêu nguyên nhân; không bao giờ in khóa, bản rõ, label hay IV.

## 2.6. Giao diện dòng lệnh

```
rsatool keygen  --bits 3072 --pub pub.pem --priv priv.pem
rsatool encrypt --in msg.bin --pub pub.pem --out ct.bin [--label TEXT] [--encode raw|hex|base64] [--force-hybrid]
rsatool decrypt --in ct.bin --priv priv.pem --out msg.bin [--label TEXT] [--encode raw|hex|base64]
rsatool --kat kat/vectors.json
```

Bản rõ ngắn dùng RSA-OAEP trực tiếp; vượt giới hạn thì tự chuyển sang hybrid (`--force-hybrid` ép dùng envelope). Không có `--out` thì ciphertext in ra màn hình dạng hex.

# 3. Cơ sở lý thuyết (Required Discussion)

## 3.1. Vì sao RSA-OAEP an toàn (IND-CCA2)

RSA "sách giáo khoa" (`c = m^e mod n`) là tất định và có tính nhân (`(m1·m2)^e = m1^e · m2^e`), nên không đạt ngay cả bảo mật ngữ nghĩa và dễ bị sửa bản mã. **OAEP** (Bellare và Rogaway, 1994; RFC 8017 mục 7.1) mã hóa thông điệp trước khi lũy thừa:

```
DB = lHash || PS (các byte 0) || 0x01 || M          lHash = SHA-256(label)
maskedDB   = DB XOR MGF1(seed, k − hLen − 1)       seed ngẫu nhiên hLen byte
maskedSeed = seed XOR MGF1(maskedDB, hLen)
EM = 0x00 || maskedSeed || maskedDB                 c = EM^e mod n
```

- **Ngẫu nhiên hóa:** `seed` ngẫu nhiên làm cùng một bản rõ cho bản mã khác nhau (đã kiểm: hai lần mã hóa khác nhau).
- **Tính nhận biết bản rõ (plaintext awareness):** muốn tạo bản mã hợp lệ, kẻ tấn công phải biết `seed` và `M`; `lHash` và phần đệm 0 buộc mọi thay đổi bản mã (kể cả đổi label) làm giải mã thất bại.
- **IND-CCA2:** RSA-OAEP đạt IND-CCA2 trong **mô hình random oracle** với giả thiết bài toán RSA khó (chứng minh gốc của Bellare và Rogaway có lỗ hổng do Shoup chỉ ra năm 2001; Fujisaki, Okamoto, Pointcheval và Stern đã chứng minh lại cho RSA-OAEP năm 2001). Đây là kết quả trong mô hình lý tưởng, không phải bảo đảm vô điều kiện.
- Điều kiện thực hiện: **mọi lỗi giải mã phải không phân biệt được** (cả thông điệp lẫn thời gian). Tấn công Manger (2001) khai thác việc cài đặt phân biệt lỗi "byte đầu khác 0x00" với lỗi khác. Bài này dùng một thông báo duy nhất; tính constant-time của bước giải mã OAEP là của Crypto++ (không tự cài lại, xem mục 6).

## 3.2. Vì sao PKCS#1 v1.5 không an toàn

Padding PKCS#1 v1.5 cho mã hóa (`0x00 || 0x02 || PS ngẫu nhiên khác 0 || 0x00 || M`) có cấu trúc đủ đơn giản để một máy chủ nói "padding hợp lệ / không hợp lệ" trở thành **oracle**. **Bleichenbacher (CRYPTO 1998)** dùng oracle này để giải mã một bản mã bất kỳ bằng khoảng một triệu truy vấn thích ứng (adaptive chosen-ciphertext) theo bài báo gốc; biến thể **ROBOT (2018)** vẫn khai thác được nhiều máy chủ TLS hai thập kỷ sau. Khác với OAEP, v1.5 không có chứng minh IND-CCA2. RFC 8017 khuyến nghị RSAES-OAEP cho ứng dụng mới và chỉ giữ RSAES-PKCS1-v1_5 để tương thích. Vì vậy `rsatool` chỉ hỗ trợ OAEP.

## 3.3. Giới hạn kích thước bản rõ

Với modulus k byte: `EM` có k byte = `0x00` (1) + `maskedSeed` (hLen) + `maskedDB` (k − hLen − 1). `DB = lHash (hLen) || PS (≥ 0) || 0x01 (1) || M`, nên `|DB| = k − hLen − 1 ≥ hLen + 1 + mLen`, suy ra

```
mLen ≤ k − 2·hLen − 2
```

Với hLen = 32 (SHA-256): RSA-3072 (k = 384) cho **318 byte**; RSA-4096 (k = 512) cho **446 byte**. Đã kiểm bằng test biên: 318 và 446 byte dùng chế độ direct, 319 và 447 byte tự chuyển sang hybrid.

## 3.4. Vì sao RSA không thể mã hóa file lớn trực tiếp

1. **Giới hạn kích thước:** mỗi khối tối đa 318 byte (3072 bit), phải chia khối.
2. **Chậm:** theo số đo của bài, một lần giải mã RSA-3072 mất 4.193 ms (Windows) và 3.622 ms (Ubuntu) cho 318 byte, tương đương chỉ khoảng **76 KB/s** (88 KB/s trên Ubuntu), so với hàng GB/s của AES-GCM (mục 5.4, suy ra từ số đo từng khối, chưa đo RSA chia khối từ đầu đến cuối).
3. **Phình dữ liệu:** mỗi 318 byte rõ thành 384 byte mã (khoảng 21%).
4. **Không có tính toàn vẹn cho cả tệp:** các khối RSA độc lập, kẻ tấn công có thể xóa, đảo thứ tự hoặc lặp khối mà không bị phát hiện; cần thêm cơ chế xác thực toàn tệp (chính là GCM trong mã hóa lai).
5. **Bộ nhớ và độ phức tạp thiết kế:** để an toàn, phải tự thiết kế thêm chế độ nối khối và xác thực, trong khi AES-GCM đã làm đúng việc đó.

Hybrid giải quyết cả năm điểm: RSA chỉ bọc **một khóa AES 32 byte** (một phép RSA bất kể độ dài dữ liệu), dữ liệu đi qua AES-GCM.

# 4. Phương pháp kiểm thử (Test Methodology)

## 4.1. Known Answer Tests từ nguồn chính thức

`rsatool --kat kat/vectors.json` đọc vector từ JSON, in PASS/FAIL từng ca và tổng kết. Tệp có **124 vector**, sinh bằng `scripts/make_kat.py` từ hai nguồn chính thức, chép nguyên văn (script kiểm SHA-256 của tệp nguồn):

| Nhóm | Số vector | Nguồn |
| :--- | ---: | :--- |
| RSAES-OAEP, RSA-3072, SHA-256, MGF1-SHA-256, có label | 37 (18 phải giải mã đúng, 19 phải bị từ chối) | Project Wycheproof (C2SP): [rsa_oaep_3072_sha256_mgf1sha256_test.json](https://github.com/C2SP/wycheproof/blob/main/testvectors_v1/rsa_oaep_3072_sha256_mgf1sha256_test.json) |
| AES-256-GCM, IV 96 bit, tag 128 bit: mã hóa và giải mã | 75 | NIST CAVP (CAVS 14.0), `gcmEncryptExtIV256.rsp` trong [gcmtestvectors.zip](https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Algorithm-Validation-Program/documents/mac/gcmtestvectors.zip); trang: [CAVP, Block Cipher Modes](https://csrc.nist.gov/projects/cryptographic-algorithm-validation-program/cavp-testing-block-cipher-modes) |
| AES-256-GCM: xác thực phải thất bại | 12 | NIST CAVP, `gcmDecrypt256.rsp` (các ca `FAIL`) |

| Kiểm chứng | Windows MinGW | Windows MSVC | Ubuntu 24.04 |
| :--- | :--- | :--- | :--- |
| KAT (`--kat`) | Summary: 124 tests, 124 passed, 0 failed | Summary: 124 tests, 124 passed, 0 failed | Summary: 124 tests, 124 passed, 0 failed |
| `ctest` (3 test) | 100% tests passed out of 3 | 100% tests passed, 0 tests failed out of 3 | 100% tests passed, 0 tests failed out of 3 |
| Unit test Catch2 | All tests passed (855 assertions in 45 test cases) | All tests passed (855 assertions in 45 test cases) | All tests passed (855 assertions in 45 test cases) |

RSA-OAEP là mã hóa ngẫu nhiên nên KAT chỉ kiểm **giải mã** bản mã có sẵn (đúng cách Wycheproof định nghĩa); chiều **mã hóa** được kiểm bằng test round-trip và bằng tương tác với OpenSSL (mục 4.4). Chưa có vector chính thức cho định dạng envelope riêng của bài.

## 4.2. Unit test

Catch2 chạy qua `ctest` (3 test): unit test (45 test case, 855 assertion: giới hạn `mLen`, round-trip direct/hybrid ở các biên, RSA-3072 và RSA-4096, label, tính ngẫu nhiên hóa, định dạng PEM/DER/metadata, bộ đọc PEM chặt, hex/base64), KAT, và test CLI end-to-end (38 kiểm tra về mã thoát và việc không để lại tệp đầu ra).

## 4.3. Test âm tính (Negative Tests)

Các phép thử chạy bằng một khối lệnh duy nhất (`docs/TEST_EVIDENCE.md`); log ở `output/<os>/negative_*.txt`. Phân bố mã thoát: Windows MinGW: 29 lần exit 0, 3 lần exit 1, 10 lần exit 2, 20 lần exit 3; Windows MSVC: 29 lần exit 0, 3 lần exit 1, 10 lần exit 2, 20 lần exit 3; Ubuntu: 29 lần exit 0, 3 lần exit 1, 10 lần exit 2, 20 lần exit 3. Số dòng `WRITTEN (BAD)` (tệp đầu ra bị ghi khi lỗi): 0, 0 và 0. Kết quả **giống hệt** trên cả ba môi trường.

| Nhóm | Phép thử | Kết quả mong đợi | Mã thoát |
| :--- | :--- | :--- | :---: |
| A | Mã hóa rồi giải mã direct và hybrid, có và không có label | khớp từng byte | 0 |
| B | **Sửa bản mã RSA** (direct) | giải mã thất bại, không có tệp ra | 3 |
| C | **Sửa bản mã AES-GCM** (hybrid) | lỗi xác thực, không có tệp ra | 3 |
| D | **Sửa tag** | lỗi xác thực | 3 |
| E | **Sai khóa riêng** (direct và hybrid) | thất bại | 3 |
| F | **Sai label** hoặc thiếu label (direct và hybrid) | thất bại | 3 |
| G | **Sửa header**: `iv`, `wrapped_key`, `rsa_modulus`, thêm trường lạ, `header_len` sai, magic sai, cắt cụt | thất bại | 3 |
| H | Mã hóa tệp ciphertext sai: hex đọc như raw, hex/base64 hỏng | thất bại | 3 |
| H | Tên mã hóa không tồn tại (`--encode base32`) | từ chối | 1 |
| I | `keygen --bits 2048` hoặc `3071` | từ chối | 1 |
| I | Mã hóa/giải mã bằng khóa 2048 bit (do OpenSSL sinh) | từ chối | 2 |
| J | Thiếu tệp vào/khóa; khóa riêng dùng như khóa công khai và ngược lại; PEM hỏng; tệp đầu ra đã tồn tại; `keygen` đè khóa cũ | từ chối | 2 |
| L | Tệp KAT hỏng | không báo thành công | 2 |

Mọi thông báo lỗi giải mã đều là `Error: decryption failed.` (không có từ "padding", "tag", "label"...; unit test kiểm tra việc này). Thứ tự ưu tiên của lỗi: lỗi khóa không đọc được báo mã 2; lỗi giải mã báo mã 3.

## 4.4. Tương tác với OpenSSL

OpenSSL (dòng lệnh, `pkeyutl -decrypt` với OAEP SHA-256, MGF1 SHA-256 và cùng label) giải mã đúng bản mã RSA-OAEP do Crypto++ tạo ra (mục K của log âm tính), và đọc được khóa PEM do `rsatool` sinh. Đây là kiểm tra chéo độc lập cho chiều mã hóa.

# 5. Đánh giá hiệu năng (Performance)

## 5.1. Môi trường đo

| Hạng mục | Windows | Ubuntu |
| :--- | :--- | :--- |
| CPU | Intel Core i7-11800H @ 2,3 GHz, 8 nhân / 16 luồng, AES-NI, CLMUL | giống (cùng máy, cài song song) |
| RAM | 31,7 GB | 31 GiB |
| Hệ điều hành | Windows 11 Pro, build 26200 | Ubuntu 24.04.2 LTS, kernel 7.0.0-43-generic |
| Compiler | MinGW-w64 GCC 16.2.0, `Release` (`-O3 -DNDEBUG`), binary tĩnh | GCC 13.3.0, `Release` (`-O3 -DNDEBUG`) |
| Crypto++ | 8.9.0 (MSYS2) | 8.9.0-1.1build1 (gói apt) |
| Chế độ nguồn | High performance, cắm điện | cắm điện; governor ghi trong `env_ubuntu.txt` là **powersave** (không phải `performance` như protocol) |

Về governor: tệp môi trường của Ubuntu ghi `powersave`, trong khi protocol yêu cầu `performance`. Không có phép đo cô lập tác động của nó. Các phép RSA (bị giới hạn bởi tính toán CPU) có thời gian gần như bằng Windows ở cả hai cỡ khóa (mục 5.6), nên CPU rõ ràng không bị kìm ở mức thấp, nhưng điều này không thay thế cho việc đặt `performance`; coi đây là hạn chế của số liệu Ubuntu.

## 5.2. Phương pháp

Theo protocol chung của đề: **warm-up 1,5 s** cho mỗi ca (kết quả bỏ đi); mỗi lần đo là một **khối thao tác** dài khoảng 1,5 s (tối đa 1000 thao tác; số thực tế ở cột "Ops/khối"); **N = 30 lần lặp**; báo mean, median, SD và khoảng tin cậy 95% (phân phối Student t). Thông lượng tính bằng MB/s (1 MB = 10^6 byte). Khóa, IV và dữ liệu lấy từ CSPRNG. Lệnh đã dùng (cùng tham số trên cả hai hệ điều hành):

```
rsabench --suites keygen --reps 30 --out bench_keygen.csv --env-out bench_env.txt
rsabench --suites rsa    --reps 30 --out bench_rsa.csv
rsabench --suites aesgcm --reps 30 --out bench_aesgcm.csv
rsabench --suites hybrid --reps 30 --out bench_hybrid.csv
```

**Độ lệch protocol (ghi rõ):** sinh khóa RSA có thời gian ngẫu nhiên nên đo **một khóa mỗi lần lặp** (30 khóa mỗi cỡ) thay vì khối 1000 thao tác; các thao tác chậm (giải mã RSA, hybrid) dùng khối khoảng 1,5 s, ít hơn 1000 thao tác. Mỗi lần đo ghi thời gian trung bình trên một thao tác của khối. Phép đo hybrid gồm cả sinh khóa AES, mã hóa GCM và bọc/giải bọc bằng RSA-OAEP. Các phép đo AES-GCM dùng IV cố định giữa các lần lặp chỉ vì đây là đo nguyên thủy, không phải giao thức.

## 5.3. Kết quả

### Windows (MinGW GCC 16.2)

| Suite | Thao tác | RSA | Payload | Ops/khối | N | Mean | Median | SD | ±CI95 | Thông lượng (MB/s) |
| :--- | :--- | ---: | :---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| keygen | keygen | 3072 | - | 1 | 30 | 157.493 ms | 126.837 ms | 107.428 ms | ±40.114 ms |  |
| keygen | keygen | 4096 | - | 1 | 30 | 324.674 ms | 265.618 ms | 219.007 ms | ±81.778 ms |  |
| rsa | encrypt | 3072 | - | 1000 | 30 | 70.67 µs | 70.67 µs | 1.10 µs | ±411.9 ns |  |
| rsa | decrypt | 3072 | - | 408 | 30 | 4.193 ms | 4.249 ms | 256.56 µs | ±95.80 µs |  |
| rsa | encrypt | 4096 | - | 1000 | 30 | 83.17 µs | 82.70 µs | 1.48 µs | ±551.7 ns |  |
| rsa | decrypt | 4096 | - | 264 | 30 | 5.570 ms | 5.547 ms | 145.74 µs | ±54.42 µs |  |
| aesgcm | encrypt | - | 1 KiB | 1000 | 30 | 929.6 ns | 919.9 ns | 37.0 ns | ±13.8 ns | 1,103.1 ± 14.7 |
| aesgcm | decrypt | - | 1 KiB | 1000 | 30 | 978.4 ns | 979.4 ns | 16.4 ns | ±6.1 ns | 1,046.9 ± 6.3 |
| aesgcm | encrypt | - | 1 MiB | 1000 | 30 | 210.99 µs | 211.05 µs | 1.01 µs | ±377.3 ns | 4,970.0 ± 9.0 |
| aesgcm | decrypt | - | 1 MiB | 1000 | 30 | 242.52 µs | 264.09 µs | 33.97 µs | ±12.69 µs | 4,408.7 ± 235.0 |
| aesgcm | encrypt | - | 100 MiB | 44 | 30 | 31.828 ms | 31.941 ms | 516.85 µs | ±192.99 µs | 3,295.4 ± 20.0 |
| aesgcm | decrypt | - | 100 MiB | 46 | 30 | 32.367 ms | 32.269 ms | 1.214 ms | ±453.21 µs | 3,243.7 ± 42.3 |
| hybrid | encrypt | 3072 | 1 KiB | 1000 | 30 | 75.51 µs | 75.44 µs | 1.93 µs | ±720.0 ns | 13.6 ± 0.1 |
| hybrid | decrypt | 3072 | 1 KiB | 396 | 30 | 4.408 ms | 4.356 ms | 172.75 µs | ±64.51 µs | 0.2 ± 0.0 |
| hybrid | encrypt | 3072 | 1 MiB | 1000 | 30 | 888.57 µs | 881.17 µs | 22.58 µs | ±8.43 µs | 1,180.8 ± 10.8 |
| hybrid | decrypt | 3072 | 1 MiB | 253 | 30 | 5.252 ms | 5.235 ms | 117.23 µs | ±43.77 µs | 199.8 ± 1.6 |
| hybrid | encrypt | 3072 | 100 MiB | 18 | 30 | 83.218 ms | 83.019 ms | 1.734 ms | ±647.45 µs | 1,260.6 ± 9.6 |
| hybrid | decrypt | 3072 | 100 MiB | 17 | 30 | 88.489 ms | 87.853 ms | 2.191 ms | ±818.15 µs | 1,185.7 ± 10.7 |
| hybrid | encrypt | 4096 | 1 KiB | 1000 | 30 | 89.52 µs | 88.37 µs | 2.96 µs | ±1.11 µs | 11.4 ± 0.1 |
| hybrid | decrypt | 4096 | 1 KiB | 266 | 30 | 5.816 ms | 5.751 ms | 287.87 µs | ±107.49 µs | 0.2 ± 0.0 |
| hybrid | encrypt | 4096 | 1 MiB | 1000 | 30 | 897.31 µs | 887.29 µs | 27.54 µs | ±10.29 µs | 1,169.6 ± 13.0 |
| hybrid | decrypt | 4096 | 1 MiB | 215 | 30 | 7.423 ms | 6.874 ms | 1.226 ms | ±457.86 µs | 144.4 ± 7.4 |
| hybrid | encrypt | 4096 | 100 MiB | 19 | 30 | 79.301 ms | 79.147 ms | 2.738 ms | ±1.022 ms | 1,323.7 ± 16.4 |
| hybrid | decrypt | 4096 | 100 MiB | 17 | 30 | 84.286 ms | 84.132 ms | 1.724 ms | ±643.72 µs | 1,244.6 ± 9.4 |

### Ubuntu 24.04 (GCC 13.3)

| Suite | Thao tác | RSA | Payload | Ops/khối | N | Mean | Median | SD | ±CI95 | Thông lượng (MB/s) |
| :--- | :--- | ---: | :---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| keygen | keygen | 3072 | - | 1 | 30 | 215.550 ms | 205.014 ms | 113.197 ms | ±42.269 ms |  |
| keygen | keygen | 4096 | - | 1 | 30 | 358.790 ms | 316.337 ms | 278.349 ms | ±103.937 ms |  |
| rsa | encrypt | 3072 | - | 1000 | 30 | 70.79 µs | 70.74 µs | 141.9 ns | ±53.0 ns |  |
| rsa | decrypt | 3072 | - | 413 | 30 | 3.622 ms | 3.623 ms | 1.70 µs | ±633.2 ns |  |
| rsa | encrypt | 4096 | - | 1000 | 30 | 73.92 µs | 73.91 µs | 95.0 ns | ±35.5 ns |  |
| rsa | decrypt | 4096 | - | 314 | 30 | 4.791 ms | 4.794 ms | 9.95 µs | ±3.71 µs |  |
| aesgcm | encrypt | - | 1 KiB | 1000 | 30 | 1.12 µs | 1.13 µs | 21.9 ns | ±8.2 ns | 911.8 ± 6.7 |
| aesgcm | decrypt | - | 1 KiB | 1000 | 30 | 1.10 µs | 1.10 µs | 17.7 ns | ±6.6 ns | 928.1 ± 5.4 |
| aesgcm | encrypt | - | 1 MiB | 1000 | 30 | 493.08 µs | 493.02 µs | 616.6 ns | ±230.2 ns | 2,126.6 ± 1.0 |
| aesgcm | decrypt | - | 1 MiB | 1000 | 30 | 471.30 µs | 471.44 µs | 748.9 ns | ±279.6 ns | 2,224.9 ± 1.3 |
| aesgcm | encrypt | - | 100 MiB | 26 | 30 | 55.019 ms | 54.937 ms | 432.12 µs | ±161.36 µs | 1,905.9 ± 5.4 |
| aesgcm | decrypt | - | 100 MiB | 27 | 30 | 54.890 ms | 54.889 ms | 18.45 µs | ±6.89 µs | 1,910.3 ± 0.2 |
| hybrid | encrypt | 3072 | 1 KiB | 1000 | 30 | 76.03 µs | 76.01 µs | 89.3 ns | ±33.3 ns | 13.5 ± 0.0 |
| hybrid | decrypt | 3072 | 1 KiB | 405 | 30 | 3.691 ms | 3.691 ms | 2.43 µs | ±907.0 ns | 0.3 ± 0.0 |
| hybrid | encrypt | 3072 | 1 MiB | 1000 | 30 | 1.034 ms | 1.034 ms | 1.91 µs | ±714.9 ns | 1,014.5 ± 0.7 |
| hybrid | decrypt | 3072 | 1 MiB | 318 | 30 | 4.708 ms | 4.707 ms | 4.26 µs | ±1.59 µs | 222.7 ± 0.1 |
| hybrid | encrypt | 3072 | 100 MiB | 12 | 30 | 115.925 ms | 115.909 ms | 113.64 µs | ±42.43 µs | 904.5 ± 0.3 |
| hybrid | decrypt | 3072 | 100 MiB | 12 | 30 | 119.621 ms | 119.584 ms | 118.99 µs | ±44.43 µs | 876.6 ± 0.3 |
| hybrid | encrypt | 4096 | 1 KiB | 1000 | 30 | 79.66 µs | 79.68 µs | 90.5 ns | ±33.8 ns | 12.9 ± 0.0 |
| hybrid | decrypt | 4096 | 1 KiB | 307 | 30 | 4.862 ms | 4.861 ms | 4.05 µs | ±1.51 µs | 0.2 ± 0.0 |
| hybrid | encrypt | 4096 | 1 MiB | 1000 | 30 | 615.03 µs | 615.24 µs | 863.5 ns | ±322.5 ns | 1,704.9 ± 0.9 |
| hybrid | decrypt | 4096 | 1 MiB | 276 | 30 | 5.414 ms | 5.414 ms | 3.96 µs | ±1.48 µs | 193.7 ± 0.1 |
| hybrid | encrypt | 4096 | 100 MiB | 12 | 30 | 115.724 ms | 115.712 ms | 160.10 µs | ±59.78 µs | 906.1 ± 0.5 |
| hybrid | decrypt | 4096 | 100 MiB | 12 | 30 | 120.569 ms | 120.582 ms | 92.44 µs | ±34.52 µs | 869.7 ± 0.2 |

![Thời gian sinh khóa RSA](../output/compare/keygen_compare.png)

*Hình 1: Thời gian sinh khóa RSA (30 khóa mỗi cỡ; thanh sai số là CI95).*

![Thời gian mã hóa và giải mã RSA-OAEP](../output/compare/rsa_compare.png)

*Hình 2: Thời gian RSA-OAEP mỗi thao tác (trục log, thanh sai số là CI95).*

![Thông lượng AES-256-GCM](../output/compare/aesgcm_compare.png)

*Hình 3: Thông lượng AES-256-GCM theo kích thước payload.*

![Thời gian mã hóa lai](../output/compare/hybrid_compare.png)

*Hình 4: Thời gian mã hóa và giải mã lai theo kích thước payload (log-log), RSA-3072 và RSA-4096.*

## 5.4. Phân tích bắt buộc

**Giải mã RSA so với mã hóa RSA.** Giải mã chậm hơn mã hóa khoảng **59 lần** ở RSA-3072 và **67 lần** ở RSA-4096 trên Windows (51 và 65 lần trên Ubuntu). Lý do (từ cấu trúc thuật toán): mã hóa lũy thừa với `e = 65537 = 2^16 + 1`, chỉ cần 16 phép bình phương và 1 phép nhân modulo; giải mã lũy thừa với số mũ riêng `d` cỡ bằng modulus nên cần hàng nghìn phép nhân modulo trên số 3072 bit; Crypto++ giảm chi phí này bằng định lý số dư Trung Hoa (CRT) với hai nửa 1536 bit, nhưng vẫn rất chậm hơn mã hóa. Đây là lập luận lý thuyết; số lần nhân không được đo trực tiếp.

**Cỡ khóa so với hiệu năng (3072 so với 4096).**

| Phép đo | Windows: 4096 / 3072 | Ubuntu: 4096 / 3072 |
| :--- | :---: | :---: |
| Sinh khóa (trung bình) | 2.06 | 1.66 |
| Mã hóa OAEP | 1.18 | 1.04 |
| Giải mã OAEP | 1.33 | 1.32 |

Tăng từ 3072 lên 4096 bit làm giải mã chậm hơn khoảng 1.33 lần (Windows) và 1.32 lần (Ubuntu), và sinh khóa chậm hơn khoảng 2.1 lần (sinh khóa có độ phân tán rất lớn nên tỉ lệ này không chắc chắn: SD gần bằng một nửa đến bằng trung bình). Đổi lại 4096 bit có biên an toàn cao hơn; theo NIST SP 800-57, RSA-3072 tương đương khoảng 128 bit bảo mật. Với dữ liệu lớn, chênh lệch giữa hai cỡ khóa biến mất vì chi phí RSA chỉ xảy ra một lần mỗi envelope (mục 5.5).

**Ưu thế của hybrid.** Nếu chia dữ liệu thành khối 318 byte và dùng RSA-3072 thuần, tốc độ giải mã chỉ khoảng 76 KB/s. Với 1 MiB (3298 khối) mất ước tính khoảng **13.8 s** (Windows) hoặc 11.9 s (Ubuntu), trong khi giải mã lai 1 MiB đo được là 5.252 ms và 4.708 ms: nhanh hơn khoảng **2,633 lần** (Windows) và 2,538 lần (Ubuntu). Các con số RSA thuần là **ngoại suy** từ thời gian một phép giải mã đã đo, không phải kết quả đo RSA chia khối từ đầu đến cuối.

**Vì sao mã hóa đối xứng áp đảo thông lượng.** Tính theo mỗi byte, giải mã RSA-3072 tốn khoảng 13,184 ns/byte, còn AES-256-GCM ở 100 MiB tốn khoảng 0.30 ns/byte: khoảng **43,436 lần** (Windows) và 21,710 lần (Ubuntu). AES chỉ gồm các phép trên khối 128 bit (S-box, XOR) và được CPU tăng tốc bằng lệnh AES-NI (đã xác nhận có trên máy đo), GCM dùng nhân không mang CLMUL; RSA phải thực hiện hàng nghìn phép nhân số lớn. Phần đóng góp của lệnh phần cứng được suy từ việc CPU có AES-NI/CLMUL và từ thông lượng cỡ GB/s, không đo riêng.

**Bộ nhớ và CPU.** Theo mã nguồn, hybrid giữ bản rõ, bản mã và các bản sao trung gian trong RAM nên đỉnh bộ nhớ khoảng 3 đến 4 lần kích thước dữ liệu (ước lượng từ mã, chưa đo); với 100 MiB là vài trăm MiB. Giới hạn đầu vào là 1 GiB, chưa có chế độ streaming. Về CPU: ở dữ liệu nhỏ (1 KiB) chi phí do RSA chi phối (giải mã lai 1 KiB mất 4.408 ms, gần bằng một phép giải mã RSA); ở 100 MiB chi phí do AES-GCM chi phối. Ở 1 MiB, phần RSA chiếm khoảng 8% thời gian mã hóa lai trên Windows.

## 5.5. Hiệu năng của chế độ lai

Mã hóa lai ở 1 KiB mất 75.51 µs (RSA-3072, Windows), gần như chỉ là một phép mã hóa RSA (70.67 µs); giải mã lai 1 KiB mất 4.408 ms, gần như chỉ là một phép giải mã RSA. Khi payload tăng lên 100 MiB, thời gian tăng gần tuyến tính theo kích thước và chi phí RSA trở nên không đáng kể; thông lượng mã hóa lai đạt 1,261 MB/s (Windows) và 905 MB/s (Ubuntu). So với AES-GCM thuần ở cùng kích thước, hybrid đạt khoảng 38% (Windows) và 47% (Ubuntu) thông lượng; phần chênh lệch là chi phí sao chép/cấp phát bộ đệm và dựng container (suy luận từ cấu trúc mã, chưa được đo riêng).

Ở 100 MiB, hybrid dùng RSA-4096 không chậm hơn RSA-3072 đáng kể; ở 1 MiB và trên Ubuntu, một số cặp còn cho kết quả ngược kỳ vọng (ví dụ mã hóa lai 1 MiB RSA-4096 nhanh hơn RSA-3072). Phần RSA chỉ chiếm vài phần trăm thời gian ở các cỡ này nên chênh lệch nằm trong biên nhiễu đo (thứ tự chạy, trạng thái bộ nhớ đệm và xung nhịp); **không nên diễn giải như tác động của cỡ khóa.**

## 5.6. So sánh Windows và Ubuntu

| Suite | Thao tác | RSA | Payload | Windows | Ubuntu | Ubuntu / Windows | CI95 không chồng nhau |
| :--- | :--- | ---: | :---: | ---: | ---: | :---: | :---: |
| keygen | keygen | 3072 | - | 157.493 ms ± 40.114 ms | 215.550 ms ± 42.269 ms | 1.37 | Không |
| keygen | keygen | 4096 | - | 324.674 ms ± 81.778 ms | 358.790 ms ± 103.937 ms | 1.11 | Không |
| rsa | encrypt | 3072 | - | 70.67 µs ± 411.9 ns | 70.79 µs ± 53.0 ns | 1.00 | Không |
| rsa | decrypt | 3072 | - | 4.193 ms ± 95.80 µs | 3.622 ms ± 633.2 ns | 0.86 | Có |
| rsa | encrypt | 4096 | - | 83.17 µs ± 551.7 ns | 73.92 µs ± 35.5 ns | 0.89 | Có |
| rsa | decrypt | 4096 | - | 5.570 ms ± 54.42 µs | 4.791 ms ± 3.71 µs | 0.86 | Có |
| aesgcm | encrypt | - | 1 KiB | 929.6 ns ± 13.8 ns | 1.12 µs ± 8.2 ns | 1.21 | Có |
| aesgcm | decrypt | - | 1 KiB | 978.4 ns ± 6.1 ns | 1.10 µs ± 6.6 ns | 1.13 | Có |
| aesgcm | encrypt | - | 1 MiB | 210.99 µs ± 377.3 ns | 493.08 µs ± 230.2 ns | 2.34 | Có |
| aesgcm | decrypt | - | 1 MiB | 242.52 µs ± 12.69 µs | 471.30 µs ± 279.6 ns | 1.94 | Có |
| aesgcm | encrypt | - | 100 MiB | 31.828 ms ± 192.99 µs | 55.019 ms ± 161.36 µs | 1.73 | Có |
| aesgcm | decrypt | - | 100 MiB | 32.367 ms ± 453.21 µs | 54.890 ms ± 6.89 µs | 1.70 | Có |
| hybrid | encrypt | 3072 | 1 KiB | 75.51 µs ± 720.0 ns | 76.03 µs ± 33.3 ns | 1.01 | Không |
| hybrid | decrypt | 3072 | 1 KiB | 4.408 ms ± 64.51 µs | 3.691 ms ± 907.0 ns | 0.84 | Có |
| hybrid | encrypt | 3072 | 1 MiB | 888.57 µs ± 8.43 µs | 1.034 ms ± 714.9 ns | 1.16 | Có |
| hybrid | decrypt | 3072 | 1 MiB | 5.252 ms ± 43.77 µs | 4.708 ms ± 1.59 µs | 0.90 | Có |
| hybrid | encrypt | 3072 | 100 MiB | 83.218 ms ± 647.45 µs | 115.925 ms ± 42.43 µs | 1.39 | Có |
| hybrid | decrypt | 3072 | 100 MiB | 88.489 ms ± 818.15 µs | 119.621 ms ± 44.43 µs | 1.35 | Có |
| hybrid | encrypt | 4096 | 1 KiB | 89.52 µs ± 1.11 µs | 79.66 µs ± 33.8 ns | 0.89 | Có |
| hybrid | decrypt | 4096 | 1 KiB | 5.816 ms ± 107.49 µs | 4.862 ms ± 1.51 µs | 0.84 | Có |
| hybrid | encrypt | 4096 | 1 MiB | 897.31 µs ± 10.29 µs | 615.03 µs ± 322.5 ns | 0.69 | Có |
| hybrid | decrypt | 4096 | 1 MiB | 7.423 ms ± 457.86 µs | 5.414 ms ± 1.48 µs | 0.73 | Có |
| hybrid | encrypt | 4096 | 100 MiB | 79.301 ms ± 1.022 ms | 115.724 ms ± 59.78 µs | 1.46 | Có |
| hybrid | decrypt | 4096 | 100 MiB | 84.286 ms ± 643.72 µs | 120.569 ms ± 34.52 µs | 1.43 | Có |

- **RSA-OAEP:** thời gian gần nhau (chênh lệch trong khoảng 0 đến 14%); giải mã RSA nhanh hơn trên Ubuntu (0.86 lần thời gian Windows ở 3072 bit, có ý nghĩa thống kê).
- **AES-256-GCM:** Ubuntu **chậm hơn khoảng 2 lần** ở 1 MiB và 100 MiB (thông lượng Ubuntu bằng 43% và 58% của Windows khi mã hóa). Cùng máy, cùng CPU có AES-NI và CLMUL. Hiện tượng tương tự đã thấy ở Lab 2 với Crypto++ (Crypto++ trên Ubuntu chậm hơn đáng kể so với Windows trong khi OpenSSL thì không). Nguyên nhân **chưa được xác định**: khác biệt có thể nằm ở cách dựng thư viện Crypto++ (gói apt dạng thư viện chia sẻ so với bản MSYS2 liên kết tĩnh), phiên bản compiler hoặc governor; không có phép đo cô lập từng yếu tố.
- **Sinh khóa:** khác biệt không có ý nghĩa thống kê do độ phân tán lớn.
- Hai hệ điều hành dùng **compiler khác phiên bản** (GCC 16.2 và GCC 13.3) và bản dựng Crypto++ khác nhau; các so sánh trên là so sánh hai cấu hình đầy đủ, không phải so sánh hệ điều hành thuần túy.
- Thông lượng AES-GCM ở 100 MiB thấp hơn ở 1 MiB trên cả hai hệ điều hành (Windows 4,970 so với 3,295 MB/s): phù hợp với việc dữ liệu 1 MiB nằm gọn trong bộ nhớ đệm còn 100 MiB thì không (suy luận, không đo số lần trượt cache).

# 6. Thảo luận bảo mật (Security Engineering)

- **Mô hình đe dọa.** Đối thủ có thể: đọc mọi ciphertext và envelope; sửa tùy ý ciphertext, header, tag, wrapped key; gửi nhiều bản mã tùy chọn cho người giải mã và quan sát thông báo lỗi và mã thoát (tấn công kiểu oracle); thử dùng khóa hoặc label sai. Ngoài phạm vi: đối thủ đọc được khóa riêng, malware chạy với quyền người dùng, tấn công kênh kề vật lý, đo thời gian phân giải ở mức vi mô.
- **Tình huống dùng sai và xử lý.** Cỡ khóa yếu (từ chối: chỉ 3072 và 4096); bản rõ quá dài cho OAEP (tự chuyển hybrid, không cắt); label sai hoặc thiếu (thất bại như mọi lỗi giải mã); ghi đè khóa hoặc ciphertext vô ý (cần `--force`); khóa công khai/riêng dùng nhầm chỗ (từ chối); tệp khóa PKCS#1 hoặc PEM lạ (từ chối).
- **Các tấn công đã biết và biện pháp.** *Bleichenbacher/ROBOT* (padding oracle trên PKCS#1 v1.5): chỉ dùng OAEP. *Manger* (oracle trên giải mã OAEP): một thông báo lỗi duy nhất cho mọi lỗi, cùng mã thoát. *Dùng lại IV của GCM* (làm lộ khóa xác thực và cho phép giả mạo): mỗi lần mã hóa sinh khóa AES và IV mới. *Hoán đổi thành phần envelope*: header được ràng buộc vào GCM bằng AAD và phải đúng dạng chuẩn tắc. *Hạ cấp thuật toán*: `mode`, `hash` và `rsa_modulus` phải khớp chính xác giá trị cho phép và khóa riêng. *RSA thuần/malleability*: không có đường dẫn mã hóa không đệm.
- **Mặc định an toàn.** Chỉ RSA ≥ 3072; e = 65537; OAEP SHA-256; AES-256-GCM với IV 96 bit; ngẫu nhiên từ `AutoSeededRandomPool`; không in khóa, bản rõ, label, IV; tệp khóa riêng 0600 trên Linux; ghi tệp nguyên tử; không ghi bản rõ trước khi xác thực xong.
- **Fail closed.** Mọi lỗi phân tích hay giải mã dừng với mã thoát khác 0, một thông báo chung và không để lại tệp đầu ra (mục 4.3).
- **Giới hạn đã biết.** (1) Tính constant-time của giải mã OAEP và so sánh tag là của Crypto++, bài này không tự cài lại và không kiểm chứng riêng. (2) Bộ phân tích header chạy trước RSA nên một envelope sai định dạng bị từ chối nhanh hơn một envelope sai OAEP (khác biệt thời gian; thông báo và mã thoát vẫn giống nhau). (3) Hybrid đọc cả tệp vào RAM (tối đa 1 GiB), chưa có streaming. (4) Label OAEP không lưu trong envelope (là tham số ngữ cảnh) nên người nhận phải biết label. (5) Không có forward secrecy: lộ khóa riêng RSA cho phép giải mã mọi envelope đã ghi lại (hạn chế cố hữu của khóa bọc bằng RSA). (6) Windows Clang chưa được kiểm chứng.

# 7. Thảo luận (Discussion)

## 7.1. Vấn đề đã tìm thấy và đã sửa trong quá trình làm

- **Crypto++ sinh khóa với e = 17 theo mặc định.** `InvertibleRSAFunction` dùng `e = 17`; phải gọi `Initialize(rng, bits, Integer(65537))` để có e = 65537. Đã sửa và có test kiểm `e`.
- **GCM yêu cầu đặt IV cùng lúc với khóa.** `SetKey` thuần báo "this object requires an IV"; phải dùng `SetKeyWithIV`. Phát hiện qua lần chạy KAT đầu tiên.
- **Bộ phân tích header lỏng.** Phiên bản đầu chấp nhận khoảng trắng và trường thừa. Đã chuyển sang so sánh với dạng chuẩn tắc (test negative nhóm G).
- **Dùng nhầm thư viện giữa các toolchain.** Khi build bằng MSVC, `find_package(nlohmann_json)` nhặt nhầm gói MinGW của MSYS2 trên `PATH` làm hỏng header của MSVC. Đã chặn tìm gói theo `PATH` cho MSVC và Linux (cùng cách đã dùng ở các lab trước).
- **KAT từ nguồn tự tạo không đủ tin cậy.** Đã thay bằng vector chính thức (Wycheproof và NIST CAVP) kèm SHA-256 của tệp nguồn.

## 7.2. Bài học

- OAEP chỉ an toàn nếu mọi lỗi giải mã không phân biệt được; thiết kế thông báo lỗi và mã thoát là một phần của thiết kế mật mã.
- Mã hóa lai là bắt buộc trong thực tế: RSA chỉ nên bọc một khóa; dữ liệu đi qua AES-GCM, kèm xác thực.
- Ràng buộc header vào AEAD (AAD) và yêu cầu dạng chuẩn tắc đóng một lớp tấn công hoán đổi thành phần mà không tốn thêm chi phí đáng kể.
- Số đo trên hai hệ điều hành chỉ so sánh được khi ghi rõ cả compiler lẫn bản dựng thư viện; mọi kết luận về nguyên nhân phải nói rõ là đo hay suy luận.

# 8. Kết luận và hướng phát triển

Đã cài RSA-OAEP (SHA-256, MGF1-SHA-256) với RSA-3072 và RSA-4096 và mã hóa lai AES-256-GCM trên Crypto++, đúng với 124 vector chính thức, có đầy đủ test âm tính fail-closed, chạy trên Windows (MinGW, MSVC) và Ubuntu 24.04. Giải mã RSA-3072 chậm hơn mã hóa khoảng 59 lần và chỉ đạt khoảng 76 KB/s nếu dùng thuần túy, còn mã hóa lai đạt khoảng 1,186 MB/s ở 100 MiB (giải mã, Windows).

Hướng phát triển: chế độ streaming cho tệp lớn; kiểm chứng thời gian giải mã (constant-time) bằng công cụ chuyên dụng; chuyển sang trao đổi khóa có forward secrecy (ECDHE) hoặc KEM hậu lượng tử (ML-KEM, Lab 6); đặt governor `performance` cho Ubuntu và đo lại để loại trừ ảnh hưởng của chế độ nguồn.

# Phụ lục

## P1. Lệnh build và kiểm thử (4 lệnh của đề)

```
mkdir build && cd build
cmake ..
cmake --build .
ctest --output-on-failure
```

MSVC cần thêm `-DCMAKE_BUILD_TYPE=Release` và toolchain vcpkg (`-DCMAKE_TOOLCHAIN_FILE=.../vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`).

## P2. Tệp bằng chứng

`output/windows/` và `output/ubuntu/`: `kat_*.txt`, `ctest_*.txt`, `negative_*.txt`, `env_*.txt`, `bench/*.csv`, `analysis/summary.md`; `output/compare/*.png`. Lệnh sinh các tệp này nằm ở `docs/TEST_EVIDENCE.md` và `docs/BENCHMARK_RUNBOOK.md`; nguồn KAT ở `kat/SOURCES.md`.

## P3. Lưu ý về tệp KAT

SHA-256 của `kat/vectors.json` ghi trong `env_windows.txt` khác `env_ubuntu.txt` chỉ vì kiểu xuống dòng (Windows CRLF do git, Ubuntu LF); hash của bản LF trong git trùng với Ubuntu.
