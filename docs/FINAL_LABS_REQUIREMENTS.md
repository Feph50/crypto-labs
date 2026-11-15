# NT219 — Final Labs (Lab 1–6): Tổng hợp Yêu cầu, Quy tắc và Tiêu chí chấm điểm

> **Nguồn:** `Cryptography_Labs.docx` ("Cryptography & Applications — Laboratory Series (Labs 1–6)").
> File này là bản tổng hợp chi tiết **mọi yêu cầu** để đạt điểm của final labs, dùng làm checklist khi triển khai.
> Những chỗ **mình thêm vào** (không có nguyên văn trong đề) đều được đánh dấu *(ghi chú của người tổng hợp)*.
> Khi có khác biệt giữa file này và đề gốc, **đề gốc luôn đúng**.

**Quy ước mức độ:**

| Ký hiệu | Ý nghĩa |
|---|---|
| 🔴 **BẮT BUỘC** | Đề dùng các từ *must / required / mandatory / never*, hoặc là điều kiện để lab được chấm đúng. Thiếu là mất điểm hoặc vi phạm ràng buộc. |
| 🟡 Khuyến nghị | Đề dùng *recommended / encouraged / where applicable / if feasible*. |
| 🟢 Bonus | Phần điểm thưởng (`+N`). Chỉ làm khi phần bắt buộc đã xong. |
| ⚠ Ràng buộc | Điều kiện cấm/giới hạn của lab (thư viện được phép, đạo đức...). Vi phạm có thể bị 0 điểm phần liên quan hoặc phạt học thuật. |

**Mục lục**

