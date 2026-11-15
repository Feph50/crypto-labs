# Lab 1 — Runbook Windows và Linux (cùng cấu hình để so sánh công bằng)

Mục đích: bạn chạy benchmark và kiểm thử Lab 1 trên **Windows** (đang làm) rồi sang **Linux** với **đúng cùng cấu hình**, để phần so sánh
Windows vs Linux trong báo cáo có ý nghĩa. Mọi lệnh bên dưới là lệnh gõ trực tiếp (PowerShell / bash), không cần chạy file `.sh` hay `.bat`.

> Tất cả lệnh chạy từ thư mục `lab1` của repo, trừ khi ghi khác. Lệnh `..\scripts\...` / `../scripts/...` nghĩa là thư mục `scripts` ở gốc repo.

---

## 0. Repo của bạn

Bạn **đã có repo sẵn**:

| Mục | Giá trị |
|---|---|
| Remote `origin` | `https://github.com/Feph50/crypto-labs.git` |
| Nhánh | `main` (đang đồng bộ với `origin/main` tại commit `ab5393b`) |
| Thay đổi chưa commit | **Có**: toàn bộ phần sửa Lab 1 và Lab 2 trong phiên làm việc gần đây **chưa được commit**, nên GitHub chưa có chúng |

Vì vậy, **trước khi sang Linux** bạn phải commit và push từ Windows để Linux lấy được đúng code:

```powershell
cd "D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs-Lnguyen"
git status --short
git add -A
git commit -m "Lab 1: portable CMake, 29 KAT vectors, new tests, runbook, release binaries"
git push origin main
```

Sau đó ghi lại commit dùng để đo (cả hai máy phải trùng):

```powershell
git rev-parse HEAD
```

> Lưu ý: nếu muốn Linux chạy được file binary dựng sẵn ngay sau khi clone, cần cờ thực thi. `lab1/release/ubuntu/aestool_gcc` đã có cờ này trong git (`100755`), nên không phải làm gì thêm.
> Các file `run.sh` ở thư mục gốc có xuống dòng kiểu Windows (CRLF); bạn **không dùng** chúng nên không ảnh hưởng. Nếu muốn dùng trên Linux: `sed -i 's/\r$//' run.sh lab1/run.sh`.

---

## 1. Hợp đồng đo — những thứ phải GIỐNG HỆT nhau giữa hai hệ điều hành

| Hạng mục | Windows | Linux | Cách đảm bảo |
|---|---|---|---|
| Mã nguồn | commit `X` | **cùng commit `X`** | `git rev-parse HEAD` ở cả hai máy |
| Compiler | MinGW-w64 GCC (MSYS2) | GCC (apt) | Cùng họ **GCC**; ghi lại số phiên bản (không thể trùng 100%) |
| Cờ biên dịch | `Release` = `-O3 -DNDEBUG` | `Release` = `-O3 -DNDEBUG` | Cùng lệnh CMake (`-DCMAKE_BUILD_TYPE=Release`) |
| Thư viện mật mã | Crypto++ **8.9.0** (pacman) | Crypto++ **8.9.0** (apt hoặc build từ nguồn) | Kiểm tra phiên bản ở mục 2.4 và 3.4 |
| Liên kết | tĩnh (`-DAESTOOL_STATIC_RUNTIME=ON`) | tĩnh | Dùng binary release hoặc build với cùng cờ |
| Số luồng | 1 | 1 | Không dùng `--threads` khi đo để so sánh |
| Số lần lặp | `--runs 30` | `--runs 30` | Cùng lệnh |
| Tham số bench | mặc định (warm-up 1,5 s, ngân sách 256 MiB) | mặc định | **Không** đổi `--warmup`, `--ops`, `--budget-mb` giữa hai máy |
| Bộ vector KAT | `kat/vectors.json` | cùng file | So sánh SHA-256 ở mục 2.4 / 3.4 |
| Nguồn điện | cắm điện, hiệu năng cao | cắm điện, governor `performance` | Mục 2.2 / 3.2 |
| Phần cứng | **cùng một máy** | **cùng một máy** (Linux cài song song hoặc live USB) | Xem cảnh báo bên dưới |

