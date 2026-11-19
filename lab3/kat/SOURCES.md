# Nguồn của các Known Answer Tests (chính thức)

`vectors.json` được sinh bởi `scripts/make_kat.py` từ hai nguồn dưới đây. Vector được chép **nguyên văn**, không sửa tay; script kiểm tra SHA-256 của tệp nguồn
đúng bằng giá trị đã ghim, nên bộ KAT luôn tái lập được.

## 1. RSAES-OAEP, RSA-3072, SHA-256, MGF1-SHA-256 (có label)

- Nguồn: **Project Wycheproof** (do Google khởi xướng, hiện được C2SP duy trì).
- Trang: <https://github.com/C2SP/wycheproof/blob/main/testvectors_v1/rsa_oaep_3072_sha256_mgf1sha256_test.json>
- Tệp thô: <https://raw.githubusercontent.com/C2SP/wycheproof/main/testvectors_v1/rsa_oaep_3072_sha256_mgf1sha256_test.json>
- SHA-256 của tệp đã dùng: `c8122302f522bfd539650c71c4aa966bd1f89acb50c6f4c190030fdb9b2684a4` (55 686 byte; bản sao ở `sources/`).
- Nội dung: 1 khóa RSA-3072 (PKCS#8) và 37 vector: **18 hợp lệ** (phải giải mã ra đúng `msg`, có cả label rỗng và label khác rỗng) và **19 không hợp lệ**
  (phải bị từ chối: sai padding, sai label, bản mã sai độ dài...). Các vector có kết quả `acceptable` (nếu có) không được dùng làm tiêu chí đạt/không đạt.
- Tiêu chuẩn thuật toán: RFC 8017 (PKCS #1 v2.2), mục 7.1, <https://www.rfc-editor.org/rfc/rfc8017>.

## 2. AES-256-GCM (NIST CAVP)

- Nguồn: **NIST Cryptographic Algorithm Validation Program**, mục "Block Cipher Modes → GCM":
  <https://csrc.nist.gov/projects/cryptographic-algorithm-validation-program/cavp-testing-block-cipher-modes>
- Tệp: <https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Algorithm-Validation-Program/documents/mac/gcmtestvectors.zip>
  (CAVS 14.0, 9 154 174 byte, SHA-256 `f9fc479e134cde2980b3bb7cddbcb567b2cd96fd753835243ed067699f26a023`).
- Dùng: `gcmEncryptExtIV256.rsp` (khóa 256 bit, IV 96 bit, tag 128 bit: lấy 3 ca đầu của mỗi nhóm độ dài PT/AAD = **75 ca**, PT 0 đến 408 bit, AAD 0 đến 720 bit)
  và `gcmDecrypt256.rsp` (**12 ca `FAIL`**: xác thực phải thất bại).
- Tiêu chuẩn thuật toán: NIST SP 800-38D, <https://csrc.nist.gov/pubs/sp/800/38/d/final>.

## 3. Tái tạo

```bash
python3 scripts/make_kat.py --wycheproof <đường dẫn tệp Wycheproof> --gcm-zip <đường dẫn gcmtestvectors.zip> --out kat/vectors.json
```

## 4. Ghi chú về phạm vi

- RSA-OAEP là mã hóa ngẫu nhiên nên KAT chỉ kiểm **giải mã** bản mã có sẵn (đúng cách Wycheproof định nghĩa); phía **mã hóa** được kiểm bằng unit test round-trip và bằng
  tương tác với OpenSSL (OpenSSL giải mã bản mã do Crypto++ tạo, có label; xem `docs/TEST_EVIDENCE.md` mục K).
- Chưa tìm thấy vector chính thức cho định dạng envelope riêng của bài (JSON header, AAD): phần này được kiểm bằng unit test và test âm tính.