1. [Tổng quan và gói nộp](#1-tổng-quan-và-gói-nộp)
2. [Yêu cầu chung cho MỌI lab](#2-yêu-cầu-chung-cho-mọi-lab)
3. [Lab 1 — Symmetric Encryption với Crypto++](#3-lab-1--symmetric-encryption-với-crypto)
4. [Lab 2 — AES-128 C++ thuần (CTR, FIPS-197)](#4-lab-2--aes-128-c-thuần-ctr-fips-197)
5. [Lab 3 — RSA-OAEP và Hybrid Encryption](#5-lab-3--rsa-oaep-và-hybrid-encryption)
6. [Lab 4 — Hashing, PKI và Practical Attacks](#6-lab-4--hashing-pki-và-practical-attacks)
7. [Lab 5 — Chữ ký số cổ điển (ECDSA, RSA-PSS)](#7-lab-5--chữ-ký-số-cổ-điển-ecdsa-rsa-pss)
8. [Lab 6 — Post-Quantum (ML-DSA, ML-KEM)](#8-lab-6--post-quantum-ml-dsa-ml-kem)
9. [Báo cáo: mẫu, protocol đo, cam kết liêm chính, bảng rubric](#9-báo-cáo-mẫu-protocol-đo-cam-kết-liêm-chính-bảng-rubric)
10. [Điểm mơ hồ / mâu thuẫn trong đề cần hỏi giảng viên](#10-điểm-mơ-hồ--mâu-thuẫn-trong-đề-cần-hỏi-giảng-viên)
11. [Checklist tổng trước khi nộp](#11-checklist-tổng-trước-khi-nộp)

---

## 1. Tổng quan và gói nộp

### 1.1. Thông tin chung của môn

| Mục | Giá trị theo đề |
|---|---|
| Trình độ | Upper Undergraduate / Early Graduate |
| Nền tảng | **Windows 10/11** và **Linux (Ubuntu LTS khuyến nghị)** |
| Ngôn ngữ | **Modern C++ (C++17 trở lên)** |
| Thư viện | Crypto++ và/hoặc OpenSSL, **tùy từng lab** (xem bảng 1.2) |
| GUI (tùy chọn) | Python (PySide6/Qt) hoặc C# (WPF/WinUI) |
| Build system | **CMake — bắt buộc** |
| Định dạng nộp | **Một file ZIP/RAR duy nhất** |

### 1.2. Thư viện được phép theo từng lab (⚠ ràng buộc)

| Lab | Chủ đề | Thư viện cho nguyên thủy mật mã | Ghi chú |
|---|---|---|---|
| 1 | AES nhiều mode | **Chỉ Crypto++**. **CẤM** OpenSSL, libsodium và mọi thư viện mật mã khác. STL được dùng. | Phải biên dịch trên Windows và Linux |
| 2 | AES-128 + CTR thuần | **CẤM mọi thư viện mật mã ngoài** (OpenSSL, Crypto++, libsodium...). Chỉ STL. Mọi nguyên thủy phải **tự cài**. AES-NI **chỉ** ở phần bonus. | OpenSSL/Crypto++ chỉ dùng để **đối chiếu kết quả** và **so sánh hiệu năng** |
| 3 | RSA-OAEP + Hybrid | Crypto++ / OpenSSL | |
| 4 | Hash, PKI, tấn công | Crypto++ / OpenSSL (parse X.509); `hashclash`, `hashpump` cho phần demo | |
| 5 | ECDSA, RSA-PSS | Crypto++ **và** OpenSSL | |
| 6 | PQC (ML-DSA, ML-KEM) | Không nêu cụ thể trong đề | *(Ghi chú: cần xác nhận với giảng viên thư viện nào được dùng cho ML-DSA/ML-KEM.)* |

### 1.3. Mục tiêu học tập chung (Labs 1–6)

- Cài đặt và phân tích các nguyên thủy mật mã hiện đại.
- Áp dụng thực hành kỹ nghệ an toàn cho công cụ dòng lệnh (CLI).
- Kiểm chứng tính đúng đắn bằng **vector kiểm thử chính thức của NIST**.
- Phát hiện và ngăn chặn các lỗi dùng sai mật mã phổ biến.
- Đo hiệu năng có cơ sở thống kê.
- Viết báo cáo kỹ thuật chuyên nghiệp.

### 1.4. 🔴 Cấu trúc gói nộp — `MSV.All6labs.rar`

Một file nén duy nhất phải chứa **tất cả**:

- [ ] Source code của **cả 6 lab**
- [ ] Hệ thống build **CMake**
- [ ] `README.md` **top-level**
- [ ] **Báo cáo một file PDF/DOCX duy nhất cho cả 6 lab**
- [ ] **Binary cho Windows và Linux**
- [ ] **Unit tests**
- [ ] **Build scripts**
- [ ] Mọi **resource files** cần thiết (vector KAT, file mẫu...)

### 1.5. 🔴 Deliverables Checklist (mục 6 của đề)

Gói cuối cùng phải có: Source code · CMake files · README · Unit tests · **Windows binary** · **Linux binary** · **Report (PDF/DOCX)** · **Self-grade checklist** · **Academic integrity statement**.

### 1.6. 🔴 Submission Checklist đính kèm mỗi lab (cuối đề)

- [ ] `README.md` có các bước **build và chạy cho cả Windows và Linux**
- [ ] `CMakeLists.txt` **build được mà không cần sửa đường dẫn hay `tasks.json`**
- [ ] Binary cho cả hai hệ điều hành (**hoặc** hướng dẫn build rõ ràng)
- [ ] Source code, header, test, **file KAT**
- [ ] Báo cáo `.pdf/.docx` có bảng/biểu đồ và phân tích
- [ ] **Bảng rubric tự chấm điểm** do nhóm điền
- [ ] *(Tùy chọn)* Dự án GUI dùng core `.dll/.so` của bạn

> *(Ghi chú: dòng "CMakeLists.txt builds without editing paths" là điểm rất dễ mất. Không được hard-code đường dẫn máy cá nhân như `D:/...` hoặc `~/Desktop/...` làm mặc định duy nhất.)*

---

## 2. Yêu cầu chung cho MỌI lab

Áp dụng cho tất cả Lab 1–6.

### 2.1. 🔴 Môi trường và build

- [ ] **Bắt buộc dùng CMake.**
- [ ] **Hỗ trợ out-of-source build** theo đúng lệnh:
  ```bash
  mkdir build && cd build
  cmake ..
  cmake --build .
  ```
- [ ] **Phải biên dịch thành công trên:** Ubuntu LTS **và** Windows (**MSVC** *và* **MinGW64**).
- [ ] Có **build scripts**.

### 2.2. 🔴 `README.md` — bắt buộc có đủ 5 mục

1. **Danh sách phụ thuộc kèm phiên bản** (Dependency list with versions)
2. **Hướng dẫn cài đặt** (Windows **và** Linux)
3. **Lệnh build cho cả hai hệ điều hành**
4. **Ví dụ sử dụng CLI** (Example CLI usage)
5. **Hạn chế đã biết** (Known limitations)

### 2.3. 🔴 Chuẩn CLI thống nhất cho mọi lab

Mọi công cụ phải theo cùng cấu trúc:

```text
mytool <command>
       [--in INFILE | --text "…"]
       [--out OUTFILE]
       [--key KEYFILE | --key-hex HEX]
       [--iv IV-hex]
       [--nonce NONCE-hex]
       [--mode MODE]
       [--aead]
       [--encode hex|base64|raw]
       [--threads N]
       [--kat path/to/vectors.json]
       [--verbose]
```

Chương trình **bắt buộc**:

- [ ] **Nhận input UTF-8**
- [ ] **Xuất output UTF-8**
- [ ] **Fail closed** khi input sai định dạng (malformed)

> *(Ghi chú: đề liệt kê tập tham số chung; từng lab dùng tập con phù hợp. Ví dụ `--aead` vô nghĩa với CTR thuần. Vẫn nên từ chối tham số không áp dụng bằng lỗi rõ ràng thay vì "unknown option" để thể hiện fail-closed.)*

### 2.4. 🔴 Input / Output và encoding

**Input**

- `--in <file>` — **binary-safe**
- `--text "..."` — **UTF-8**

**Output**

- `--out <file>` — **binary-safe**
- **Trên màn hình, mặc định là Hex** cho ciphertext, digest, chữ ký.
- **Ghi ra file, mặc định là Raw binary** (trừ khi ghi đè bằng tùy chọn).

**Encoding** (`--encode`): `hex` · `base64` · `raw`

### 2.5. 🔴 Sinh số ngẫu nhiên và quản lý khóa

**Bắt buộc dùng:**

- Crypto++ **`AutoSeededRandomPool`**
- OpenSSL **`RAND_bytes`**

**Tuyệt đối không dùng:**

- `rand()`
- `std::random` (hoặc các engine `<random>`) cho mục đích mật mã

**Quy tắc IV / Nonce:**

- [ ] **Bắt buộc kiểm tra đúng độ dài IV**
- [ ] **Bắt buộc chống tái sử dụng nonce** khi bị cấm
- [ ] **Tự sinh an toàn** nếu thiếu (khi phù hợp)
- [ ] **Phải ghi tài liệu định dạng lưu trữ** (header hoặc sidecar)

**Kích thước khóa tối thiểu:**

| Thuật toán | Yêu cầu |
|---|---|
| AES | **128 / 192 / 256 bit** (*ưu tiên 256 trong báo cáo*) |
| RSA | **≥ 3072 bit** |
| ECDSA | **≥ P-256 (secp256r1)** |

### 2.6. 🔴 Kiểm chứng và kiểm thử

**Known Answer Tests (KAT)** — mỗi lab phải có:

- [ ] **Vector chuẩn NIST** (nếu áp dụng được)
- [ ] **Bộ chạy `--kat`**
- [ ] **Định dạng vector JSON**
- Ví dụ: `mytool --kat vectors.json`
- Phải in ra: **PASS/FAIL cho từng test** và **thống kê tổng kết**

**Negative testing** — sinh viên phải có:

- [ ] Test **sai khóa** (wrong key)
- [ ] Test **ciphertext bị sửa** (tampered ciphertext)
- [ ] Test **authentication tag bị sửa** (AEAD)
- [ ] Test **input sai định dạng** (malformed)
- [ ] Test **file PEM/key bị hỏng** (corrupted PEM/key file — *where applicable*)
- Chương trình phải **fail securely** (thất bại an toàn).

**Unit testing**

- [ ] **Bắt buộc dùng Catch2 hoặc GoogleTest**
- [ ] **`ctest` phải pass trên cả Windows và Linux**
- 🟡 Tích hợp CI (khuyến nghị, không bắt buộc)

### 2.7. 🔴 Phương pháp đo hiệu năng (chuẩn hóa)

Với **mỗi thuật toán mật mã**:

1. **Warm-up: 1–2 giây**
2. Thực hiện **một khối ~1.000 thao tác**
3. **Lặp lại 30–100 lần**
4. **Thu thập thống kê**

**Báo cáo thống kê bắt buộc phải có đủ 4 chỉ số:**

| Chỉ số | Bắt buộc |
|---|---|
| Mean (trung bình) | 🔴 |
| Median (trung vị) | 🔴 |
| Standard deviation (độ lệch chuẩn) | 🔴 |
| **95% confidence interval** | 🔴 |

**Phân tích so sánh (khi áp dụng):** Windows vs Linux · các biến thể thuật toán · overhead giữa các mode · ảnh hưởng của tăng tốc phần cứng. Phải kèm **bảng**, **biểu đồ** và **diễn giải** (không chỉ số liệu thô).

**Performance Protocol (cuối đề — "copy vào mỗi lab"):**

| Hạng mục | Yêu cầu |
|---|---|
| Bảng phần cứng/OS | CPU model, số core/thread, RAM, loại ổ lưu trữ, phiên bản OS, compiler và cờ biên dịch |
| Warm-up | 1–2 s để ổn định cache và allocator; mỗi khối chạy 1000 vòng |
| Số lần chạy | **N ≥ 30** lần đo độc lập cho mỗi trường hợp |
| Chỉ số | mean, median, std dev, 95% CI; biểu đồ **có error bar** |
| Công bằng | Ghim CPU governor (Linux: `performance`; Windows: "High performance") |
| Lặp lại được | PRNG seed cố định **chỉ** dùng để sinh dữ liệu tổng hợp, **không bao giờ** cho khóa/nonce |

### 2.8. 🔴 Security Engineering Standards — phần báo cáo bắt buộc của MỖI lab

Mỗi báo cáo lab phải thảo luận:

- [ ] **Threat model** (mô hình mối đe dọa)
- [ ] **Misuse scenarios** (kịch bản dùng sai)
- [ ] **Known attacks** liên quan đến thuật toán
- [ ] **Secure defaults** (cấu hình mặc định an toàn)
- [ ] **Fail-closed behavior** (hành vi thất bại an toàn)
- [ ] **Limitations** của bản cài đặt

> Đề nhấn mạnh: *"Students must demonstrate understanding beyond implementation."*

### 2.9. 🔴 Yêu cầu báo cáo (một file duy nhất cho cả 6 lab)

Báo cáo phải chứa:

- [ ] **Objectives** của từng lab
- [ ] **Implementation details**
- [ ] **Test results**
- [ ] **Performance methodology**
- [ ] **Statistical tables & plots**
- [ ] **Security analysis**
- [ ] **Cross-platform comparison**
- [ ] **Lessons learned**

**Độ dài tối thiểu mong đợi: ~60 trang tổng cộng** (typical). Mẫu cụ thể ở [mục 9.1](#91-mẫu-báo-cáo-reporting-template).

### 2.10. 🟢 GUI Bonus (tùy chọn)

Có thể:

- **Export core mật mã** thành `.dll` / `.so` / `.a` / `.lib`
- **Làm GUI** bằng Python (PySide6/Qt) hoặc C# (WPF/WinUI)

GUI **bắt buộc**:

- [ ] **Gọi cùng thư viện đã biên dịch** (same compiled library)
- [ ] **Không nhân bản logic mật mã**

Điểm bonus theo đặc tả từng lab (Lab 1: +5).

### 2.11. 🔴 Liêm chính học thuật và đạo đức

Sinh viên phải đưa vào báo cáo:

- [ ] **Tuyên bố tự thực hiện** (statement of originality)
- [ ] **Khai báo việc dùng AI/công cụ** hỗ trợ (nếu có)
- [ ] **Trích dẫn đúng** tài liệu tham khảo bên ngoài
- [ ] **Không sao chép** cài đặt từ bên ngoài

**Vi phạm sẽ bị xử lý kỷ luật học thuật.** Văn bản mẫu (Appendix A) ở [mục 9.3](#93-appendix-a--academic-integrity--ethics-dán-vào-mỗi-báo-cáo).

### 2.12. Learning impact cấp chương trình (để biết đề muốn đánh giá điều gì)

Hiểu sâu mật mã đối xứng và bất đối xứng · cài đặt hệ thống mật mã an toàn · nhận diện mẫu dùng sai trong thực tế · đo hiệu năng có cơ sở thống kê · viết tài liệu kỹ thuật chuyên nghiệp · nối lý thuyết với thực hành kỹ nghệ.

---

## 3. Lab 1 — Symmetric Encryption với Crypto++

### 3.1. ⚠ Ràng buộc

- Phải dùng **Crypto++** cho nguyên thủy mật mã.
- **Không** dùng OpenSSL, libsodium hay thư viện mật mã nào khác.
- C++ STL được phép.
- Phải biên dịch trên Windows và Linux; bắt buộc CMake.

### 3.2. 🔴 Phạm vi thuật toán

**A. Các mode AES (chọn lúc chạy)** — phải hỗ trợ đủ 8 mode:

```text
--mode ecb|cbc|ofb|cfb|ctr|xts|ccm|gcm
```

`ECB` · `CBC` · `OFB` · `CFB` · `CTR` · `XTS` · `CCM` (AEAD) · `GCM` (AEAD)

**B. Xử lý AEAD (CCM, GCM)**

- [ ] Cờ **`--aead`** bật hành vi xác thực
- [ ] Hỗ trợ **`--aad FILE`** và **`--aad-text STRING`**
- [ ] **Phải xác minh authentication tag khi giải mã**
- [ ] Tag sai → **fail closed**

**C. Yêu cầu chống dùng sai (misuse prevention)** — phải cài đặt kiểm tra tường minh

*Hạn chế ECB:*
- [ ] **In `WARNING`** khi chọn ECB
- [ ] **Chặn file lớn hơn 16 KiB** theo mặc định
- [ ] Chỉ cho phép ghi đè bằng **`--allow-ecb`**

*Xử lý IV / Nonce (với mode cần IV/nonce):*
- [ ] **Ép đúng độ dài** theo từng mode
- [ ] Nếu thiếu IV/nonce: **sinh an toàn bằng Crypto++ `AutoSeededRandomPool`**
- [ ] **Lưu IV vào header đầu ra hoặc sidecar JSON**
- [ ] **Từ chối** IV có độ dài sai

*Chống tái sử dụng nonce — với CTR, CCM, GCM:*
- [ ] Nếu header cho thấy **cùng key + nonce đã dùng trước đó → từ chối thao tác**
- [ ] **Phải ghi tài liệu logic phát hiện**

### 3.3. 🔴 CLI và I/O

```bash
aestool encrypt --mode gcm --key key.bin --in msg.txt --out ct.bin
aestool decrypt --mode gcm --key key.bin --in ct.bin --out msg.txt
```

**Xử lý khóa** — hỗ trợ cả hai:

- `--key-hex HEXSTRING`
- `--key KEYFILE` — file khóa có thể là **raw binary** hoặc **hex có header**

**Xử lý IV / Nonce:**

- `--iv IVFILE`
- `--nonce NONCEFILE`
- Nếu bỏ qua (trừ ECB/XTS): **phải tự sinh an toàn** và **phải lưu vào header hoặc sidecar**

**Input và encoding:**

- Input từ file hoặc `--text`
- Output: **ciphertext thô ghi ra file**; **Hex hoặc Base64 hiển thị lên màn hình**
- An toàn UTF-8

**Sidecar JSON header (tùy chọn nhưng nếu dùng phải có đủ trường):**

```json
{
  "alg": "AES-256-GCM",
  "iv": "…hex…",
  "aad": "…hex…",
  "tag": "…hex…"
}
```

Header phải chứa: **thuật toán · mode · IV/nonce · AAD (nếu có) · authentication tag (mode AEAD)**.

### 3.4. 🔴 Đúng đắn và kiểm chứng

**KAT — phải kiểm chứng với:**

- NIST SP 800-38A: **CBC, CFB, OFB, CTR**
- **Vector NIST GCM**
- **Vector NIST CCM**

**KAT runner:** `--kat vectors.json` — phải **parse file JSON, chạy test case, in PASS/FAIL từng case, in tổng kết**.

**Negative tests — phải chứng minh:**

| Tình huống | Kết quả mong đợi |
|---|---|
| Sai key | Plaintext sai |
| Sai IV | Plaintext sai |
| Ciphertext bị sửa (non-AEAD) | Output bị hỏng |
| Ciphertext bị sửa (AEAD) | **Lỗi xác thực** |
| Tag không hợp lệ | **Từ chối giải mã** |
| Độ dài IV sai | **Bị từ chối** |

Mọi input sai định dạng đều phải **fail closed**.

### 3.5. 🔴 Đánh giá hiệu năng

**Kích thước payload:** 1 KB · 4 KB · 16 KB · 256 KB · 1 MB · 8 MB

**Chỉ số:** Throughput (MB/s) · Latency mỗi thao tác

**Nghiên cứu so sánh bắt buộc:**

- Windows vs Linux
- Stream mode vs Block mode
- AEAD vs non-AEAD
- Overhead của tag (GCM/CCM)

Kèm **bảng**, **biểu đồ**, và **phân tích ảnh hưởng của OS scheduling và system call overhead**.

### 3.6. 🔴 Thảo luận bảo mật (mục báo cáo BẮT BUỘC)

*Mức mode:*
- Vì sao ECB không an toàn
- Rủi ro padding oracle của CBC
- Thảm họa tái sử dụng nonce trong CTR
- Đảm bảo của AEAD so với non-AEAD
- Vì sao GCM bắt buộc nonce duy nhất
- Hạn chế của XTS (không có tính toàn vẹn)

*Mức cài đặt:*
- Sinh ngẫu nhiên đúng cách
- Lưu IV an toàn
- Xác minh authentication tag
- Xử lý lỗi (fail closed)
- Rủi ro lưu trữ khóa

*Đa nền tảng:*
- Khác biệt RNG giữa các OS
- Hành vi hệ thống file
- Nguyên nhân dao động hiệu năng

### 3.7. 🟢 Bonus (+5)

Export core mã hóa thành `.dll` (Windows) / `.so` (Linux) / `.a`/`.lib` **và** dựng GUI tối thiểu bằng Python hoặc C#. **GUI phải gọi thư viện đã biên dịch.**

### 3.8. Rubric Lab 1 (100 điểm)

| Thành phần | Điểm |
|---|---:|
| Correctness & KATs | 25 |
| Security hygiene | 15 |
| UX & I/O design | 10 |
| Performance methodology | 20 |
| Cross-platform build | 10 |
| Report quality | 20 |
| **Bonus: Library export + GUI** | **+5** |

Learning impact mong đợi: dùng thư viện mã hóa đối xứng thực tế · khác biệt block/stream/AEAD · vòng đời IV/nonce · rủi ro dùng sai · đánh đổi hiệu năng giữa các OS · tầm quan trọng của xác thực.

---

## 4. Lab 2 — AES-128 C++ thuần (CTR, FIPS-197)

### 4.1. ⚠ Ràng buộc

- **Không** dùng thư viện mật mã ngoài. **STL được phép.**
- **Mọi nguyên thủy mật mã phải tự cài đặt thủ công.**
- **Không** dùng OpenSSL, Crypto++, libsodium...
- **AES-NI chỉ được dùng ở phần bonus.**
- OpenSSL/Crypto++ chỉ để **cross-validation** và **so sánh hiệu năng**, không được gọi từ lõi AES.

### 4.2. 🔴 Phạm vi thuật toán

**A. AES block cipher (FIPS-197, bản cập nhật 09/05/2023) — AES-128 bắt buộc**

Phải tự cài đặt: **AddRoundKey · SubBytes (tra S-box) · ShiftRows · MixColumns · KeyExpansion**.

| Tham số | Giá trị |
|---|---|
| Kích thước khối | 128 bit |
| Kích thước khóa | 128 bit |
| Số vòng | 10 |

**Phải tuân thủ nghiêm ngặt FIPS-197.**

**B. Chế độ CTR (NIST SP 800-38A)**

Định nghĩa đúng với block thứ *i*:

```text
S_i = AES(K, Nonce | Counter_i)
C_i = P_i XOR S_i
P_i = C_i XOR S_i
```

- [ ] **IV 128-bit (Nonce + Counter)**
- [ ] **Bộ đếm tăng đúng**
- [ ] **Phải ghi tài liệu rõ:** **endianness của counter** và **hành vi khi counter tràn**
- [ ] Hỗ trợ **plaintext độ dài tùy ý**
- [ ] Xử lý đúng **khối cuối thiếu**
- [ ] **Không padding**

**🔴 Critical Security Rule:** *IV (nonce) phải duy nhất cho mỗi key.* **Tái sử dụng IV dưới cùng key phải được thảo luận như một thảm họa (catastrophic).**

### 4.3. 🔴 CLI và Input/Output

```bash
aestool encrypt --mode ctr --key key.bin --iv iv.bin --in msg.bin --out ct.bin
aestool decrypt --mode ctr --key key.bin --iv iv.bin --in ct.bin --out msg.bin
```

- Input/output: **raw binary**; tùy chọn hex để debug
- **Key: đúng 16 byte** · **IV: đúng 16 byte**

**Kiểm tra đầu vào (fail closed) — phải kiểm:**

- [ ] Độ dài key sai
- [ ] Độ dài IV sai
- [ ] **Phát hiện tràn counter** (nếu áp dụng)
- [ ] Lỗi file I/O

CTR **không** cần: padding, căn khối, kiểm tra padding. Mọi input sai định dạng phải **dừng chương trình một cách an toàn**.

### 4.4. 🔴 Đúng đắn và kiểm chứng

**KAT:** vector AES chính thức **FIPS-197** và vector **NIST SP 800-38A CTR**.

**Cross-validation:** phải cho **ciphertext giống hệt** với **Crypto++ AES-128-CTR** và **OpenSSL AES-128-CTR** (cùng key, cùng IV).

**Negative tests — phải chứng minh:**

- Sai key → plaintext sai
- Sai IV → plaintext sai
- Ciphertext bị sửa → plaintext hỏng
- **Phải giải thích vì sao CTR không thể phát hiện việc sửa đổi.**

### 4.5. 🔴 Đánh giá hiệu năng

- Throughput **mã hóa AES-128** và **giải mã AES-128**
- Kích thước file: **1 MiB · 100 MiB · 1 GiB (nếu khả thi)**

**Phân tích bắt buộc:**

- S-box dạng bảng (table) vs S-box tính toán (computed)
- Memory footprint
- Hành vi cache
- Ảnh hưởng của branch prediction
- So sánh với hiệu năng Crypto++

Kèm **bảng throughput (MB/s)** và **biểu đồ throughput**.

### 4.6. 🔴 Thảo luận bảo mật (mục báo cáo BẮT BUỘC)

*Rủi ro lõi AES:* timing leakage từ S-box dạng bảng · tấn công dựa trên cache (T-table leakage) · tầm quan trọng của constant-time coding.

*Rủi ro mode CTR:* keystream reuse · two-time pad · **vì sao tái sử dụng IV là thảm họa** · vì sao CTR chỉ cho bảo mật (confidentiality only) · vì sao CTR cần MAC để xác thực · vì sao CTR không phải AEAD.

*Tăng tốc phần cứng:* vì sao AES-NI giảm nhiều rủi ro rò rỉ phần mềm · đánh đổi hiệu năng vs bảo mật.

### 4.7. 🟢 Advanced Topics (+20 Bonus)

| Bonus | Điểm | Nội dung |
|---|---:|---|
| **AES-192 / AES-256** | +10 | **Lịch khóa đầy đủ** + **bắt buộc có KAT** |
| **AES-XTS** | +5 | Cấu trúc **hai khóa** · **nhân tweak trong GF(2¹²⁸)** · giải thích use-case mã hóa ổ đĩa |
| **AES-NI hoặc bit-sliced** | +5 | **Phát hiện tính năng CPU lúc chạy** · so sánh hiệu năng với phần mềm thuần · giải thích cải thiện constant-time |

### 4.8. Rubric Lab 2 (100 điểm + Bonus)

| Thành phần | Điểm |
|---|---:|
| Lõi AES-128 đúng FIPS-197 | 30 |
| CTR mode đúng | 15 |
| KATs & cross-validation | 15 |
| Engineering quality & documentation | 10 |
| Performance study & analysis | 10 |
| Security discussion | 20 |
| Bonus: AES-192/256 | +10 |
| Bonus: XTS | +5 |
| Bonus: AES-NI/bit-sliced | +5 |

Learning impact: cấu trúc vòng AES · hoạt động của stream mode · vì sao nonce reuse phá confidentiality · vì sao CTR phải kết hợp MAC · vì sao constant-time quan trọng · tăng tốc phần cứng thay đổi tư thế bảo mật ra sao.

---

## 5. Lab 3 — RSA-OAEP và Hybrid Encryption

Thư viện: **Crypto++ / OpenSSL**.

### 5.1. Learning outcomes

Sinh và lưu an toàn cặp khóa RSA · cài RSA-OAEP (SHA-256) · xử lý giới hạn kích thước plaintext và hybrid encryption · phát hiện và xử lý lỗi mật mã · đo hiệu năng RSA theo cỡ khóa · hiểu vì sao hybrid là cần thiết.

### 5.2. 🔴 Thuật toán và tham số

**A. RSA-OAEP**

| Tham số | Thiết lập bắt buộc |
|---|---|
| Kích thước modulus | **≥ 3072 bit** |
| Hàm băm | **SHA-256** |
| Padding | **OAEP** |
| MGF | **MGF1 (SHA-256)** |

- [ ] Cài **RSA-3072 với OAEP(SHA-256)**
- [ ] Thêm **RSA-4096** để so sánh hiệu năng
- [ ] Hỗ trợ **OAEP label** tùy chọn

**Thảo luận bắt buộc (trong báo cáo):**

- Vì sao OAEP an toàn (**IND-CCA2**)
- Vì sao **PKCS#1 v1.5 encryption không an toàn**
- Giới hạn kích thước plaintext: **mLen ≤ k − 2·hLen − 2**
  *(Ghi chú: với k = 384 byte (RSA-3072), hLen = 32 → tối đa **318 byte**; RSA-4096 (k = 512) → **446 byte**.)*
- Vì sao RSA **không thể mã hóa file lớn trực tiếp**

**B. Hybrid Encryption (Envelope Encryption)** — khi plaintext vượt giới hạn OAEP:

1. Sinh **khóa AES ngẫu nhiên 256-bit**
2. Mã hóa dữ liệu bằng **AES-GCM**
3. Mã hóa khóa AES bằng **RSA-OAEP**
4. Xuất **envelope có cấu trúc**

(Phản ánh hành vi của TLS và PGP trong thực tế.)

### 5.3. 🔴 CLI và định dạng

**Sinh khóa:**

```bash
rsatool keygen --bits 3072 --pub pub.pem --priv priv.pem
```

Phải lưu: **định dạng PEM**, **định dạng DER**, và **metadata JSON**:

```json
{
  "creation_time": "...",
  "modulus_bits": 3072,
  "hash": "SHA-256"
}
```

**Mã hóa:**

```bash
rsatool encrypt --in msg.bin --pub pub.pem --out ct.bin --label optional
```

Nếu thông điệp vượt giới hạn RSA → **tự động chuyển sang hybrid mode** và xuất envelope.

**Giải mã:**

```bash
rsatool decrypt --in ct.bin --priv priv.pem --out msg.bin
```

**Định dạng hỗ trợ:** ciphertext **raw/hex/base64** · header envelope **JSON** · khóa **PEM/DER**.

**Phải kiểm tra (validate):** ciphertext sai định dạng · **label sai** · cỡ khóa không hỗ trợ · encoding sai.

### 5.4. 🔴 Định dạng Hybrid Envelope

```json
{
  "mode": "RSA-OAEP-AES-GCM",
  "rsa_modulus": 3072,
  "hash": "SHA-256",
  "wrapped_key": "<base64>",
  "iv": "<base64>",
  "tag": "<base64>"
}
```

Payload: dữ liệu mã hóa bằng AES-GCM.

**Thuộc tính bảo mật bắt buộc:**

- [ ] **AES-256-GCM**
- [ ] **IV 96-bit**
- [ ] **Giải mã có xác thực**
- [ ] **Từ chối ciphertext bị sửa**
- [ ] Kiểm tra tag **constant-time** (*dùng hàm của thư viện được chấp nhận*)

### 5.5. 🔴 Đúng đắn và negative tests

Phải chứng minh:

- RSA ciphertext bị đổi → giải mã **thất bại**
- AES-GCM ciphertext bị đổi → **lỗi tag**
- **Sai private key** → thất bại
- **Sai OAEP label** → thất bại
- **Header envelope bị sửa** → thất bại

Phải có: **unit test tự động** · **thông báo lỗi rõ ràng, không rò rỉ thông tin (non-leaky)** · **fail closed khi parse lỗi**.

### 5.6. 🔴 Đánh giá hiệu năng

**RSA:** thời gian sinh khóa · thời gian mã hóa · thời gian giải mã · so sánh **3072 vs 4096**.

**Hybrid:** throughput mã hóa AES-GCM · thời gian mã hóa hybrid cho **1 KiB · 1 MiB · 100 MiB**.

**Phân tích bắt buộc:** chi phí giải mã so với mã hóa RSA · đánh đổi cỡ khóa vs hiệu năng · lợi thế hiệu quả của hybrid · vì sao mã hóa đối xứng chi phối throughput · cân nhắc bộ nhớ và CPU. Kèm **biểu đồ latency và throughput**.

### 5.7. 🟢 Advanced Topics (+15)

| Bonus | Điểm | Nội dung |
|---|---:|---|
| **Manual OAEP padding** | +5 | Tự cài encode/decode OAEP: **MGF1**, **XOR masking**, **kiểm tra padding**; giải thích **vì sao decode phải constant-time** |
| **Phân tích bảo mật AES+RSA hybrid** | +10 | So sánh envelope vs RSA trực tiếp · TLS handshake dùng thiết kế hybrid thế nào · hạn chế **forward secrecy** của RSA thuần · đề xuất lộ trình nâng cấp lên **ECDHE hoặc ML-KEM** (xem trước Lab 6) |
| *Mở rộng tùy chọn* | — | Thêm so sánh AES-CTR + HMAC vs AES-GCM; đo độ biến thiên thời gian khi xác thực thất bại |

### 5.8. Rubric Lab 3 (100 điểm + Bonus)

| Thành phần | Điểm |
|---|---:|
| RSA-OAEP (SHA-256) đúng | 20 |
| Hybrid envelope (AES-GCM + RSA) đúng | 20 |
| Key management & formats | 15 |
| Negative tests & secure error handling | 15 |
| Performance study & analysis | 15 |
| Cross-platform build & documentation | 5 |
| Report quality | 10 |
| Bonus: manual OAEP | +5 |
| Bonus: hybrid security analysis | +10 |

---

## 6. Lab 4 — Hashing, PKI và Practical Attacks

### 6.1. ⚠ Ethics Notice (BẮT BUỘC tuân thủ)

> Lab này **chỉ phục vụ giáo dục phòng thủ**. Dùng **file thử nghiệm offline do bạn sở hữu**, trong **môi trường cô lập**. **KHÔNG** nhắm vào hệ thống thật hay dịch vụ đang chạy.

### 6.2. Learning outcomes

Cài và benchmark hàm băm hiện đại · parse và kiểm tra chứng chỉ X.509 · minh họa an toàn điểm yếu va chạm MD5 · minh họa length-extension trên MAC ngây thơ · giải thích vì sao hàm băm cũ không an toàn · đề xuất biện pháp giảm thiểu (HMAC, SHA-2/3, chính sách).

### 6.3. 🔴 Bộ công cụ băm (Hashing suite)

Một CLI băm (`hashtool`) phải hỗ trợ:

| Họ | Thuật toán (kích thước đầu ra) |
|---|---|
| **SHA-2** | SHA-224 (224) · SHA-256 (256) · SHA-384 (384) · SHA-512 (512) |
| **SHA-3** | SHA3-224 · SHA3-256 · SHA3-384 · SHA3-512 |
| **SHAKE (XOF)** | **SHAKE128**, **SHAKE256** — độ dài đầu ra **biến thiên** |

```bash
hashtool --algo shake256 --outlen 64 --in file.bin
hashtool --algo sha256 --in file.bin
hashtool --algo sha512 --in large.iso --stream
hashtool --algo shake128 --outlen 128 --in file.bin
```

**Tính năng bắt buộc:**

- [ ] **KAT**
- [ ] **Streamed I/O** (hỗ trợ file nhiều GB)
- [ ] Output: **hex (console)** và **raw binary (file)**
- [ ] Xử lý input sai định dạng · **kiểm tra định danh thuật toán** · **fail closed khi tham số không hỗ trợ**

**Thảo luận bắt buộc:** Merkle–Damgård vs sponge · collision resistance vs preimage resistance · vì sao SHA-3 khác cấu trúc với SHA-2 · ứng dụng của XOF (ví dụ domain separation).

### 6.4. 🔴 PKI và phân tích chứng chỉ

**Parse chứng chỉ X.509** bằng OpenSSL hoặc Crypto++. **Phải trích xuất:**

- Subject
- Issuer
- **Subject Public Key Info** (thuật toán và tham số)
- Signature algorithm
- Validity period
- Key usage
- **Subject Alternative Names (SANs)**

**Xác minh chữ ký:**

- [ ] Xác minh chữ ký chứng chỉ bằng **public key của issuer** (nếu được cung cấp)
- [ ] Nếu **không có** issuer key: **xác minh tính toàn vẹn cấu trúc TBS** · **kiểm tra tính nhất quán thuật toán** · **fail closed**

**TLS Deployment Task** — cấu hình HTTPS trên:

- **Apache hoặc Nginx**
- **TLS 1.2 hoặc TLS 1.3**
- Dùng **root đáng tin cậy** (ví dụ ZeroSSL)
- Với **chứng chỉ ECDSA** (ưu tiên)

**Nộp:** ảnh chụp màn hình hoặc log · đoạn cấu hình (config snippet) · giải thích ngắn về chuỗi tin cậy (trust chain).

**Thảo luận bắt buộc:** cấu trúc X.509 (TBS vs signature) · chuỗi tin cậy · vì sao **SHA-1 và MD5 bị cấm** · Certificate Transparency và yêu cầu CA/B.

### 6.5. 🔴 Minh họa va chạm MD5 có kiểm soát

Phải minh họa va chạm MD5 bằng **`hashclash`**. **Chọn MỘT:**

- **Option A:** hai file **PNG vô hại** có cùng MD5
- **Option B:** hai **chương trình C++ ngắn** có cùng MD5

**Phải nộp:**

- [ ] Cả hai file đầu vào
- [ ] Giá trị MD5 trùng khớp
- [ ] **Bài giải thích 1 trang:** vì sao MD5 bị phá · khác biệt giữa collision và preimage · các sự cố lịch sử

**Không được nhắm vào mục tiêu thật.**

### 6.6. 🔴 Length-extension attack trên MAC ngây thơ

Cấu trúc mục tiêu: **MAC = H(k ‖ m)**.

Dùng **`hashpump`** (hoặc tự cài — bonus +5). Phải minh họa: cho trước **H(k‖m)** và **độ dài của k**, tính **H(k ‖ m ‖ pad ‖ m')** **mà không biết k**.

**Phải nộp:**

- [ ] Thông điệp gốc
- [ ] Thông điệp mở rộng giả mạo (forged)
- [ ] MAC giả mạo khớp
- [ ] **Sơ đồ padding**
- [ ] Giải thích biện pháp giảm thiểu (mitigation)

**Thảo luận bắt buộc:** vì sao Merkle–Damgård cho phép length extension · vì sao **HMAC** ngăn được · cấu trúc prefix-free.

### 6.7. 🔴 Đánh giá hiệu năng

Benchmark: **SHA-256 · SHA-512 · SHA3-256 · SHA3-512**, trên **1 MiB · 100 MiB · 1 GiB (nếu khả thi)**.

**Phân tích bắt buộc:** throughput (MB/s) · **streaming vs memory-mapped I/O** · **sử dụng CPU** · khác biệt hiệu năng SHA-2 vs SHA-3 · ảnh hưởng cache và bộ nhớ. Kèm **biểu đồ hiệu năng**. Test trên **cả hai OS** (Windows + Linux nếu có thể).

### 6.8. 🟢 Advanced Topics (+15)

| Bonus | Điểm | Nội dung |
|---|---:|---|
| **Custom length-extension** | +5 | Tự cài cho SHA-256: **dựng lại trạng thái nội bộ** · áp padding · tiếp tục hàm nén · giải thích thao tác trạng thái nội bộ |
| **Chosen-prefix certificate demo** | +10 | **Chỉ trong sandbox:** sinh chứng chỉ tự ký MD5/RSA · dùng chosen-prefix collision · tạo chứng chỉ thứ hai cùng chữ ký MD5. Nộp: giải thích vì sao chứng chỉ ký MD5 là thảm họa · biện pháp chính sách (cấm MD5/SHA-1, bắt buộc CA/B Baseline Requirements, algorithm agility). **Không cần exploit vận hành.** |

### 6.9. Rubric Lab 4 (100 điểm + Bonus)

| Thành phần | Điểm |
|---|---:|
| Hash suite correctness & KATs | 25 |
| Streaming & large-file performance | 10 |
| X.509 parsing & signature verification | 25 |
| MD5 collision demo & explanation | 20 |
| Length-extension demo & mitigation analysis | 20 |
| Bonus: custom length-extension | +5 |
| Bonus: chosen-prefix certificate demo | +10 |

---

## 7. Lab 5 — Chữ ký số cổ điển (ECDSA, RSA-PSS)

Thư viện: **Crypto++ + OpenSSL**.

### 7.1. Learning outcomes

Sinh cặp khóa ECDSA và RSA-PSS · tạo và xác minh **chữ ký detached** · chọn tham số và hàm băm an toàn · phát hiện lỗi cài đặt và dùng sai chữ ký · benchmark sign/verify · so sánh chữ ký đường cong elliptic với RSA.

### 7.2. 🔴 Thuật toán và tham số

**A. ECDSA (đường cong NIST prime)**

| Mức bảo mật | Đường cong | Độ mạnh cổ điển xấp xỉ |
|---|---|---|
| Level 1 | **secp256r1 (P-256)** | ~128-bit |
| Level 3 | secp384r1 (P-384) | ~192-bit |

- [ ] **Bắt buộc:** cài **ECDSA-P256**
- 🟢 Thêm **ECDSA-P384 (+5 điểm)**
- [ ] **Phải dùng ECDSA tất định (RFC 6979)** trừ khi có lý do chính đáng khác

**Thảo luận bắt buộc:** nonce tất định (RFC 6979) vs nonce ngẫu nhiên · **lỗ hổng thảm họa khi tái sử dụng nonce** · kích thước chữ ký so với RSA-PSS · chi phí verify so với sign.

**B. RSA-PSS**

| Tham số | Thiết lập bắt buộc |
|---|---|
| Kích thước modulus | **3072 bit** |
| Hàm băm | **SHA-256** |
| Độ dài salt | **hashLen (32 byte)** |

- [ ] Cài **RSA-PSS-3072 với SHA-256**
- [ ] **Dùng salt ngẫu nhiên cho mỗi chữ ký**

**Thảo luận bắt buộc:** vì sao PSS được ưu tiên hơn PKCS#1 v1.5 · vai trò của salt trong chữ ký xác suất · kích thước chữ ký so với ECDSA · lựa chọn số mũ công khai (ví dụ **65537**).

### 7.3. 🔴 CLI và định dạng

*(CLI theo cùng cấu trúc với Lab 6 để nhất quán.)*

```bash
sigtool keygen --algo ecdsa-p256 --pub pub.pem --priv priv.pem
sigtool keygen --algo rsa-pss-3072 --pub pub.pem --priv priv.pem
sigtool sign   --algo ecdsa-p256 --in msg.bin --out sig.bin --hash sha256
sigtool verify --algo ecdsa-p256 --in msg.bin --sig sig.bin --pub pub.pem
```

**Định dạng:** khóa **PEM/DER** · chữ ký **raw / DER / base64** (`--encode`) · hash: **SHA-256 bắt buộc** (SHA-384 tùy chọn cho P-384).

**Phải kiểm tra:** khóa sai định dạng · **định danh thuật toán sai** · encoding không hỗ trợ · lỗi hash không khớp.

### 7.4. 🔴 Đúng đắn và negative tests

Phải chứng minh:

- Message bị sửa → verify **thất bại**
- Signature bị sửa → thất bại
- **Public key bị sửa** → thất bại
- **Algorithm identifier sai** → thất bại
- **Hash function sai** → thất bại

Phải có: **unit test tự động** · **Batch verification (verify N chữ ký)** · **mã lỗi và thông báo UX rõ ràng**.

**Thảo luận bắt buộc:** vì sao verify chữ ký phải **constant-time** · xử lý lỗi sai có thể rò rỉ thông tin như thế nào.

### 7.5. 🔴 Đánh giá hiệu năng

**ECDSA** và **RSA-PSS** — mỗi loại đo: **latency sinh khóa · latency sign · latency verify · throughput (ops/sec)**.

**Kích thước message:** 1 KiB · 16 KiB · 1 MiB · 8 MiB.

**Phân tích bắt buộc:** hash chi phối chi phí khi message lớn · verify ECDSA nhanh hơn vs đặc tính RSA-PSS · so sánh kích thước chữ ký · cân nhắc bộ nhớ. **Phải so sánh kết quả với Lab 6 (PQC)** trong phần thảo luận. Khuyến khích biểu đồ (latency theo kích thước message).

### 7.6. 🟢 Advanced Topics (+15)

- **A. Cài ở mức công thức (từ nguyên thủy):** phép toán điểm đường cong elliptic · nghịch đảo modular · lũy thừa modular RSA · sinh padding PSS · giải thích cân nhắc constant-time và rủi ro side-channel.
- **B. Cân nhắc security engineering:** bề mặt tấn công timing · rủi ro fault injection · rủi ro side-channel trong lũy thừa modular · bảo vệ nonce tất định · kiểm chứng một phần (đo biến thiên thời gian cơ bản, thảo luận thư viện constant-time). *(Không cần demo khai thác.)*

### 7.7. Rubric Lab 5 (100 điểm + Bonus)

| Thành phần | Điểm |
|---|---:|
| ECDSA sign/verify đúng | 20 |
| RSA-PSS sign/verify đúng | 20 |
| Robust key handling & formats | 15 |
| Negative tests & UX | 10 |
| Performance study & analysis | 20 |
| Cross-platform build & documentation | 5 |
| Report quality | 10 |
| Bonus (advanced implementation) | +15 |

---

## 8. Lab 6 — Post-Quantum (ML-DSA, ML-KEM)

### 8.1. Learning outcomes

Sinh cặp khóa PQC (ML-DSA, ML-KEM) · ký và xác minh chữ ký detached bằng ML-DSA · encapsulation/decapsulation bằng ML-KEM · hiểu chọn tham số PQC và mức bảo mật · benchmark chữ ký PQ và KEM · dựng cấu trúc chứng chỉ PQ đơn giản (khóa công khai được ký bằng ML-DSA).

### 8.2. 🔴 Thuật toán và tham số

**A. ML-DSA**

| Mức | Parameter set | Tương đương cổ điển |
|---|---|---|
| Level 2 | **ML-DSA-44** | ~128-bit |
| Level 3 | ML-DSA-65 | ~192-bit |
| Level 5 | ML-DSA-87 | ~256-bit |

- [ ] **Bắt buộc cài ML-DSA-44**
- 🟢 Thêm **ML-DSA-65 (+5 điểm)**
- [ ] Thảo luận đánh đổi **kích thước chữ ký vs mức bảo mật**

**Thảo luận trọng tâm:** chữ ký **lớn** (so với ECDSA) · **ký tất định** (không có vấn đề kiểu RFC 6979 như ECDSA) · **rejection sampling** và xác suất thất bại.

**B. ML-KEM**

| Mức | Parameter set |
|---|---|
| Level 2 | **ML-KEM-512** |
| Level 3 | ML-KEM-768 |
| Level 5 | ML-KEM-1024 |

- [ ] **Bắt buộc cài ML-KEM-512**
- [ ] Thực hiện **encapsulation → shared secret**
- [ ] Thực hiện **decapsulation → kiểm tra shared secret khớp**
- [ ] Thảo luận: **IND-CCA** · **Fujisaki–Okamoto transform** · kích thước ciphertext so với trao đổi khóa RSA

### 8.3. 🔴 CLI và định dạng

```bash
pqtool keygen --algo mldsa-44  --pub pub.pem --priv priv.pem
pqtool keygen --algo mlkem-512 --pub pub.pem --priv priv.pem
pqtool sign   --algo mldsa-44  --in msg.bin --out sig.bin
pqtool verify --algo mldsa-44  --in msg.bin --sig sig.bin --pub pub.pem
pqtool encaps --algo mlkem-512 --pub pub.pem  --ct ct.bin --ss ss.bin
pqtool decaps --algo mlkem-512 --priv priv.pem --ct ct.bin --ss ss.bin
```

**Định dạng:** khóa **PEM/DER** · chữ ký **raw/base64** · ciphertext **nhị phân** · 🟡 tùy chọn: định dạng chứng chỉ **JSON có cấu trúc**.

### 8.4. 🔴 Post-Quantum Certificate Mini-Project

Cài cấu trúc chứng chỉ tối thiểu:

```json
{
  "subject": "your Subject",
  "public_key": "<ML-DSA public key>",
  "issuer": "PQ-CA",
  "signature": "<ML-DSA signature by CA>"
}
```

**Các việc phải làm:**

- [ ] **Sinh cặp khóa CA (ML-DSA)**
- [ ] **Ký public key của subject**
- [ ] **Xác minh chữ ký chứng chỉ**
- [ ] **Phát hiện can thiệp (tampering)**

**Thảo luận bắt buộc:** **vì sao ML-KEM KHÔNG dùng để ký** · chứng chỉ **hybrid (ECDSA + ML-DSA)** · lộ trình chuyển sang **PQ TLS**.

### 8.5. 🔴 Đúng đắn và negative tests

Phải chứng minh:

- Message bị sửa → verify thất bại
- Signature bị sửa → thất bại
- Public key bị sửa → thất bại
- **Ciphertext KEM bị sửa → decapsulation thất bại**
- **Sai private key → thất bại**

Phải có: **unit test tự động** · **Batch verification (verify N chữ ký)** · **Batch decapsulation timing**.

### 8.6. 🔴 Đánh giá hiệu năng

**ML-DSA:** latency keygen · sign · verify · throughput (ops/sec).
**ML-KEM:** latency keygen · encapsulation · decapsulation · throughput sinh shared secret.

**Kích thước message thử:** 1 KiB · 16 KiB · 1 MiB · 8 MiB.

**Thảo luận:** hash chi phối chi phí · overhead chữ ký lớn · bộ nhớ · so sánh với ECDSA/RSA-PSS (*so sánh lý thuyết được chấp nhận*).

### 8.7. 🟢 Advanced Topics (+15)

- **A. ML-DSA hoặc ML-KEM ở mức công thức (từ nguyên thủy):** số học đa thức dựa trên **NTT** · modular reduction · **rejection sampling** · cân nhắc constant-time.
- **B. Nhận thức side-channel:** bề mặt tấn công timing · rủi ro rò rỉ lattice · tầm quan trọng của **NTT constant-time** · xử lý lỗi trong decapsulation · kiểm chứng một phần (đo biến thiên thời gian cơ bản, thảo luận timing phụ thuộc input).

### 8.8. Rubric Lab 6 (theo đề: cộng lại là 110)

| Thành phần | Điểm |
|---|---:|
| ML-DSA sign/verify đúng | 25 |
| ML-KEM encaps/decaps đúng | 20 |
| Robust key handling & formats | 15 |
| Certificate implementation | 10 |
| Negative tests & UX | 10 |
| Performance study & analysis | 15 |
| Cross-platform build & documentation | 5 |
| Report quality | 10 |
| Bonus (advanced implementation) | +15 |

> *(Ghi chú: tổng các thành phần ở trên là **110**, không phải 100 như các lab khác. Xem [mục 10](#10-điểm-mơ-hồ--mâu-thuẫn-trong-đề-cần-hỏi-giảng-viên).)*

Learning impact: vì sao chữ ký PQ lớn · mật mã lattice ở mức tổng quan · ML-KEM thay thế trao đổi khóa RSA thế nào · chiến lược chuyển sang PQ TLS · đánh đổi kỹ thuật thực tế.

---

## 9. Báo cáo: mẫu, protocol đo, cam kết liêm chính, bảng rubric

### 9.1. Mẫu báo cáo (Reporting Template — "use & adapt")

1. **Objective & outcomes** — đã xây dựng gì và vì sao.
2. **Design & security** — tham số, lựa chọn mode, các kiểm tra chống dùng sai.
3. **Test methodology** — vector đã dùng (**phải trích dẫn nguồn**), negative tests.
4. **Performance** — bảng phần cứng (CPU, RAM, OS, cờ compiler), protocol, biểu đồ (throughput MB/s; latency ms/op), **khoảng tin cậy 95%**.
5. **Discussion** — lỗi đúng-đắn đã phát hiện và sửa; so sánh giữa các OS; đánh đổi bảo mật.
6. **Conclusion** — bài học chính và hướng phát triển.
- **Appendix** — ví dụ CLI; build log; độ phủ KAT; tóm tắt unit test.

### 9.2. Checklist nội dung báo cáo (gom từ mục 2.9, 2.8 và từng lab)

Với **mỗi lab**, báo cáo phải có:

- [ ] Objective & outcomes
- [ ] Implementation details (thuật toán, tham số, định dạng, cấu trúc code)
- [ ] Test results: KAT (nguồn vector được trích dẫn), negative tests, kết quả `ctest`
- [ ] Performance methodology + **bảng phần cứng/OS**
- [ ] **Bảng thống kê đủ mean / median / SD / 95% CI** và **biểu đồ có error bar**
- [ ] Phân tích so sánh theo yêu cầu riêng của lab (Windows vs Linux, mode, AEAD...) **có diễn giải**
- [ ] Security analysis: threat model · misuse scenarios · known attacks · secure defaults · fail-closed · limitations
- [ ] Cross-platform comparison
- [ ] Lessons learned
- [ ] Conclusion & future work
- [ ] Appendix (CLI, build log, KAT coverage, unit test summary)
- [ ] **Bảng rubric tự chấm điểm** (Appendix B)
- [ ] **Academic integrity statement + khai báo AI/công cụ** (Appendix A)
- [ ] **Trích dẫn tài liệu tham khảo** (NIST, RFC, paper, thư viện...)

### 9.3. Appendix A — Academic Integrity & Ethics (dán vào mỗi báo cáo)

- Bài làm là của chính bạn; **trích dẫn thư viện, nguồn KAT và tài liệu tham khảo**.
- Phần minh họa tấn công **chỉ giới hạn trên artifact thử nghiệm của chính bạn, trong sandbox offline**.
- **Không thử nghiệm trên dữ liệu, mạng hay dịch vụ của bên thứ ba.**
- **Khóa và bí mật có trong repo chỉ là khóa thử nghiệm (test keys).**
- Bạn hiểu sự khác biệt giữa **minh họa lỗ hổng** và **khai thác ác ý**, và cam kết chỉ làm điều thứ nhất, hoàn toàn vì mục đích học tập.

Ngoài ra (mục 9 của đề): **khai báo việc dùng AI/công cụ nếu có**, và **không sao chép cài đặt bên ngoài**.

### 9.4. Appendix B — Bảng rubric (đưa vào và tự chấm)

| Tiêu chí | Điểm | Self-score | TA score |
|---|---|---|---|
| Correctness & KATs | 35–40 | | |
| Security hygiene / misuse checks | 10–20 | | |
| Cross-platform build & UX | 10 | | |
| Performance study quality | 10–20 | | |
| Report quality & clarity | 5–10 | | |
| Negative tests | 10–15 | | |
| Bonuses (nếu có) | + tối đa 15 | | |
| **Tổng** | **100 (+bonus)** | | |

---

## 10. Điểm mơ hồ / mâu thuẫn trong đề cần hỏi giảng viên

*(Phần này do người tổng hợp rút ra khi đọc đề; không phải nguyên văn đề.)*

| # | Điểm | Chi tiết | Hướng xử lý đề xuất |
|---|---|---|---|
| 1 | **Rubric Lab 6 cộng lại 110** | 25+20+15+10+10+15+5+10 = 110, trong khi các lab khác đúng 100 | Hỏi lại; vẫn làm đủ mọi thành phần |
| 2 | **Số lần lặp: "30–100" và "N ≥ 30"** | Mục 4 ghi 30–100; Performance Protocol ghi N ≥ 30 | Dùng **30–100** (thỏa cả hai) |
| 3 | **Tham số IV/nonce** | CLI chuẩn ghi `--iv IV-hex`, `--nonce NONCE-hex`; Lab 1 ghi `--iv IVFILE`, `--nonce NONCEFILE`; Lab 2 ghi `--iv iv.bin` | Hỗ trợ **cả file lẫn hex** cho `--iv`/`--nonce` |
| 4 | **Thư viện ngẫu nhiên** | Đề ghi "Must use: Crypto++ AutoSeededRandomPool / OpenSSL RAND_bytes" nhưng Lab 2 cấm thư viện mật mã ngoài trong lõi AES | Lab 1 → `AutoSeededRandomPool`; Lab 2 → `RAND_bytes` chỉ cho sinh khóa/IV (không thuộc lõi AES) |
| 5 | **Bonus +5 pts** trong phần bắt buộc | ECDSA-P384 (+5), ML-DSA-65 (+5) được ghi "+5 pts" nhưng Lab 5/6 cũng có "+15 Bonus" | Hỏi xem hai mục này nằm trong hay ngoài +15 |
| 6 | **GUI "PySide6/Qt"** | Đề ghi PySide6; nhiều bài dùng PyQt6 | Hỏi PyQt6 có được chấp nhận không |
| 7 | **"Gọi cùng thư viện đã biên dịch"** | GUI gọi CLI qua tiến trình con có tính là "gọi thư viện" không? | Hỏi; an toàn nhất là export `.dll/.so` + API C và gọi bằng `ctypes` |
| 8 | **ML-DSA / ML-KEM dùng thư viện nào** | Đề Lab 6 không nêu thư viện; cũng ghi "implement" | Hỏi thư viện được phép (liboqs? OpenSSL 3.5+?) |
| 9 | **Lab 2: "Crypto++ AES-128-CTROR"** | Có vẻ là lỗi gõ của "CTR" | Hiểu là Crypto++ AES-128-CTR |
| 10 | **"1000 rounds per block"** | Câu trong Performance Protocol viết lệch ngữ pháp | Hiểu là **khối ~1000 thao tác** như mục 4 |
| 11 | **Cột "Equivalent Classical" của ML-KEM bỏ trống** | Đề không điền | Tự tra tài liệu NIST (FIPS 203) và trích dẫn |
| 12 | **Độ dài báo cáo ~60 trang** | Đề ghi "typical" | Coi là mục tiêu tối thiểu thực tế; kiểm tra bằng cách xuất PDF |
| 13 | **Một báo cáo chung vs "attach to each lab ZIP"** | Mục 7 yêu cầu **một file duy nhất cho cả 6 lab**; cuối đề lại ghi submission checklist "attach to each lab ZIP" | Làm **một gói `MSV.All6labs.rar`** có báo cáo chung; mỗi lab vẫn có README và checklist riêng bên trong |

---

## 11. Checklist tổng trước khi nộp

### 11.1. Gói nộp

- [ ] File nén duy nhất `MSV.All6labs.rar` (MSV = mã số sinh viên)
- [ ] Có source của **cả 6 lab**
- [ ] Có `README.md` top-level + README từng lab
- [ ] Có **báo cáo PDF/DOCX duy nhất ~60 trang**
- [ ] Có **binary Windows (MSVC và/hoặc MinGW) và Linux** build từ **code cuối cùng**
- [ ] Có **bảng tự chấm điểm** và **cam kết liêm chính** (kèm khai báo AI/công cụ)
- [ ] Có file KAT và mọi resource cần thiết
- [ ] Không có thư mục build tạm, khóa thật, dữ liệu cá nhân

### 11.2. Build và test (mỗi lab)

- [ ] `mkdir build && cd build && cmake .. && cmake --build .` chạy được **mà không sửa đường dẫn**
- [ ] Biên dịch được trên **Ubuntu LTS**, **Windows MSVC**, **Windows MinGW64**
- [ ] `ctest` pass trên **Windows và Linux**
- [ ] `--kat vectors.json` in PASS/FAIL từng test + tổng kết
- [ ] Có test: wrong key · tampered ciphertext · tampered tag (AEAD) · malformed input · corrupted PEM/key (nếu có)
- [ ] Mọi lỗi đầu vào **fail closed** (không ghi file đầu ra dở dang, không rò rỉ thông tin)

### 11.3. Rà soát theo chuẩn chung

- [ ] CLI theo chuẩn thống nhất; nhận/xuất **UTF-8**
- [ ] Màn hình mặc định **hex**; file mặc định **raw**; có `--encode hex|base64|raw`
- [ ] **Không** dùng `rand()`/`std::random` cho mật mã; dùng `AutoSeededRandomPool`/`RAND_bytes`
- [ ] Kiểm tra độ dài IV; chống tái dùng nonce; auto-generate; có tài liệu định dạng header/sidecar
- [ ] Cỡ khóa: AES 128/192/256 · RSA ≥ 3072 · ECDSA ≥ P-256
- [ ] Hiệu năng: warm-up 1–2 s · khối ~1000 thao tác · lặp 30–100 · **mean, median, SD, 95% CI** · bảng phần cứng · governor ghim · biểu đồ có error bar · diễn giải
- [ ] Phần so sánh: Windows vs Linux · biến thể thuật toán · overhead mode · tăng tốc phần cứng
- [ ] Báo cáo có đủ **threat model · misuse · known attacks · secure defaults · fail-closed · limitations**
- [ ] Báo cáo có **cross-platform comparison** và **lessons learned**
- [ ] Trích dẫn nguồn (NIST, RFC, vector KAT, thư viện); không sao chép cài đặt bên ngoài
- [ ] Phần tấn công chỉ trên artifact tự tạo, offline

### 11.4. Tóm tắt điểm tối đa theo lab

| Lab | Điểm cốt lõi | Bonus tối đa |
|---|---:|---:|
| 1 | 100 | +5 (library export + GUI) |
| 2 | 100 | +20 (AES-192/256 +10, XTS +5, AES-NI/bit-sliced +5) |
| 3 | 100 | +15 (manual OAEP +5, hybrid analysis +10) |
| 4 | 100 | +15 (custom length-extension +5, chosen-prefix cert +10) |
| 5 | 100 | +15 (advanced implementation) |
| 6 | 100 (đề cộng ra 110) | +15 (advanced implementation) |

> *(Ghi chú: Appendix B của đề giới hạn bonus ở "+ tối đa 15" trong bảng rubric chung; điểm bonus Lab 2 (+20) và đề từng lab có thể được áp dụng theo đặc tả riêng của lab — "Bonus awarded per lab specification". Nên hỏi giảng viên cách cộng.)*
