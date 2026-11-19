# Lab 2 — Việc cuối cùng cần làm trên Ubuntu 24.04

File này dành cho người chạy trên **Ubuntu 24.04 LTS cài trực tiếp** (cùng máy với Windows; **không dùng WSL2**). Nếu bạn nhờ Claude hướng dẫn, hãy bảo Claude đọc file này và làm từng bước,
chạy lệnh, đối chiếu "Kết quả mong đợi" rồi báo cáo kết quả. Chạy từ thư mục `lab2` của repo, trừ khi ghi khác.

## Bối cảnh (để người hướng dẫn nắm)

- Lab 2: AES-128 tự cài bằng C++ thuần (CTR, FIPS-197). `aestool` không link thư viện crypto; `aestool_ref` có thêm OpenSSL và Crypto++ chỉ để kiểm chứng.
- Đã có sẵn trên repo: mã nguồn, benchmark Windows và Ubuntu (`output/*/bench/*.csv`), `output/ubuntu/env_ubuntu.txt`, log Windows, báo cáo `docs/lab2-main-report.pdf`.
- **Còn thiếu** (việc của file này): 4 log kiểm thử của Ubuntu (KAT, cross-validation, ctest, test âm tính) và binary release tĩnh dựng trên Ubuntu 24.04.
  Các log cũ tạo từ WSL2 đã bị xóa có chủ ý. **Không** chạy lại benchmark.
- Không sửa mã nguồn. Nếu một kết quả không khớp "mong đợi", **dừng lại và báo nguyên văn output**, đừng tự sửa hay bỏ qua.

## Bước 0 (làm trên Windows trước, nếu chưa làm)

Commit và push để Ubuntu lấy được bản mới nhất (tài liệu, báo cáo, nhánh code đã sửa):

```powershell
cd "D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs-Lnguyen"
git add -A
git commit -m "Lab 2: Ubuntu 24.04 only, report, plots, docs"
git push origin main
```

## Bước 1: Lấy mã và build

```bash
cd ~/crypto-labs && git pull
git rev-parse --short HEAD
cd lab2
sudo apt install -y build-essential cmake git ninja-build libssl-dev libcrypto++-dev catch2   # nếu chưa cài
mkdir -p build_ubuntu_gcc && cd build_ubuntu_gcc
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
ctest --output-on-failure
cd ..
```

Kết quả mong đợi: `100% tests passed, 0 tests failed out of 4`. Nếu `build_ubuntu_gcc` đã tồn tại từ lần benchmark trước, vẫn dùng được, nhưng phải build lại (`cmake --build build_ubuntu_gcc`) sau `git pull`.

## Bước 2: Ba log (KAT, cross-validation, ctest)

```bash
mkdir -p output/ubuntu
T=./build_ubuntu_gcc/aestool; R=./build_ubuntu_gcc/aestool_ref
{ echo "== Known Answer Tests: aestool (Ubuntu 24.04, GCC) =="; echo "date : $(date -u +%Y-%m-%dT%H:%M:%SZ)"; echo "git  : $(git rev-parse --short HEAD)"; echo "kat  : kat/vectors.json  sha256 $(sha256sum kat/vectors.json | cut -c1-64)"; echo; $T --kat kat/vectors.json; echo "[exit code: $?]"; } 2>&1 | tee output/ubuntu/kat_ubuntu_gcc.txt
```

Kết quả mong đợi: dòng `Summary: 31 tests, 31 passed, 0 failed` và `[exit code: 0]`.

```bash
{ echo "== Cross-validation vs OpenSSL and Crypto++: aestool_ref (Ubuntu 24.04, GCC) =="; echo "date : $(date -u +%Y-%m-%dT%H:%M:%SZ)"; echo "git  : $(git rev-parse --short HEAD)"; echo; $R xval --ref all --cases 200; echo "[exit code: $?]"; } 2>&1 | tee output/ubuntu/xval_ubuntu_gcc.txt
```

