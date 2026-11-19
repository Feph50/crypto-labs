# Lab 3 — Việc cần làm trên Ubuntu 24.04

File này dành cho người chạy trên **Ubuntu 24.04 LTS cài trực tiếp** (cùng máy với Windows; **không dùng WSL2**). Nếu bạn nhờ Claude hướng dẫn, hãy bảo Claude đọc file này và làm từng bước,
chạy lệnh, đối chiếu "Kết quả mong đợi" rồi báo cáo kết quả. Chạy từ thư mục `lab3` của repo, trừ khi ghi khác.

## Bối cảnh (để người hướng dẫn nắm)

- Lab 3: RSA-OAEP (SHA-256, MGF1-SHA-256) và mã hóa lai (RSA-OAEP bọc khóa AES-256, AES-256-GCM) bằng **Crypto++**; công cụ `rsatool` và `rsabench`.
- Đã có trên repo: mã nguồn, bộ KAT chính thức (`kat/vectors.json`), binary và log Windows (`output/windows/`).
- **Việc của file này**: build, kiểm thử, 4 log bằng chứng của Ubuntu, binary release tĩnh và **benchmark Ubuntu**.
- Không sửa mã nguồn. Nếu một kết quả không khớp "mong đợi", **dừng lại và báo nguyên văn output**, đừng tự sửa hay bỏ qua.

## Bước 0 (làm trên Windows trước, nếu chưa làm)

Commit và push để Ubuntu lấy được bản mới nhất:

```powershell
cd "D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs-Lnguyen"
git add -A
git commit -m "Lab 3: rsatool, rsabench, tests, KAT, docs, Windows logs"
git push origin main
```

## Bước 1: Cài thư viện và lấy mã

```bash
sudo apt update
sudo apt install -y build-essential cmake git ninja-build libcrypto++-dev nlohmann-json3-dev catch2 openssl python3-matplotlib
cd ~/crypto-labs && git pull
git rev-parse --short HEAD
cd lab3
```

(`nlohmann-json3-dev` và `catch2` không bắt buộc: nếu thiếu, CMake tự tải khi cấu hình, cần mạng một lần.)

## Bước 2: Build và `ctest`

```bash
mkdir -p build_ubuntu_gcc && cd build_ubuntu_gcc
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
ctest --output-on-failure
cd ..
```

Kết quả mong đợi: `100% tests passed, 0 tests failed out of 3`.

## Bước 3: Log KAT, ctest và môi trường

Làm đúng các lệnh ở `docs/TEST_EVIDENCE.md`:

- mục 3: log KAT (`output/ubuntu/kat_ubuntu_gcc.txt`; mong đợi `Summary: 124 tests, 124 passed, 0 failed`) và log `ctest` (`output/ubuntu/ctest_ubuntu_gcc.txt`; mong đợi `100% tests passed, 0 tests failed out of 3` và `All tests passed (855 assertions in 45 test cases)`);
- mục 5: `output/ubuntu/env_ubuntu.txt`.

## Bước 4: Log test âm tính

Mở `docs/TEST_EVIDENCE.md`, **mục 4**, lấy **nguyên khối lệnh** trong khung ```bash nằm ngay sau dòng `<!-- NEG-BLOCK -->` và chạy trong thư mục `lab3`
(đã đặt sẵn `T=./build_ubuntu_gcc/rsatool`, `PY=python3`, `OUT=output/ubuntu/negative_ubuntu_gcc.txt`; cần `openssl` và `python3`). Sau khi chạy xong:

```bash
rm -r neg_tmp
grep -h "^\[exit code" output/ubuntu/negative_ubuntu_gcc.txt | sort | uniq -c
grep -c "WRITTEN (BAD)" output/ubuntu/negative_ubuntu_gcc.txt
```

Kết quả mong đợi (giống hệt Windows): `29 [exit code: 0]`, `3 [exit code: 1]`, `10 [exit code: 2]`, `20 [exit code: 3]`; số dòng `WRITTEN (BAD)` là **0**.

## Bước 5: Binary release tĩnh dựng trên Ubuntu 24.04

```bash
mkdir -p release/ubuntu
mkdir build_rel && cd build_rel
cmake .. -DCMAKE_BUILD_TYPE=Release -DLAB3_STATIC_RUNTIME=ON
cmake --build .
ctest --output-on-failure
cp rsatool ../release/ubuntu/rsatool_gcc
cp rsabench ../release/ubuntu/rsabench_gcc
cd ..
file release/ubuntu/rsatool_gcc | cut -c1-140
./release/ubuntu/rsatool_gcc --kat kat/vectors.json | tail -2
rm -r build_rel
```

Kết quả mong đợi: `ctest` 3/3; `file` ghi `statically linked`; dòng KAT cuối `124 passed, 0 failed`.

## Bước 6: Benchmark Ubuntu

Làm theo `docs/BENCHMARK_RUNBOOK.md` **mục 3** (governor `performance`, cắm điện, 4 lệnh `rsabench` với `--reps 30`), cùng lệnh như đã chạy trên Windows.
Mất khoảng 20 đến 25 phút (ước tính). Không chạy việc khác trong lúc đo.

## Bước 7: Push

```bash
cd ~/crypto-labs
git add lab3/output/ubuntu lab3/release/ubuntu
git status --short
git commit -m "Lab 3: Ubuntu 24.04 logs, release binaries and benchmark"
git push origin main
```

## Báo lại cho người làm báo cáo (trên Windows)

1. Kết quả `ctest` ở bước 2 và bước 5 (3/3 hay không).
2. Dòng tổng kết của KAT; đếm mã thoát của test âm tính và số dòng `WRITTEN (BAD)`.
3. Dòng `file release/ubuntu/rsatool_gcc`.
4. Commit đã push (`git rev-parse --short HEAD`).
5. Bất kỳ chỗ nào lệch "mong đợi", kèm output nguyên văn.

Sau đó trên Windows chạy `git pull` rồi báo để sinh biểu đồ, bảng thống kê và báo cáo.
