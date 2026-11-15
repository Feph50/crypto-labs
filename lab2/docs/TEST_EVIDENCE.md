# Lab 2 — Bằng chứng kiểm thử (KAT, cross-validation, ctest, test âm tính, môi trường)

Tài liệu này mô tả các file log dùng làm bằng chứng cho báo cáo và **lệnh chính xác để tạo ra chúng**, để log Windows và Ubuntu có cùng nội dung và
cùng định dạng. Mọi lệnh gõ trực tiếp (bash / PowerShell), không cần file `.sh` hay `.bat`; lệnh chạy từ thư mục `lab2`.

## 1. Các file log

| File (trong `lab2/output/<os>/`) | Nội dung |
|---|---|
| `kat_<os>_<compiler>.txt` | `aestool --kat kat/vectors.json`: PASS/FAIL từng vector và tổng kết (31/31) |
| `xval_<os>_<compiler>.txt` | `aestool_ref xval --ref all`: so khớp ciphertext của bản tự cài với OpenSSL và Crypto++ |
| `ctest_<os>_<compiler>.txt` | `ctest` (4 test: unit, KAT, CLI end-to-end, cross-validation) kèm phiên bản compiler/CMake/thư viện và tóm tắt Catch2 |
| `negative_<os>_<compiler>.txt` | Các phép thử fail-closed của CLI (sai key, sai IV, ciphertext bị sửa, độ dài key/IV sai, tràn counter, ...) với mã thoát và thông báo |
| `env_<os>.txt` | Commit, SHA-256 của file KAT, phiên bản compiler/thư viện, phần cứng, hệ điều hành |

`<os>` = `windows` hoặc `ubuntu`; `<compiler>` = `mingw`, `msvc` hoặc `gcc`.

## 2. KAT và cross-validation

```bash
cd lab2
mkdir -p output/ubuntu
T=./build_ubuntu_gcc/aestool; R=./build_ubuntu_gcc/aestool_ref
{ echo "== Known Answer Tests: aestool (Ubuntu, GCC) =="; echo "date : $(date -u +%Y-%m-%dT%H:%M:%SZ)"; echo "git  : $(git rev-parse --short HEAD)"; echo "kat  : kat/vectors.json  sha256 $(sha256sum kat/vectors.json | cut -c1-64)"; echo; $T --kat kat/vectors.json; echo "[exit code: $?]"; } 2>&1 | tee output/ubuntu/kat_ubuntu_gcc.txt
{ echo "== Cross-validation vs OpenSSL and Crypto++: aestool_ref (Ubuntu, GCC) =="; echo "date : $(date -u +%Y-%m-%dT%H:%M:%SZ)"; echo "git  : $(git rev-parse --short HEAD)"; echo; $R xval --ref all --cases 200; echo "[exit code: $?]"; } 2>&1 | tee output/ubuntu/xval_ubuntu_gcc.txt
```

Windows: cùng hai lệnh với `T=./build_windows_gcc/aestool.exe` (MinGW) hoặc `T=./build_windows_msvc/aestool.exe` (MSVC), chạy trong Git Bash
(thay `sha256sum` bằng `certutil` nếu không có), log ghi vào `output/windows/kat_windows_mingw.txt` / `kat_windows_msvc.txt`, tương tự cho `xval_*`.

## 3. `ctest`

Build từ mã nguồn bằng đúng 4 lệnh của đề rồi chạy `ctest`:

```bash
cd lab2
mkdir -p build_ubuntu_gcc && cd build_ubuntu_gcc
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
{ echo "== ctest: GCC on Ubuntu (Release) =="; echo "date : $(date -u +%Y-%m-%dT%H:%M:%SZ)"; echo "git  : $(git rev-parse --short HEAD)"; echo "g++  : $(g++ --version | head -1)"; echo "cmake: $(cmake --version | head -1)"; echo "openssl : $(dpkg -s libssl-dev | grep ^Version)"; echo "cryptopp: $(dpkg -s libcrypto++-dev | grep ^Version)"; echo "catch2  : $(dpkg -s catch2 2>/dev/null | grep ^Version)"; echo; ctest --output-on-failure; echo; echo "-- unit tests (Catch2) summary:"; ./aes_tests | tail -3; } 2>&1 | tee ../output/ubuntu/ctest_ubuntu_gcc.txt
cd ..
```

Kết quả mong đợi: `100% tests passed, 0 tests failed out of 4`.

## 4. Test âm tính (fail-closed)

Dán nguyên khối sau (tự tạo thư mục tạm `neg_tmp`, ghi log vào `output/ubuntu/negative_ubuntu_gcc.txt`). Đổi `T`, `PY`, `OUT` cho hệ điều hành khác,
ví dụ Windows (Git Bash): `T=./build_windows_gcc/aestool.exe; PY=python; OUT=output/windows/negative_windows_mingw.txt`.

