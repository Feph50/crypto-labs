# Lab 1 — Bằng chứng kiểm thử (KAT, ctest, test âm tính)

Tài liệu này mô tả các file log dùng làm bằng chứng cho báo cáo (mục 4: KAT, `ctest`, test âm tính) và **lệnh chính xác để tạo ra chúng**,
để log Windows và Ubuntu có cùng nội dung và cùng định dạng. Mọi lệnh gõ trực tiếp, không cần file `.sh` hay `.bat`; lệnh chạy từ thư mục `lab1`.

## 1. Các file log

| File (trong `lab1/output/<os>/`) | Nội dung | Windows | Ubuntu |
|---|---|:---:|:---:|
| `kat_<os>_<compiler>.txt` | Kết quả `aestool --kat kat/vectors.json`: PASS/FAIL từng vector và tổng kết (29/29) | ✅ MinGW, MSVC | cần tạo (mục 3.1) |
| `ctest_<os>_<compiler>.txt` | Kết quả `ctest` (3 test: unit, KAT, CLI negative) kèm phiên bản compiler/CMake/Crypto++/Catch2 và tóm tắt Catch2 (62 assertion, 15 test case) | ✅ MinGW, MSVC | cần tạo (mục 3.2) |
| `negative_<os>_<compiler>.txt` | 34 phép thử fail-closed (sai key, sai IV, ciphertext/tag/IV bị sửa, IV sai độ dài, ECB, tái dùng nonce, file khóa hỏng...) với mã thoát và thông báo lỗi | ✅ MinGW, MSVC | cần tạo (mục 3.3) |
| `env_<os>.txt` | Phần cứng, hệ điều hành, phiên bản compiler và Crypto++ | ✅ | ✅ |

Kết quả mong đợi của phép thử âm tính (đã kiểm chứng giống hệt trên Windows MinGW, Windows MSVC và Linux/WSL2): **12 lệnh mã thoát 0** (đối chứng dương và các trường hợp "giải mã được nhưng sai"),
**17 lệnh mã 1** (đầu vào bị từ chối) và **5 lệnh mã 2** (xác thực/giải mã thất bại); không dòng nào ghi `plaintext file WRITTEN (BAD)`.

## 2. Windows (đã tạo; ghi lại để lặp lại được)

Các log Windows được tạo trong Git Bash (đi kèm Git for Windows) bằng đúng khối lệnh ở mục 3.3 với:

```bash
T=./release/windows/aestool_mingw.exe; PY=python; OUT=output/windows/negative_windows_mingw.txt     # rồi dán khối lệnh mục 3.3
T=./release/windows/aestool_msvc.exe;  PY=python; OUT=output/windows/negative_windows_msvc.txt      # lần chạy thứ hai
```

## 3. Ubuntu: lệnh để tạo các log còn thiếu

Chuẩn bị: đã làm theo `CROSS_PLATFORM_RUNBOOK.md` (có repo, `cd lab1`).

### 3.1. Log KAT

```bash
cd lab1
{ echo "== NIST/IEEE Known Answer Tests: aestool_gcc (Ubuntu) =="; echo "date : $(date -u +%Y-%m-%dT%H:%M:%SZ)"; echo "git  : $(git rev-parse --short HEAD)"; echo "kat  : kat/vectors.json  sha256 $(sha256sum kat/vectors.json | cut -c1-64)"; echo; ./release/ubuntu/aestool_gcc --kat kat/vectors.json; echo "[exit code: $?]"; } 2>&1 | tee output/ubuntu/kat_ubuntu_gcc.txt
```

### 3.2. Log `ctest` (build từ mã nguồn rồi chạy kiểm thử)

```bash
cd lab1
mkdir -p build_cmp_linux && cd build_cmp_linux
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DAESTOOL_STATIC_RUNTIME=ON
cmake --build .
{ echo "== ctest: GCC on Ubuntu (Release, static) =="; echo "date : $(date -u +%Y-%m-%dT%H:%M:%SZ)"; echo "git  : $(git rev-parse --short HEAD)"; echo "g++  : $(g++ --version | head -1)"; echo "cmake: $(cmake --version | head -1)"; echo "cryptopp: $(dpkg -s libcrypto++-dev | grep ^Version)"; echo "catch2 : $(dpkg -s catch2 2>/dev/null | grep ^Version)"; echo; ctest --output-on-failure; echo; echo "-- unit tests (Catch2) summary:"; ./test_core | tail -3; } 2>&1 | tee ../output/ubuntu/ctest_ubuntu_gcc.txt
cd ..
```

### 3.3. Log test âm tính (fail-closed)

