# Lab 3 — Thiết kế: RSA-OAEP và mã hóa lai (envelope)

## 1. Tham số cố định

| Hạng mục | Giá trị |
|---|---|
| Thư viện mật mã | **Crypto++ 8.9** (không dùng OpenSSL trong lõi; OpenSSL chỉ là công cụ đối chiếu trong test) |
| RSA | 3072 bit (bắt buộc) và 4096 bit (so sánh hiệu năng); e = 65537; các cỡ khác bị từ chối |
| OAEP | hash SHA-256, MGF1 dùng SHA-256, label tùy chọn (`--label` / `--label-hex`) |
| Giới hạn bản rõ trực tiếp | `mLen ≤ k − 2·hLen − 2`, k = số byte của modulus, hLen = 32: **318 byte** (3072), **446 byte** (4096) |
| Mã hóa lai | khóa AES-256 ngẫu nhiên, AES-256-GCM, IV 96 bit, tag 128 bit, khóa được bọc bằng RSA-OAEP |
| Ngẫu nhiên | `CryptoPP::AutoSeededRandomPool` (khóa AES, IV, nhân tố OAEP, khóa RSA); không `rand()`/`std::random` |
| Chuẩn | RFC 8017 (RSAES-OAEP), NIST SP 800-38D (GCM) |

## 2. Cấu trúc mã nguồn

