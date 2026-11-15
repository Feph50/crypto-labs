# Lab 2 — Việc cần làm trên Linux (chạy benchmark, cùng cấu hình với Windows)

Bản rút gọn, chỉ có lệnh bash để chạy trên **Ubuntu LTS thật** (không phải WSL2), cùng máy vật lý với Windows. Chi tiết và lý do nằm ở
[BENCHMARK_RUNBOOK.md](BENCHMARK_RUNBOOK.md). Mọi lệnh gõ trực tiếp, không dùng file `.sh`.

## 1. Cài đặt (một lần)

```bash
sudo apt update
sudo apt install -y build-essential cmake git ninja-build libssl-dev libcrypto++-dev catch2 python3-matplotlib
```

## 2. Lấy mã nguồn đúng commit đã dùng trên Windows

```bash
git clone https://github.com/Feph50/crypto-labs.git
cd crypto-labs
git pull
git rev-parse HEAD        # phải TRÙNG với commit ghi ở Windows
cd lab2
```

## 3. Chuẩn bị đo công bằng

```bash
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor        # mong đợi: performance
```

Cắm điện, đóng trình duyệt và ứng dụng nặng, để máy nghỉ khoảng 1 phút. Nếu không có `cpufreq`, thử `sudo apt install -y linux-tools-common linux-tools-generic`
rồi `sudo cpupower frequency-set -g performance`; nếu vẫn không đặt được, ghi rõ trong báo cáo.

## 4. Build và kiểm tra (đúng 4 lệnh của đề, rồi ctest)

```bash
mkdir build_ubuntu_gcc && cd build_ubuntu_gcc
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
ctest --output-on-failure
cd ..
```

Mong đợi: `100% tests passed, 0 tests failed out of 4`.

```bash
./build_ubuntu_gcc/aestool --kat kat/vectors.json | tail -1       # mong đợi: 31 tests, 31 passed, 0 failed
./build_ubuntu_gcc/aestool_ref xval --ref all | tail -1           # mong đợi: 0 failed
```

## 5. Benchmark

Chạy **đúng bộ lệnh đã chạy trên Windows** (cùng `--backends`, `--ops`, `--runs`), **tuần tự**, không làm việc khác trên máy. 1 GiB cần khoảng 2,2 GiB RAM trống.
Đặt biến (nếu mở terminal mới thì đặt lại):

```bash
cd ~/crypto-labs/lab2
R=./build_ubuntu_gcc/aestool_ref; O=output/ubuntu/bench; mkdir -p $O
$R --help | head -3
```

### Bước 1: `table` ở 1 MiB, 100 MiB, 1 GiB (AES-128, mã hóa và giải mã, so với Crypto++ và OpenSSL; ~20 phút, ước tính)

```bash
$R bench --bits 128 --sizes 1MiB --backends table --ref all --runs 30 --csv $O/bench_aes128_1MiB.csv
```

```bash
$R bench --bits 128 --sizes 100MiB --backends table --ref all --runs 30 --csv $O/bench_aes128_100MiB.csv
```

```bash
$R bench --bits 128 --sizes 1GiB --backends table --ref all --runs 30 --csv $O/bench_aes128_1GiB.csv
```

### Bước 2: so sánh table với computed, cùng cấu hình 4 KiB x 1000 thao tác (~4 phút)

```bash
$R bench --bits 128 --sizes 4KiB --backends table,computed --ref none --runs 30 --csv $O/bench_sbox_4KiB.csv
```

### Bước 3: `computed` ở 1 MiB (~2 phút)

```bash
$R bench --bits 128 --sizes 1MiB --backends computed --ops 2 --ref none --runs 30 --csv $O/bench_computed_1MiB.csv
```

### Bước 4: `computed` ở 100 MiB (~1,7 giờ; chạy khi không dùng máy)

```bash
$R bench --bits 128 --sizes 100MiB --backends computed --ops 1 --ref none --runs 30 --csv $O/bench_computed_100MiB.csv
```

Nếu trên Windows bạn đã thêm `--dir enc` cho lệnh này (để rút còn ~50 phút), thì Linux cũng phải thêm `--dir enc`.
`computed` ở 1 GiB không khả thi (~17 giờ), nên không chạy; báo cáo sẽ ghi lý do.

### Tùy chọn (bonus): AES-NI, AES-192, AES-256

```bash
$R bench --bits 128 --sizes 1MiB --backends aesni --ref none --runs 30 --csv $O/bench_aesni_1MiB.csv
$R bench --bits 192 --sizes 1MiB --backends table,aesni --ref all --runs 30 --csv $O/bench_aes192.csv
$R bench --bits 256 --sizes 1MiB --backends table,aesni --ref all --runs 30 --csv $O/bench_aes256.csv
```

Chỉ chạy nếu bạn đã chạy các lệnh này trên Windows. Không đổi `--warmup`, `--ops`, `--budget-mb` ngoài các lệnh trên; không dùng `--threads`.

## 6. Ghi lại môi trường (trên Linux thật, thay file `env_ubuntu.txt` tạo từ WSL2)

```bash
F=output/ubuntu/env_ubuntu.txt
{
  echo "== commit =="; git rev-parse HEAD
  echo "== kat sha256 =="; sha256sum kat/vectors.json
  echo "== compiler / cmake / libraries =="
  g++ --version | head -1
  cmake --version | head -1
  dpkg -s libssl-dev | grep -E "^Version"
  dpkg -s libcrypto++-dev | grep -E "^Version"
  echo "== cpu =="; lscpu | grep -E "Model name|^CPU\(s\)|Thread|Core|MHz"; lscpu | grep -qw aes && echo "AES-NI: yes (aes flag present)"
  echo "== governor =="; cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null
  echo "== ram =="; free -h
  echo "== os =="; lsb_release -d; uname -a
  echo "== disk =="; lsblk -d -o NAME,ROTA,SIZE,MODEL
} > "$F"
cat "$F"
```

So sánh dòng `kat sha256` với `output/windows/env_windows.txt`: phải giống nhau.

## 7. (Tùy chọn) Tạo lại log KAT / xval / ctest / negative trên Linux thật

Các lệnh nằm ở [TEST_EVIDENCE.md](TEST_EVIDENCE.md) (mục 2, 3, 4). Làm bước này nếu muốn log Ubuntu không phải từ WSL2.

## 8. Đưa kết quả về repo

```bash
cd ~/crypto-labs
git add lab2/output/ubuntu
git commit -m "Lab 2: Ubuntu benchmark results and environment"
git push origin main
```

Lần đầu cần `git config --global user.name "Tên"` và `git config --global user.email "email"`; GitHub yêu cầu token cá nhân khi push. Sau đó trên Windows chạy `git pull`
và báo mình để vẽ biểu đồ, làm bảng thống kê và viết báo cáo.