Dán nguyên khối sau (tự tạo thư mục tạm `neg_tmp`, ghi log vào `output/ubuntu/negative_ubuntu_gcc.txt`):

```bash
cd lab1
T=./release/ubuntu/aestool_gcc; PY=python3; OUT=output/ubuntu/negative_ubuntu_gcc.txt
mkdir -p neg_tmp
W=neg_tmp
{
echo "== Lab 1 negative tests (fail-closed evidence) =="
echo "tool : $T"
echo "date : $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "git  : $(git rev-parse --short HEAD 2>/dev/null)"
echo
run() { echo "\$ $*"; "$@"; echo "[exit code: $?]"; echo; }

echo "################ 0. Setup (test keys and IVs, generated with the OS CSPRNG)"
run $T keygen --bits 256 --out $W/key.bin
run $T keygen --bits 256 --out $W/key_wrong.bin
echo "Hello World plaintext for negative tests" > $W/plain.txt
$PY -c "import os;[open('$W/'+n,'wb').write(os.urandom(k)) for n,k in (('iv16.bin',16),('iv_wrong.bin',16),('iv8.bin',8))];open('$W/big.bin','wb').write(b'A'*20000)"

echo "################ A. Positive controls (must succeed, exit code 0)"
run $T encrypt --mode cbc --key $W/key.bin --iv $W/iv16.bin --in $W/plain.txt --out $W/cbc.bin --nonce-db $W/nonces
run $T decrypt --mode cbc --key $W/key.bin --iv $W/iv16.bin --in $W/cbc.bin --out $W/cbc.out
run cmp $W/plain.txt $W/cbc.out
run $T encrypt --mode gcm --aead --key $W/key.bin --in $W/plain.txt --out $W/gcm.bin --nonce-db $W/nonces
run $T decrypt --mode gcm --aead --key $W/key.bin --in $W/gcm.bin --out $W/gcm.out
run cmp $W/plain.txt $W/gcm.out

echo "################ B. Wrong key -> refusal / wrong plaintext"
run $T decrypt --mode cbc --key $W/key_wrong.bin --iv $W/iv16.bin --in $W/cbc.bin --out $W/wrongkey.out
run $T decrypt --mode gcm --aead --key $W/key_wrong.bin --in $W/gcm.bin --out $W/wrongkey_gcm.out
test -e $W/wrongkey_gcm.out && echo "RESULT: plaintext file WRITTEN (BAD)" || echo "RESULT: no plaintext file written (good)"; echo

echo "################ C. Wrong IV -> wrong plaintext (CTR: decryption 'succeeds' but output differs)"
run $T encrypt --mode ctr --key $W/key.bin --iv $W/iv16.bin --in $W/plain.txt --out $W/ctr.bin --nonce-db $W/nonces
run $T decrypt --mode ctr --key $W/key.bin --iv $W/iv_wrong.bin --in $W/ctr.bin --out $W/ctr_wrongiv.out
run cmp $W/plain.txt $W/ctr_wrongiv.out
echo "(cmp exit code 1 = files differ = expected)"; echo

echo "################ D. Tampered ciphertext"
echo "--- D1. non-AEAD (CTR): one flipped bit -> decrypts, plaintext corrupted, NOT detected"
$PY -c "d=bytearray(open('$W/ctr.bin','rb').read());d[3]^=0x80;open('$W/ctr_bad.bin','wb').write(d)"
run $T decrypt --mode ctr --key $W/key.bin --iv $W/iv16.bin --in $W/ctr_bad.bin --out $W/ctr_bad.out
run cmp $W/plain.txt $W/ctr_bad.out
echo "(cmp exit code 1 = corrupted output = expected)"; echo
echo "--- D2. AEAD (GCM): one flipped ciphertext bit -> authentication failure, no plaintext file"
$PY -c "d=bytearray(open('$W/gcm.bin','rb').read());d[0]^=1;open('$W/gcm_bad.bin','wb').write(d)"
cp $W/gcm.bin.json $W/gcm_bad.bin.json
run $T decrypt --mode gcm --aead --key $W/key.bin --in $W/gcm_bad.bin --out $W/gcm_bad.out
test -e $W/gcm_bad.out && echo "RESULT: plaintext file WRITTEN (BAD)" || echo "RESULT: no plaintext file written (good)"; echo

echo "################ E. Tampered / invalid authentication tag (AEAD)"
$PY -c 'import json,sys;j=json.load(open(sys.argv[1]));t=j["tag"];j["tag"]=("1" if t[0]=="0" else "0")+t[1:];json.dump(j,open(sys.argv[2],"w"))' $W/gcm.bin.json $W/tag_bad.json
run $T decrypt --mode gcm --aead --key $W/key.bin --in $W/gcm.bin --meta $W/tag_bad.json --out $W/tag_bad.out
test -e $W/tag_bad.out && echo "RESULT: plaintext file WRITTEN (BAD)" || echo "RESULT: no plaintext file written (good)"; echo
run $T decrypt --mode gcm --aead --key $W/key.bin --in $W/gcm.bin --tag-hex 00112233 --out $W/tag_short.out
$PY -c 'import json,sys;j=json.load(open(sys.argv[1]));t=j["iv"];j["iv"]=("1" if t[0]=="0" else "0")+t[1:];json.dump(j,open(sys.argv[2],"w"))' $W/gcm.bin.json $W/iv_bad.json
echo "--- tampered IV in the header:"
run $T decrypt --mode gcm --aead --key $W/key.bin --in $W/gcm.bin --meta $W/iv_bad.json --out $W/iv_bad.out

echo "################ F. Invalid IV / nonce length -> rejected"
run $T encrypt --mode cbc --key $W/key.bin --iv $W/iv8.bin --in $W/plain.txt --out $W/x1.bin --nonce-db $W/nonces
run $T encrypt --mode gcm --aead --key $W/key.bin --nonce 0011 --in $W/plain.txt --out $W/x2.bin --nonce-db $W/nonces

echo "################ G. Misuse prevention"
echo "--- G1. AEAD mode without --aead"
run $T encrypt --mode gcm --key $W/key.bin --in $W/plain.txt --out $W/x3.bin --nonce-db $W/nonces
echo "--- G2. ECB on a 20000-byte file (> 16 KiB): blocked, then allowed only with --allow-ecb"
run $T encrypt --mode ecb --key $W/key.bin --in $W/big.bin --out $W/ecb1.bin --nonce-db $W/nonces
run $T encrypt --mode ecb --allow-ecb --encode raw --key $W/key.bin --in $W/big.bin --out $W/ecb2.bin --nonce-db $W/nonces
echo "--- G3. Nonce reuse (same key + same IV again, CTR) -> rejected"
run $T encrypt --mode ctr --key $W/key.bin --iv $W/iv16.bin --in $W/plain.txt --out $W/ctr2.bin --nonce-db $W/nonces

echo "################ H. Malformed input -> fail closed"
echo "--- H1. malformed hex key"
run $T encrypt --mode ctr --key-hex ZZ --in $W/plain.txt --out $W/x4.bin --nonce-db $W/nonces
echo "--- H2. key of wrong length"
run $T encrypt --mode ctr --key-hex 00112233 --in $W/plain.txt --out $W/x5.bin --nonce-db $W/nonces
echo "--- H3. corrupted key file (hex header, non-hex content)"
printf 'AESKEY-HEX\nnot-hex-at-all\n' > $W/badkey.hex
run $T encrypt --mode ctr --key $W/badkey.hex --in $W/plain.txt --out $W/x6.bin --nonce-db $W/nonces
echo "--- H4. missing key file / missing input file"
run $T encrypt --mode ctr --key $W/no_such_key.bin --in $W/plain.txt --out $W/x7.bin --nonce-db $W/nonces
run $T encrypt --mode ctr --key $W/key.bin --in $W/no_such_input.bin --out $W/x8.bin --nonce-db $W/nonces
echo "--- H5. unknown mode / XTS input shorter than 16 bytes / corrupted sidecar header"
run $T encrypt --mode des --key $W/key.bin --in $W/plain.txt --out $W/x9.bin
run $T encrypt --mode xts --key-hex 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f --text short --out $W/x10.bin --nonce-db $W/nonces
echo "this is not json" > $W/garbage.json
run $T decrypt --mode gcm --aead --key $W/key.bin --in $W/gcm.bin --meta $W/garbage.json --out $W/g.out

echo "################ I. Known Answer Test file that is not valid JSON must not report success"
echo "[ {\"name\": \"x\", \"mode\": \"gcm\"" > $W/bad_kat.json
run $T --kat $W/bad_kat.json
echo "== end =="
} 2>&1 | tee "$OUT"
```

Sau khi chạy xong, xóa thư mục tạm và kiểm tra kết quả:

```bash
rm -r neg_tmp
grep -h "^\[exit code" output/ubuntu/negative_ubuntu_gcc.txt | sort | uniq -c       # mong đợi: 12 x 0, 17 x 1, 5 x 2
grep -c "WRITTEN (BAD)" output/ubuntu/negative_ubuntu_gcc.txt                         # mong đợi: 0
```

### 3.4. Đưa log lên repo

```bash
cd ~/crypto-labs
git add lab1/output/ubuntu
git commit -m "Lab 1: Ubuntu KAT, ctest and negative-test logs"
git push origin main
```