| Tệp | Vai trò |
|---|---|
| `src/keyio.*` | Sinh khóa; PEM (SPKI / PKCS#8) tự viết, DER, metadata JSON; nạp và kiểm khóa |
| `src/rsa_oaep.*` | RSA-OAEP SHA-256 (`oaep_encrypt`, `oaep_decrypt`, `max_plaintext_len`) |
| `src/envelope.*` | Container và header JSON, AAD; bộ phân tích chặt |
| `src/hybrid.*` | `encrypt_message` (tự chọn direct/hybrid), `decrypt_message`, AES-256-GCM |
| `src/kat.*` | Chạy `kat/vectors.json` |
| `src/encoding.*`, `src/util.*`, `src/errors.hpp` | hex/base64 chặt, I/O nguyên tử, mã lỗi |
| `src/cli.*`, `src/main.cpp` | công cụ `rsatool` |
| `bench/bench_main.cpp` | `rsabench` |

## 3. Quản lý khóa

`rsatool keygen --bits 3072 --pub pub.pem --priv priv.pem` ghi 5 tệp:

| Tệp | Nội dung |
|---|---|
| `pub.pem` | khóa công khai, **SPKI** (`-----BEGIN PUBLIC KEY-----`) |
| `priv.pem` | khóa riêng, **PKCS#8** (`-----BEGIN PRIVATE KEY-----`) |
| `pub.der`, `priv.der` | cùng khóa ở dạng DER (đổi đuôi `.pem` thành `.der`; không có `.pem` thì thêm `.der`) |
| `priv.meta.json` | `{"creation_time":"<UTC ISO-8601>","modulus_bits":3072,"hash":"SHA-256"}` |

- Tệp riêng được ghi với quyền 0600 trên Linux. Mọi tệp được ghi qua `<tên>.tmp` rồi đổi tên (không để lại tệp dở); từ chối ghi đè khi thiếu `--force`, và không ghi tệp nào nếu một đích đã tồn tại.
- PEM parser chặt: đúng nhãn BEGIN/END, base64 chuẩn (cột 64 khi ghi, chấp nhận CRLF khi đọc), không header kiểu `Proc-Type`, không còn gì sau END. Khóa PKCS#1 (`RSA PRIVATE KEY`) không được hỗ trợ.
- Nạp khóa tự nhận PEM hay DER theo nội dung; kiểm cấu trúc, kiểm tính hợp lệ của khóa (`Validate`), từ chối byte thừa, từ chối cỡ khóa khác 3072/4096. Lỗi khóa chỉ báo một câu chung ("invalid or unsupported key file"), mã thoát 2.

## 4. Container (dùng chung cho cả hai chế độ)

```
"RSAE" | version = 0x01 | header_len (u32, big-endian) | header (JSON UTF-8, ≤ 64 KiB) | payload
```

| Chế độ | Header | Payload |
|---|---|---|
| Direct | `{"mode":"RSA-OAEP","rsa_modulus":3072,"hash":"SHA-256"}` | bản mã RSA-OAEP (đúng k byte) |
| Hybrid | `{"mode":"RSA-OAEP-AES-GCM","rsa_modulus":3072,"hash":"SHA-256","wrapped_key":"<b64>","iv":"<b64>","tag":"<b64>"}` | bản mã AES-256-GCM (không gồm tag) |

Header hybrid có **đúng các trường của đề**. `wrapped_key` = RSA-OAEP(khóa AES 32 byte), k byte; `iv` 12 byte; `tag` 16 byte (base64 chuẩn).

**Header chỉ được chấp nhận ở dạng chuẩn tắc:** sau khi phân tích JSON, header được dựng lại theo đúng thứ tự khóa, không khoảng trắng, không trường lạ, rồi so sánh từng byte
với bản đọc được. Mọi khác biệt (khoảng trắng, đổi thứ tự, trường thêm, khóa trùng, base64 không chuẩn) đều bị từ chối.

### AAD (ràng buộc header vào GCM)

```
AAD = "RSAE-AAD-v1|" + mode + "|" + rsa_modulus + "|" + hash + "|" + wrapped_key_b64 + "|" + iv_b64
```

Tag không nằm trong AAD. Nhờ AAD, sửa `mode`, `rsa_modulus`, `hash`, `wrapped_key` hoặc `iv` đều làm xác thực thất bại; sửa `tag` hoặc payload thì GCM thất bại.

## 5. Luồng xử lý

**Mã hóa (`encrypt`)**: bản rõ ≤ giới hạn và không có `--force-hybrid` → Direct. Ngược lại → Hybrid: sinh khóa AES-256 và IV 96 bit ngẫu nhiên, bọc khóa bằng RSA-OAEP (cùng label),
AES-256-GCM với AAD ở trên, ghi container. Mỗi lần mã hóa đều sinh khóa AES và IV mới nên không có nguy cơ tái dùng IV của GCM.

**Giải mã (`decrypt`), thứ tự bắt buộc**:
1. Phân tích container nghiêm ngặt (magic, version, `header_len ≤ 65536`, JSON, dạng chuẩn tắc, mode hợp lệ, hash là SHA-256, `rsa_modulus` khớp khóa riêng, độ dài `wrapped_key`/`iv`/`tag`).
2. RSA-OAEP giải bọc khóa AES (label do người dùng truyền; label không lưu trong tệp).
3. AES-256-GCM giải mã có xác thực vào **bộ đệm RAM**; chỉ khi thẻ hợp lệ mới trả bản rõ và ghi tệp (ghi nguyên tử). Thất bại thì xóa bộ đệm và **không tạo tệp đầu ra**.
4. Tag do Crypto++ so sánh (constant-time); mã của bài không tự so sánh tag.

## 6. Xử lý lỗi (fail closed, không rò rỉ)

| Mã thoát | Ý nghĩa |
|---:|---|
| 0 | thành công |
| 1 | sai tham số / cỡ khóa không hỗ trợ khi sinh khóa |
| 2 | lỗi I/O, khóa không đọc được hoặc không hỗ trợ, tệp đầu ra đã tồn tại, KAT hỏng |
| 3 | **giải mã thất bại** |
| 4 | KAT thất bại |

Mọi lỗi giải mã (OAEP sai, sai label, sai khóa, tag sai, header sai, base64/hex hỏng, độ dài sai, cắt cụt...) in **đúng một thông báo**: `Error: decryption failed.` và thoát với mã 3,
không nêu nguyên nhân. Ngoại lệ bất ngờ trong lúc giải mã cũng được báo y như vậy. Khóa, bản rõ, label, IV không bao giờ được in; `--verbose` chỉ in chế độ và kích thước.

## 7. Giới hạn đã biết

- Bộ phân tích header chạy **trước** RSA nên một envelope sai định dạng bị từ chối nhanh hơn một envelope sai OAEP (khác biệt thời gian). Thông điệp và mã thoát vẫn giống hệt. Thời gian của phép RSA nằm trong thư viện, ngoài tầm kiểm soát.
- Crypto++ dùng làm nền cho RSA; tính constant-time của OAEP decoding là của thư viện, bài này không tự cài lại (phần bonus không làm).
- Hybrid đọc cả tệp vào RAM (giới hạn 1 GiB); chưa có chế độ streaming.
- Nhãn OAEP không được lưu trong envelope (đúng chủ ý: là tham số ngữ cảnh), nên người nhận phải biết label.
- `rsatool` không dùng `--threads` (không có tác vụ song song); `--threads` của chuẩn CLI chung chỉ áp dụng cho công cụ đo.