**Cảnh báo phần cứng:** để so sánh Windows vs Linux có ý nghĩa, hãy chạy trên **cùng một máy vật lý**. Ubuntu cài song song (dual boot) hoặc
boot từ USB là phù hợp. **WSL2 và máy ảo không phù hợp** cho số đo chính thức: chúng chạy trên hypervisor của Windows, có độ trễ lập lịch và
giới hạn CPU riêng, và không có `cpufreq` để đặt governor. Nếu buộc phải dùng WSL2, hãy ghi rõ trong báo cáo đây là "Linux trong WSL2".

---

## 2. Windows (PowerShell)

### 2.1. Cài đặt (một lần)

Cài MSYS2 từ <https://www.msys2.org>, mở shell **MSYS2 MINGW64** và chạy:

```bash
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-crypto++ mingw-w64-x86_64-catch
```

Trong PowerShell, đưa MinGW lên đầu `PATH` cho phiên làm việc hiện tại:

```powershell
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
g++ --version
cmake --version
pacman -Q mingw-w64-x86_64-crypto++
```

Cần Python cho biểu đồ:

```powershell
pip install pandas matplotlib numpy
```

### 2.2. Chuẩn bị đo công bằng

```powershell
# Chế độ nguồn: High performance, và hãy cắm điện
powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c
powercfg /getactivescheme
```

- Cắm sạc, tắt trình duyệt, game, ứng dụng nền nặng, tạm dừng cập nhật Windows và Windows Defender quét tự động nếu có thể.
- Để máy nghỉ khoảng 1 phút cho nguội trước khi bắt đầu đo.

### 2.3. Build từ mã nguồn (cùng cấu hình với binary release)

```powershell
cd "D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs-Lnguyen\lab1"
mkdir build_cmp_win
cd build_cmp_win
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DAESTOOL_STATIC_RUNTIME=ON
cmake --build .
ctest --output-on-failure
.\aestool.exe --kat ..\kat\vectors.json
cd ..
```

Kết quả mong đợi: `100% tests passed, 0 tests failed out of 3` và `29 passed, 0 failed, 29 total`.
(Bạn cũng có thể dùng thẳng binary dựng sẵn `release\windows\aestool_mingw.exe`; bước build này chỉ để chắc chắn cùng cấu hình.)

### 2.4. Ghi lại môi trường (chạy một lần, lưu cùng kết quả)

```powershell
mkdir -Force output\windows
$envfile = "output\windows\env_windows.txt"
"== commit ==" | Out-File $envfile
git rev-parse HEAD | Out-File $envfile -Append
"== kat sha256 ==" | Out-File $envfile -Append
(Get-FileHash kat\vectors.json -Algorithm SHA256).Hash | Out-File $envfile -Append
"== compiler / cmake / cryptopp ==" | Out-File $envfile -Append
g++ --version | Select-Object -First 1 | Out-File $envfile -Append
cmake --version | Select-Object -First 1 | Out-File $envfile -Append
pacman -Q mingw-w64-x86_64-crypto++ 2>$null | Out-File $envfile -Append
"== cpu ==" | Out-File $envfile -Append
Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors,MaxClockSpeed | Format-List | Out-File $envfile -Append
"== ram ==" | Out-File $envfile -Append
Get-CimInstance Win32_ComputerSystem | Select-Object @{n='RAM_GB';e={[math]::Round($_.TotalPhysicalMemory/1GB,1)}} | Format-List | Out-File $envfile -Append
"== os ==" | Out-File $envfile -Append
Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,BuildNumber | Format-List | Out-File $envfile -Append
"== disk ==" | Out-File $envfile -Append
Get-PhysicalDisk | Select-Object FriendlyName,MediaType,Size | Format-List | Out-File $envfile -Append
"== power plan ==" | Out-File $envfile -Append
powercfg /getactivescheme | Out-File $envfile -Append
Get-Content $envfile
```

> `pacman` có thể không có trong PowerShell; nếu dòng đó báo lỗi, chạy `pacman -Q mingw-w64-x86_64-crypto++` trong MSYS2 MINGW64 và ghi phiên bản (8.9.0) vào file.

### 2.4b. Kiểm tra trước khi đo

```powershell
.\release\windows\aestool_mingw.exe --kat kat\vectors.json | Select-Object -Last 1
```

### 2.5. Chạy benchmark (mất khoảng 6 phút mỗi lệnh)

