# Lab 3 — RSA-OAEP và mã hóa lai (envelope) với Crypto++

`rsatool` là công cụ dòng lệnh cài đặt **RSA-OAEP (SHA-256, MGF1-SHA-256)** và **mã hóa lai**: khóa AES-256 ngẫu nhiên mã hóa dữ liệu bằng AES-256-GCM, khóa AES được bọc
bằng RSA-OAEP. Bản rõ ngắn (≤ 318 byte với RSA-3072, ≤ 446 byte với RSA-4096) dùng RSA-OAEP trực tiếp; dài hơn thì tự chuyển sang envelope. `rsabench` đo hiệu năng.

> **Cảnh báo:** đây là bài thực hành. Không dùng để bảo vệ dữ liệu thật; khóa trong bài chỉ là khóa thử.

## 1. Tính năng

- RSA-3072 (bắt buộc) và RSA-4096 (so sánh), e = 65537; cỡ khóa khác bị từ chối. OAEP có label tùy chọn.
- Khóa: PEM (SPKI / PKCS#8), DER và metadata JSON `{"creation_time","modulus_bits","hash"}`.
- Envelope JSON: `mode`, `rsa_modulus`, `hash`, `wrapped_key`, `iv` (96 bit), `tag` (128 bit); header được ràng buộc vào GCM bằng AAD.
- Mã hóa ciphertext: raw, hex hoặc base64 (`--encode`).
- Fail closed: mọi lỗi giải mã in **một** thông báo chung (`Error: decryption failed.`), mã thoát 3, không để lại tệp đầu ra.
- KAT từ nguồn chính thức (Project Wycheproof, NIST CAVP): 124 vector. Unit test Catch2 (45 test case) và test CLI end-to-end qua `ctest`.

## 2. Phụ thuộc

| Thành phần | Phiên bản đã kiểm chứng | Ghi chú |
|---|---|---|
| CMake | >= 3.20 | |
| Compiler C++17 | GCC 13.3 (Ubuntu 24.04), MinGW-w64 GCC 16.2, MSVC 19.51 | |
| **Crypto++** | 8.9.0 | thư viện mật mã duy nhất |
| nlohmann_json | 3.11.3 trở lên | phân tích header envelope và tệp KAT; nếu chưa cài, CMake tự tải khi cấu hình |
| Catch2 | v3 | chỉ cho test; thiếu thì CMake tự tải v3.5.3 |
| Python 3 + matplotlib | 3.x | tùy chọn: `scripts/tamper.py`, `scripts/analyze_bench.py` (đồ thị cần matplotlib) |
| openssl (dòng lệnh) | bất kỳ | tùy chọn: phép thử tương tác và khóa 2048 bit trong `docs/TEST_EVIDENCE.md` |

### Cài đặt

```bash
# Ubuntu 24.04
sudo apt install -y build-essential cmake git ninja-build libcrypto++-dev nlohmann-json3-dev catch2 openssl python3-matplotlib
# Windows, MinGW-w64 (MSYS2 MINGW64 shell)
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-crypto++ mingw-w64-x86_64-nlohmann-json mingw-w64-x86_64-catch
```

Windows MSVC: `vcpkg install cryptopp:x64-windows-static` (nlohmann_json và Catch2 được CMake tự tải nếu chưa cài).

## 3. Build (out-of-source)

```bash
mkdir build && cd build
cmake ..
cmake --build .
ctest --output-on-failure
```

MSVC (trong "x64 Native Tools Command Prompt", binary tĩnh):

```bat
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=cl -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
      -DVCPKG_TARGET_TRIPLET=x64-windows-static -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DLAB3_STATIC_RUNTIME=ON
cmake --build . && ctest --output-on-failure
```

Tùy chọn: `-DLAB3_STATIC_RUNTIME=ON` (binary tĩnh cho `release/`), `-DLAB3_WERROR=ON` (warning thành lỗi, cho phát triển), `-DBUILD_TESTS=OFF`.
Với MSVC + Ninja luôn truyền `-DCMAKE_BUILD_TYPE=Release`. CMake không có đường dẫn cứng: Crypto++ được tìm tự động (`../cmake/FindCryptoPP.cmake`), và gói của MSYS2/WSL trên `PATH` không bị nhặt nhầm khi build bằng MSVC hoặc Linux.

Binary dựng sẵn: `release/windows/rsatool_{mingw,msvc}.exe`, `rsabench_{mingw,msvc}.exe` (tĩnh), `release/ubuntu/rsatool_gcc`, `rsabench_gcc`.

## 4. Sử dụng

```bash
rsatool keygen --bits 3072 --pub pub.pem --priv priv.pem          # ghi pub.pem pub.der priv.pem priv.der priv.meta.json
rsatool encrypt --in msg.bin --pub pub.pem --out ct.bin           # tự chọn direct hay hybrid
rsatool encrypt --in msg.bin --pub pub.pem --out ct.bin --label "project-x"
rsatool encrypt --in msg.bin --pub pub.pem --out ct.hex --encode hex
rsatool encrypt --in msg.bin --pub pub.pem --out ct.bin --force-hybrid --verbose
rsatool decrypt --in ct.bin --priv priv.pem --out msg.bin --label "project-x"
rsatool --kat kat/vectors.json
```

| Tùy chọn | Ý nghĩa |
|---|---|
| `--bits 3072\|4096` | cỡ khóa (mặc định 3072) |
| `--pub`, `--priv` | khóa công khai / riêng (PEM hoặc DER, tự nhận) |
| `--in FILE` / `--text "..."` | dữ liệu vào (UTF-8 với `--text`) |
| `--out FILE` | tệp ra; không có `--out` thì ciphertext in ra màn hình dạng hex |
| `--label TEXT` / `--label-hex HEX` | label OAEP; khi giải mã phải truyền đúng label |
| `--encode raw\|hex\|base64` | định dạng tệp ciphertext (mặc định raw) |
| `--force-hybrid` | ép dùng envelope |
| `--force` | cho phép ghi đè tệp đã có |
| `--verbose` / `--quiet` | in chế độ và kích thước / tắt thông báo thành công |

Mã thoát: `0` thành công, `1` sai tham số, `2` lỗi I/O hoặc khóa không đọc được / không hỗ trợ, `3` giải mã thất bại, `4` KAT thất bại.

## 5. Định dạng

Container: `"RSAE" | 0x01 | header_len (u32 BE) | header JSON | payload`. Chi tiết, AAD, thứ tự giải mã và giới hạn: [docs/DESIGN.md](docs/DESIGN.md).

## 6. Kiểm thử và bằng chứng

- `ctest`: unit test (45 test case, 855 assertion), KAT (124 vector), CLI end-to-end (38 kiểm tra).
- Nguồn KAT chính thức và cách tái tạo: [kat/SOURCES.md](kat/SOURCES.md). Lệnh sinh log bằng chứng: [docs/TEST_EVIDENCE.md](docs/TEST_EVIDENCE.md).
- Test âm tính bắt buộc của đề đều có: sửa bản mã RSA, sửa bản mã/tag AES-GCM, sai khóa riêng, sai label, sửa header; cùng nhiều biến thể (cắt cụt, thêm byte, sai magic/version, mã hóa hỏng, khóa không hỗ trợ).

## 7. Benchmark

`rsabench` đo sinh khóa RSA, RSA-OAEP, AES-256-GCM và mã hóa lai (1 KiB / 1 MiB / 100 MiB) và ghi CSV; `scripts/analyze_bench.py` tính mean, median, SD, CI 95% và vẽ biểu đồ.
Các lệnh chạy và protocol: [docs/BENCHMARK_RUNBOOK.md](docs/BENCHMARK_RUNBOOK.md).

## 8. Cấu trúc thư mục

```text
lab3/
├── CMakeLists.txt
├── src/            rsa_core (keyio, rsa_oaep, envelope, hybrid, kat, encoding, util) + cli + main
├── bench/          rsabench
├── tests/          Catch2 (encoding, rsa_oaep, keyio, hybrid, negative, kat) + cli_e2e.cmake
├── kat/            vectors.json, SOURCES.md, sources/ (tệp Wycheproof gốc)
├── scripts/        make_kat.py, tamper.py, analyze_bench.py
├── docs/           DESIGN, TEST_EVIDENCE, BENCHMARK_RUNBOOK, UBUNTU_FINISH, REQUIREMENTS_CHECKLIST
├── release/        binary tĩnh Windows / Ubuntu
├── output/         log bằng chứng, CSV benchmark, biểu đồ theo hệ điều hành
└── reference_inclass/  bài tập trên lớp bằng OpenSSL (tham khảo, không phải bài nộp)
```

## 9. Ghi chú bảo mật và giới hạn

- Ngẫu nhiên: `AutoSeededRandomPool`; không `rand()` / `std::random`. Mỗi lần mã hóa lai sinh khóa AES và IV mới.
- Không in khóa, bản rõ, label hay IV. Tệp khóa riêng 0600 trên Linux. Khóa riêng không nên đặt trong repo.
- Tính constant-time của OAEP và so sánh tag là của Crypto++; bài này không tự cài lại. Bộ phân tích header chạy trước RSA nên thời gian từ chối khác thời gian giải mã (thông điệp và mã thoát giống nhau).
- Hybrid đọc cả tệp vào RAM (tối đa 1 GiB); chưa có chế độ streaming. Bonus (OAEP thủ công, phân tích bảo mật hybrid) **không** được làm.
- Windows Clang chưa được kiểm chứng.