Kết quả mong đợi: `Summary: 1722 cross-validation cases, 0 failed` và `[exit code: 0]`. (Số ca có thể hơi khác nếu `--cases` khác, nhưng `0 failed` là bắt buộc.)

```bash
cd build_ubuntu_gcc
{ echo "== ctest: GCC on Ubuntu 24.04 (Release) =="; echo "date : $(date -u +%Y-%m-%dT%H:%M:%SZ)"; echo "git  : $(git rev-parse --short HEAD)"; echo "g++  : $(g++ --version | head -1)"; echo "cmake: $(cmake --version | head -1)"; echo "openssl : $(dpkg -s libssl-dev | grep ^Version)"; echo "cryptopp: $(dpkg -s libcrypto++-dev | grep ^Version)"; echo "catch2  : $(dpkg -s catch2 2>/dev/null | grep ^Version)"; echo; ctest --output-on-failure; echo; echo "-- unit tests (Catch2) summary:"; ./aes_tests | tail -3; } 2>&1 | tee ../output/ubuntu/ctest_ubuntu_gcc.txt
cd ..
```

Kết quả mong đợi: `100% tests passed, 0 tests failed out of 4` và `All tests passed (3276 assertions in 32 test cases)`.

## Bước 3: Log test âm tính

Mở `docs/TEST_EVIDENCE.md`, **mục 4 ("Test âm tính")**, lấy **nguyên khối lệnh** trong khung ```bash nằm ngay sau dòng `<!-- NEG-BLOCK -->` và chạy trong thư mục `lab2`.
Khối này đã đặt sẵn `T=./build_ubuntu_gcc/aestool`, `PY=python3`, `OUT=output/ubuntu/negative_ubuntu_gcc.txt`, tự tạo thư mục tạm `neg_tmp` và ghi log.

Sau khi chạy xong:

```bash
rm -r neg_tmp
grep -h "^\[exit code" output/ubuntu/negative_ubuntu_gcc.txt | sort | uniq -c
grep -c "WRITTEN (BAD)" output/ubuntu/negative_ubuntu_gcc.txt
```

Kết quả mong đợi (giống hệt Windows): `14 [exit code: 0]`, `3 [exit code: 1]`, `4 [exit code: 2]`, `10 [exit code: 3]`, `4 [exit code: 4]`, `1 [exit code: 5]`; và số dòng `WRITTEN (BAD)` là **0**.

## Bước 4: Binary release tĩnh dựng trên Ubuntu 24.04

```bash
mkdir build_rel && cd build_rel
cmake .. -DCMAKE_BUILD_TYPE=Release -DLAB2_STATIC_RUNTIME=ON
cmake --build .
ctest --output-on-failure
cp aestool ../release/ubuntu/aestool_gcc
cd ..
file release/ubuntu/aestool_gcc | cut -c1-140
./release/ubuntu/aestool_gcc --kat kat/vectors.json | tail -2
rm -r build_rel
```

Kết quả mong đợi: `ctest` 4/4; `file` ghi `statically linked`; dòng KAT cuối `31 passed, 0 failed`.

## Bước 5: Push

```bash
cd ~/crypto-labs
git add lab2/output/ubuntu lab2/release/ubuntu
git status --short
git commit -m "Lab 2: Ubuntu 24.04 logs and release binary"
git push origin main
```

(Lần đầu cần `git config --global user.name "Tên"` và `git config --global user.email "email"`; GitHub yêu cầu token cá nhân khi push.)

## Báo lại cho người làm báo cáo (trên Windows)

Gửi các thông tin sau:

1. Kết quả `ctest` ở bước 1 và bước 4 (4/4 hay không).
2. Dòng tổng kết của KAT, cross-validation và đếm mã thoát của test âm tính (bước 2 và 3).
3. Dòng `file release/ubuntu/aestool_gcc`.
4. Commit đã push (`git rev-parse --short HEAD`).
5. Bất kỳ chỗ nào lệch "mong đợi", kèm output nguyên văn.

Sau đó trên Windows chạy `git pull` rồi báo để sinh lại báo cáo `docs/lab2-main-report.pdf`.