```powershell
mkdir -Force output\windows\benchmarks
.\release\windows\aestool_mingw.exe bench --runs 30 --out output\windows\benchmarks\bench_windows_gcc.csv
```
Tùy chọn, để so sánh MSVC với MinGW:
```powershell
.\release\windows\aestool_msvc.exe bench --runs 30 --out output\windows\benchmarks\bench_windows_msvc.csv
```

> Không thao tác trên máy trong lúc đo. Nếu có việc làm gián đoạn (máy đi ngủ, bật ứng dụng nặng), hãy xóa file CSV và đo lại.

### 2.6. Bảng thống kê cho báo cáo

```powershell
python ..\scripts\csv_to_stats_table.py output\windows\benchmarks\bench_windows_gcc.csv --size 1048576 > output\windows\stats_1MiB.md
```

---

## 3. Linux (Ubuntu LTS, bash)

### 3.1. Cài đặt (một lần)

```bash
sudo apt update
sudo apt install -y build-essential cmake git ninja-build libcrypto++-dev catch2 pkg-config python3-pip
pip3 install --user pandas matplotlib numpy
# (Ubuntu 24.04+: nếu pip báo "externally-managed-environment", dùng:  sudo apt install -y python3-pandas python3-matplotlib python3-numpy)
```

Lấy mã nguồn **đúng commit đã đo trên Windows**:

```bash
git clone https://github.com/Feph50/crypto-labs.git
cd crypto-labs
git checkout main
git pull
git rev-parse HEAD            # phải TRÙNG với commit ghi ở Windows (mục 0)
cd lab1
```

### 3.2. Chuẩn bị đo công bằng

```bash
# Cắm điện. Đặt governor = performance (máy Linux thật; WSL không có cpufreq)
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
```

Nếu không có thư mục `cpufreq` (một số máy dùng `intel_pstate`), cài `linux-tools-common linux-tools-generic` rồi:

```bash
sudo cpupower frequency-set -g performance
```

- Đóng trình duyệt và các ứng dụng nặng; để máy nghỉ khoảng 1 phút trước khi đo.
- Không bật chế độ tiết kiệm pin. (Tắt turbo là **tùy chọn**: nếu bạn tắt turbo ở Linux thì cũng phải tắt ở Windows để công bằng, nên mặc định cả hai đều giữ nguyên.)

### 3.3. Build từ mã nguồn (cùng cấu hình với Windows)

```bash
mkdir build_cmp_linux && cd build_cmp_linux
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DAESTOOL_STATIC_RUNTIME=ON
cmake --build .
ctest --output-on-failure
./aestool --kat ../kat/vectors.json | tail -1
cd ..
```

Kết quả mong đợi: `100% tests passed, 0 tests failed out of 3` và `29 passed, 0 failed, 29 total`, giống hệt Windows.

Nếu `cmake` báo không tìm thấy Crypto++, hoặc phiên bản Crypto++ **thấp hơn 8.9.0** (kiểm tra ở 3.4), xem mục 3.5.

### 3.4. Ghi lại môi trường (chạy một lần, lưu cùng kết quả)

```bash
mkdir -p output/ubuntu
F=output/ubuntu/env_ubuntu.txt
{
  echo "== commit =="; git rev-parse HEAD
  echo "== kat sha256 =="; sha256sum kat/vectors.json
  echo "== compiler / cmake / cryptopp =="
  g++ --version | head -1
  cmake --version | head -1
  dpkg -s libcrypto++-dev | grep -E "^Version"
  echo "== cpu =="; lscpu | grep -E "Model name|^CPU\(s\)|Thread|Core|MHz"
  echo "== governor =="; cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null
  echo "== ram =="; free -h
  echo "== os =="; lsb_release -d; uname -a
  echo "== disk =="; lsblk -d -o NAME,ROTA,SIZE,MODEL
} > "$F"
cat "$F"
```

So sánh hai dòng `kat sha256` của Windows và Linux: **phải giống nhau** (cùng file vector). Phiên bản Crypto++ ở hai máy cũng nên là `8.9.0`.

### 3.5. Nếu Crypto++ của apt không phải 8.9.0 (tùy chọn nhưng khuyến nghị để khớp Windows)

```bash
git clone --branch CRYPTOPP_8_9_0 --depth 1 https://github.com/weidai11/cryptopp.git ~/cryptopp-8.9.0
cd ~/cryptopp-8.9.0
make -j"$(nproc)" libcryptopp.a
sudo make install PREFIX=/usr/local
cd -
```