<!-- NEG-BLOCK -->
```bash
cd lab2
T=./build_ubuntu_gcc/aestool; PY=python3; OUT=output/ubuntu/negative_ubuntu_gcc.txt
mkdir -p neg_tmp output/ubuntu
W=neg_tmp
{
echo "== Lab 2 negative tests (fail-closed evidence) =="
echo "tool : $T"
echo "date : $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "git  : $(git rev-parse --short HEAD 2>/dev/null)"
echo
run() { echo "+ $*"; "$@"; echo "[exit code: $?]"; echo; }

echo "################ 0. Setup (test key and IVs, generated with the OS CSPRNG)"
run $T keygen --out $W/key.bin
run $T keygen --out $W/key_wrong.bin
run $T ivgen --out $W/iv.bin
run $T ivgen --out $W/iv_wrong.bin
$PY -c "open('$W/msg.bin','wb').write(b'Lab 2 CTR test message, 41 bytes long!!!!')"
$PY -c "import os;open('$W/key15.bin','wb').write(os.urandom(15));open('$W/iv15.bin','wb').write(os.urandom(15));open('$W/ivff.bin','wb').write(bytes([255])*16);open('$W/z33.bin','wb').write(bytes(33));open('$W/empty.bin','wb').write(b'')"

echo "################ A. Positive controls (spec command line, must succeed, exit code 0)"
run $T encrypt --mode ctr --key $W/key.bin --iv $W/iv.bin --in $W/msg.bin --out $W/ct.bin
run $T decrypt --mode ctr --key $W/key.bin --iv $W/iv.bin --in $W/ct.bin --out $W/dec.bin
run cmp $W/msg.bin $W/dec.bin
echo "--- A2. empty input is valid (no padding): 0 bytes in, 0 bytes out"
run $T encrypt --mode ctr --key $W/key.bin --iv $W/iv.bin --in $W/empty.bin --out $W/empty.ct
echo "--- A3. IV generated automatically, stored in the sidecar <out>.iv, picked up by decrypt"
run $T encrypt --key $W/key.bin --in $W/msg.bin --out $W/auto.ct
run $T decrypt --key $W/key.bin --in $W/auto.ct --out $W/auto.dec
run cmp $W/msg.bin $W/auto.dec

echo "################ B. Wrong key -> decryption 'succeeds' but the plaintext is wrong (CTR cannot detect it)"
run $T decrypt --mode ctr --key $W/key_wrong.bin --iv $W/iv.bin --in $W/ct.bin --out $W/wrongkey.dec
run cmp $W/msg.bin $W/wrongkey.dec
echo "(cmp exit code 1 = files differ = expected)"; echo

echo "################ C. Wrong IV -> wrong plaintext"
run $T decrypt --mode ctr --key $W/key.bin --iv $W/iv_wrong.bin --in $W/ct.bin --out $W/wrongiv.dec
run cmp $W/msg.bin $W/wrongiv.dec
echo "(cmp exit code 1 = files differ = expected)"; echo

echo "################ D. Tampered ciphertext -> corrupted plaintext, NOT detected (CTR is not authenticated)"
$PY -c "d=bytearray(open('$W/ct.bin','rb').read());d[5]^=0x01;open('$W/ct_bad.bin','wb').write(d)"
run $T decrypt --mode ctr --key $W/key.bin --iv $W/iv.bin --in $W/ct_bad.bin --out $W/ct_bad.dec
echo "cmp -l lists every differing byte: flipping ONE ciphertext bit flips exactly ONE plaintext bit (malleability)"
run cmp -l $W/msg.bin $W/ct_bad.dec

echo "################ E. Invalid key / IV -> rejected, exit code 3"
run $T encrypt --mode ctr --key $W/key15.bin --iv $W/iv.bin --in $W/msg.bin --out $W/e1.ct
run $T encrypt --mode ctr --key-hex 00112233 --iv $W/iv.bin --in $W/msg.bin --out $W/e2.ct
run $T encrypt --mode ctr --key-hex ZZ --iv $W/iv.bin --in $W/msg.bin --out $W/e3.ct
run $T encrypt --mode ctr --key $W/key.bin --iv $W/iv15.bin --in $W/msg.bin --out $W/e4.ct
run $T encrypt --mode ctr --key $W/key.bin --iv-hex 00 --in $W/msg.bin --out $W/e5.ct
run $T encrypt --mode ctr --key $W/key.bin --nonce 0011 --in $W/msg.bin --out $W/e6.ct
test -e $W/e1.ct -o -e $W/e2.ct -o -e $W/e3.ct -o -e $W/e4.ct -o -e $W/e5.ct -o -e $W/e6.ct && echo "RESULT: output file WRITTEN (BAD)" || echo "RESULT: no output file written (good)"; echo

echo "################ F. Counter overflow: IV = FF..FF and a message needing more than one block -> exit code 5, no partial output"
run $T encrypt --mode ctr --key $W/key.bin --iv $W/ivff.bin --in $W/z33.bin --out $W/overflow.ct
test -e $W/overflow.ct && echo "RESULT: output file WRITTEN (BAD)" || echo "RESULT: no output file written (good)"; echo

echo "################ G. File I/O errors -> exit code 4"
run $T encrypt --mode ctr --key $W/key.bin --iv $W/iv.bin --in $W/no_such_input.bin --out $W/g1.ct
run $T encrypt --mode ctr --key $W/no_such_key.bin --iv $W/iv.bin --in $W/msg.bin --out $W/g2.ct
echo "--- output already exists (no --force) and input == output"
run $T encrypt --mode ctr --key $W/key.bin --iv $W/iv.bin --in $W/msg.bin --out $W/ct.bin
run $T encrypt --mode ctr --key $W/key.bin --iv $W/iv.bin --in $W/msg.bin --out $W/msg.bin --force

echo "--- G3. a relative path is NOT searched in parent directories: ../key.bin exists, ./key.bin does not -> must fail"
TABS="$(cd "$(dirname "$T")" && pwd)/$(basename "$T")"
mkdir -p $W/sub
( cd $W/sub && run "$TABS" encrypt --mode ctr --key key.bin --iv iv.bin --in msg.bin --out x.ct )

echo "################ H. Misuse prevention"
echo "--- H1. --aead is rejected: CTR has no authentication tag"
run $T encrypt --mode ctr --aead --key $W/key.bin --iv $W/iv.bin --in $W/msg.bin --out $W/h1.ct
echo "--- H2. unknown mode / unknown backend"
run $T encrypt --mode des --key $W/key.bin --iv $W/iv.bin --in $W/msg.bin --out $W/h2.ct
run $T encrypt --mode ctr --sbox bogus --key $W/key.bin --iv $W/iv.bin --in $W/msg.bin --out $W/h3.ct
echo "--- H3. --nonce together with --iv"
run $T encrypt --mode ctr --key $W/key.bin --iv $W/iv.bin --nonce 000102030405060708090a0b --in $W/msg.bin --out $W/h4.ct
echo "--- H4. IV supplied by hand triggers the reuse warning (see the WARNING line above in section A)"

echo "################ I. Bonus modes fail closed too"
echo "--- I1. XTS input shorter than 16 bytes, XTS with K1 == K2"
run $T encrypt --mode xts --key-hex 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f --text short --out $W/i1.ct
run $T encrypt --mode xts --key-hex 0000000000000000000000000000000000000000000000000000000000000000 --iv $W/iv.bin --in $W/msg.bin --out $W/i2.ct

echo "################ J. Known Answer Test file that is not valid JSON must not report success"
echo "[ {\"name\": \"x\", \"cipher\": \"aes128\"" > $W/bad_kat.json
run $T --kat $W/bad_kat.json
echo "== end =="
} 2>&1 | tee "$OUT"
```