Sau đó build lại, trỏ CMake vào bản vừa cài:

```bash
rm -rf build_cmp_linux
mkdir build_cmp_linux && cd build_cmp_linux
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DAESTOOL_STATIC_RUNTIME=ON -DCRYPTOPP_ROOT_DIR=/usr/local
cmake --build .
ctest --output-on-failure
cd ..
```

### 3.6. Kiểm tra trước khi đo

```bash
chmod +x release/ubuntu/aestool_gcc
./release/ubuntu/aestool_gcc --kat kat/vectors.json | tail -1
```

### 3.7. Chạy benchmark (mất khoảng 6 phút)

```bash
mkdir -p output/ubuntu/benchmarks
./release/ubuntu/aestool_gcc bench --runs 30 --out output/ubuntu/benchmarks/bench_ubuntu_gcc.csv
```

Nếu muốn đo bằng binary vừa tự build thay cho binary dựng sẵn (cho kết quả tương đương):

```bash
./build_cmp_linux/aestool bench --runs 30 --out output/ubuntu/benchmarks/bench_ubuntu_gcc.csv
```

### 3.8. Bảng thống kê cho báo cáo

```bash
python3 ../scripts/csv_to_stats_table.py output/ubuntu/benchmarks/bench_ubuntu_gcc.csv --size 1048576 > output/ubuntu/stats_1MiB.md
```

---

## 4. Đưa kết quả về một chỗ, vẽ biểu đồ và so sánh

### 4.1. Chuyển file từ Linux về Windows (dùng git)

Trên **Linux**:

```bash
cd ~/crypto-labs
git add lab1/output/ubuntu
git commit -m "Lab 1: Ubuntu benchmark results and environment"
git push origin main
```

(Lần đầu cần cấu hình: `git config --global user.name "Tên"` và `git config --global user.email "email"`; GitHub yêu cầu đăng nhập bằng token cá nhân khi push.)

Trên **Windows**:

```powershell
cd "D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs-Lnguyen"
git pull
```