Sau khi chạy xong, xóa thư mục tạm và kiểm tra:

```bash
rm -r neg_tmp
grep -h "^\[exit code" output/ubuntu/negative_ubuntu_gcc.txt | sort | uniq -c    # mong đợi: 14 x 0, 3 x 1, 4 x 2, 10 x 3, 4 x 4, 1 x 5
grep -c "WRITTEN (BAD)" output/ubuntu/negative_ubuntu_gcc.txt                 # mong đợi: 0
```

Ý nghĩa mã thoát trong log: `0` thành công (kể cả các trường hợp "giải mã được nhưng sai" của CTR và lệnh `cmp` đối chứng), `1` chỉ xuất hiện ở lệnh
`cmp` (hai file khác nhau, đúng như mong đợi), `2` sai cú pháp/misuse (`--aead`, mode lạ...), `3` đầu vào không hợp lệ (key/IV/nonce sai độ dài, hex hỏng, JSON hỏng...),
`4` lỗi I/O (thiếu file, file đầu ra đã tồn tại, **không tìm file ở thư mục cha**), `5` tràn counter. Kết quả trên Windows MinGW, Windows MSVC và Ubuntu giống hệt nhau.

## 5. Môi trường

```bash
cd lab2
mkdir -p output/ubuntu
F=output/ubuntu/env_ubuntu.txt
{
  echo "== commit =="; git rev-parse HEAD
  echo "== kat sha256 =="; sha256sum kat/vectors.json
  echo "== compiler / cmake / libraries =="
  g++ --version | head -1
  cmake --version | head -1
  dpkg -s libssl-dev | grep -E "^Version"
  dpkg -s libcrypto++-dev | grep -E "^Version"
  echo "== cpu =="; lscpu | grep -E "Model name|^CPU\(s\)|Thread|Core|MHz|Flags" | sed 's/Flags:.*\(aes\).*/Flags: ... aes ... (AES-NI present)/'
  echo "== governor =="; cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null
  echo "== ram =="; free -h
  echo "== os =="; lsb_release -d; uname -a
  echo "== disk =="; lsblk -d -o NAME,ROTA,SIZE,MODEL
} > "$F"
cat "$F"
```

Windows (PowerShell): xem mục 2.4 của [BENCHMARK_RUNBOOK.md](BENCHMARK_RUNBOOK.md).