Nếu không muốn dùng git, chép tay hai file `bench_ubuntu_gcc.csv` và `env_ubuntu.txt` vào `lab1\output\ubuntu\benchmarks\` và `lab1\output\ubuntu\`.

### 4.2. Vẽ biểu đồ, phân tích và so sánh Windows vs Linux

```powershell
cd "D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs-Lnguyen\lab1"
python scripts\plot_bench.py --auto --prefer gcc
```

Lệnh này đọc `output/<os>/benchmarks/*.csv` (mỗi hệ điều hành chọn file có chữ `gcc` trong tên, để so sánh cùng họ compiler) và tạo:

| Thư mục | Nội dung |
|---|---|
| `output/windows/plots/`, `output/ubuntu/plots/` | Đồ thị throughput và latency từng hệ điều hành |
| `output/windows/mode_cmp/`, `output/ubuntu/mode_cmp/` | Phân tích stream vs block, AEAD vs non-AEAD, overhead tag |
| `output/compare/` | So sánh Windows vs Ubuntu (đồ thị, `compare_summary.csv`, `mode_cmp/`) |

### 4.3. Cập nhật báo cáo

Chép các số vào `lab1/docs/lab1-main-report.md`:

- Mục 5.1: bảng phần cứng/OS (lấy từ `env_windows.txt` và `env_ubuntu.txt`).
- Bảng 3 và Bảng 3b: số liệu mới (bảng 3b dán từ `stats_1MiB.md` của từng hệ điều hành).
- Các hình trong `output/compare/` và `output/<os>/plots/`.
- Mục 5.5 (so sánh) và 5.6 (nguyên nhân khác biệt giữa hai hệ điều hành): viết lại theo số liệu mới.

---

## 5. Danh sách kiểm tra trước khi đo (in ra và tích)

- [ ] Hai máy dùng **cùng commit** (`git rev-parse HEAD`) và **cùng file KAT** (SHA-256 trùng).
- [ ] `ctest` 3/3 và KAT 29/29 trên **cả hai** hệ điều hành.
- [ ] Cùng **một máy vật lý** (không phải WSL2 hay máy ảo), đã cắm điện.
- [ ] Windows: plan **High performance**. Linux: governor **performance**.
- [ ] Đã ghi `env_windows.txt` và `env_ubuntu.txt` (phiên bản compiler và Crypto++, CPU, RAM, OS).
- [ ] Cùng lệnh `bench --runs 30`, không đổi `--warmup`, `--ops`, `--budget-mb`, không dùng `--threads`.
- [ ] Không chạy việc khác trong lúc đo; nếu bị gián đoạn thì đo lại cả file.
- [ ] CSV đặt đúng tên: `bench_windows_gcc.csv` và `bench_ubuntu_gcc.csv` (chữ `gcc` trong tên).
- [ ] (Khuyến nghị) Chạy mỗi hệ điều hành **2 lần** và so hai file CSV; nếu chênh quá vài phần trăm, kiểm tra lại nhiệt độ, nguồn điện và tiến trình nền.

---

## 6. Xử lý sự cố

| Triệu chứng | Nguyên nhân thường gặp | Cách xử lý |
|---|---|---|
| `Permission denied` khi chạy `aestool_gcc` | Mất cờ thực thi khi chép file | `chmod +x release/ubuntu/aestool_gcc` |
| `cmake` không tìm thấy Crypto++ (Linux) | Chưa cài `libcrypto++-dev` | `sudo apt install libcrypto++-dev`, hoặc mục 3.5 |
| `cmake` không tìm thấy Crypto++ (Windows) | `PATH` thiếu MinGW | `$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH` |
| `Catch2` không có, cần tải | Chưa cài gói Catch2 | `sudo apt install catch2`, hoặc có Internet để CMake tải một lần; hoặc thêm `-DBUILD_TESTS=OFF` |
| `error: externally-managed-environment` (pip) | Ubuntu 24.04+ | Dùng gói `python3-pandas python3-matplotlib python3-numpy` của apt |
| Không có `/sys/devices/system/cpu/cpu0/cpufreq` | WSL hoặc máy ảo, hoặc driver `intel_pstate` | Dùng máy Linux thật; thử `cpupower`; nếu không đặt được, ghi rõ trong báo cáo |
| Số đo dao động lớn giữa các lần chạy | Nhiệt độ, nguồn điện, tiến trình nền | Mục 2.2 / 3.2; chạy lại sau khi để máy nguội |
| `plot_bench.py` báo thiếu CSV Linux | Tên file không chứa `gcc` hoặc sai thư mục | Đặt `output/ubuntu/benchmarks/bench_ubuntu_gcc.csv` |
| Xuống dòng `\r` làm hỏng script (Linux) | File từ Windows (CRLF) | `sed -i 's/\r$//' <file>` |

---

## 7. Những khác biệt vốn có giữa hai hệ điều hành (ghi vào phần phân tích)

Dù cấu hình giống nhau, các yếu tố sau vẫn làm kết quả khác và nên được thảo luận trong báo cáo (yêu cầu của đề: OS scheduling, system call, RNG, hệ thống file):

- **Lập lịch:** Windows và Linux dùng bộ lập lịch khác nhau (quantum, ưu tiên, xử lý ngắt); ảnh hưởng rõ nhất ở payload nhỏ (1 KB, 4 KB) nơi một thao tác chỉ vài micro giây.
- **Chi phí gọi hệ thống và bộ nhớ:** cấp phát bộ nhớ, `QueryPerformanceCounter` so với `clock_gettime`, trang bộ nhớ lớn.
- **Nguồn ngẫu nhiên:** Windows dùng CNG (`BCryptGenRandom`), Linux dùng `getrandom(2)`; chỉ ảnh hưởng thời gian sinh khóa/IV, không ảnh hưởng thông lượng mã hóa.
- **Hệ thống file:** NTFS và ext4 khác nhau về xuống dòng, đường dẫn, bộ đệm trang; công cụ mở file ở chế độ nhị phân nên dữ liệu không bị đổi.
- **Compiler và C runtime:** GCC trên Windows dùng runtime MinGW, trên Linux dùng glibc; phiên bản GCC hai bên khác nhau, nên ghi cả hai số phiên bản.
- **Tăng tốc phần cứng:** nếu Crypto++ ở hai máy được build với/không với AES-NI và PCLMULQDQ, thông lượng GCM/CTR sẽ khác lớn; vì vậy cần cùng phiên bản và cùng cách build thư viện.
