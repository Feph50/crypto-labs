# Lab 2 — Runbook benchmark Windows và Linux (cùng cấu hình để so sánh công bằng)

Bạn chạy benchmark trên **Windows** rồi sang **Linux** với **đúng cùng lệnh và cùng tham số**. Mọi lệnh gõ trực tiếp (PowerShell / bash), không dùng file `.sh` hay `.bat`.
Chạy từ thư mục `lab2` của repo.

Tình trạng hiện tại: code, test, KAT, cross-validation và log bằng chứng (`output/<os>/kat_*, xval_*, ctest_*, negative_*, env_*`) **đã có**. Việc còn lại của bạn
chỉ là **chạy các lệnh benchmark ở mục 3 (Windows) và mục 4 (Linux)**, rồi cho mình file CSV để vẽ biểu đồ và viết báo cáo.

## 0. Đề yêu cầu gì về benchmark, và đâu là lựa chọn của dự án

| Nội dung | Nguồn |
|---|---|
| Thông lượng mã hóa **và** giải mã AES-128; 1 MiB, 100 MiB, 1 GiB (nếu khả thi) | Đề Lab 2 |
| Phân tích S-box dạng bảng và S-box tính toán (`computed`), bộ nhớ, cache, branch prediction, so sánh với Crypto++; bảng MB/s và biểu đồ | Đề Lab 2 |
| Warm-up 1-2 s; khối ~1000 thao tác; lặp 30-100 lần; mean, median, SD, CI 95%; bảng phần cứng/OS; governor/power plan; biểu đồ có error bar | Quy tắc chung của đề |
| Cách chạy cụ thể (lệnh, `--ops`, `--runs`, thứ tự) | **Đề không quy định.** Các lệnh dưới là lựa chọn của dự án để đúng quy tắc chung |
| `computed` ở 1 MiB (2 thao tác mỗi khối) và 100 MiB (1 thao tác mỗi khối), lệnh 3 và 3b bên dưới | Đề không nói rõ so sánh table với computed ở kích thước nào; đọc theo nghĩa đen thì ở các kích thước đã liệt kê. Lệch protocol có chủ ý: `computed` chạy ~1 MB/s nên 1000 thao tác 1 MiB mất ~17 phút mỗi lần lặp. 1 GiB không khả thi. **Ghi rõ trong báo cáo** |
| `computed` ở 4 KiB với đủ 1000 thao tác (lệnh 2) | Cấu hình cùng protocol cho so sánh table và computed |

## 1. Hợp đồng đo (giống hệt giữa hai hệ điều hành)

| Hạng mục | Windows | Linux |
|---|---|---|
| Mã nguồn | commit `X` | **cùng commit `X`** (`git rev-parse HEAD`) |
| Compiler | MinGW-w64 GCC (MSYS2) | GCC (apt); cùng họ GCC, ghi số phiên bản |
| Cờ biên dịch | `Release` | `Release` |
| Thư viện tham chiếu | OpenSSL + Crypto++ 8.9.0 | OpenSSL + Crypto++ 8.9.0 |
| Số lần lặp | `--runs 30` | `--runs 30` |
| Luồng | 1 (mặc định) | 1 |
| Tham số khác | **không** đổi `--warmup`, `--ops`, `--budget-mb` ngoài các lệnh dưới | giống Windows |
| Phần cứng | **cùng một máy vật lý** | cùng máy (dual boot hoặc USB). **WSL2 chỉ để thử**, không dùng cho số đo chính thức |

Chạy **tuần tự**, không chạy hai benchmark cùng lúc, không làm việc khác trên máy. Nếu bị gián đoạn thì xóa CSV và đo lại cả lệnh đó.

RAM: mỗi lượt 1 GiB cần khoảng 2,2 GiB trống (payload và bản sao riêng của worker). Nếu không đủ, bỏ `1GiB` khỏi `--sizes` ở lệnh 1 (đề cho phép "nếu khả thi").

## 2. Chuẩn bị (một lần mỗi hệ điều hành)

### 2.1. Windows (PowerShell)

Cài một lần (MSYS2 MinGW64 shell): `pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-openssl mingw-w64-x86_64-crypto++ mingw-w64-x86_64-catch`

```powershell
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c      # High performance; cắm sạc, đóng ứng dụng nặng
cd "D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs-Lnguyen\lab2"
git rev-parse HEAD                                            # ghi lại, phải trùng với Linux
mkdir build_windows_gcc; cd build_windows_gcc
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build .
ctest --output-on-failure                                     # mong đợi: 100% tests passed, 0 tests failed out of 4
cd ..
mkdir -Force output\windows\bench
```

Binary dùng để đo: `build_windows_gcc\aestool_ref.exe` (cùng cấu hình với Linux). Binary tĩnh dựng sẵn `release\windows\aestool_ref_mingw.exe` cũng dùng được (cùng mã nguồn).

### 2.2. Linux (Ubuntu LTS, bash)

```bash
sudo apt update
sudo apt install -y build-essential cmake git ninja-build libssl-dev libcrypto++-dev catch2 python3-matplotlib
git clone https://github.com/Feph50/crypto-labs.git && cd crypto-labs && git pull
git rev-parse HEAD                                            # phải TRÙNG commit ghi ở Windows
cd lab2
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor   # máy Linux thật
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
mkdir build_ubuntu_gcc && cd build_ubuntu_gcc
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
ctest --output-on-failure                                     # mong đợi: 100% tests passed, 0 tests failed out of 4
cd ..
mkdir -p output/ubuntu/bench
```

## 3. Windows: chạy benchmark

Đặt biến cho gọn rồi chạy lần lượt (mỗi lệnh ghi một CSV vào `output\windows\bench`):

```powershell
$R = ".\release\windows\aestool_ref_mingw.exe"; $O = "output\windows\bench"; mkdir -Force $O
```

```powershell
# (1) Yêu cầu của đề: AES-128, 1 MiB / 100 MiB / 1 GiB, encrypt + decrypt, so với OpenSSL và Crypto++ (3 lệnh riêng, ~30 phút tổng)
& $R bench --bits 128 --sizes 1MiB --backends table --ref all --runs 30 --csv "$O\bench_aes128_1MiB.csv"
& $R bench --bits 128 --sizes 100MiB --backends table --ref all --runs 30 --csv "$O\bench_aes128_100MiB.csv"
& $R bench --bits 128 --sizes 1GiB --backends table --ref all --runs 30 --csv "$O\bench_aes128_1GiB.csv"
```

```powershell
# (2) Table S-box so với computed S-box, cùng cấu hình 4 KiB x 1000 thao tác   (~4 phút)
& $R bench --bits 128 --sizes 4KiB --backends table,computed --ref none --runs 30 --csv "$O\bench_sbox_4KiB.csv"
```

```powershell
# (3) computed ở 1 MiB, chỉ 2 thao tác mỗi khối (lệch protocol có chủ ý)   (~2 phút)
& $R bench --bits 128 --sizes 1MiB --backends computed --ops 2 --ref none --runs 30 --csv "$O\bench_computed_1MiB.csv"
```

```powershell
# (3b) computed ở 100 MiB, 1 thao tác mỗi khối (lệch protocol có chủ ý)   (~1,7 giờ; thêm --dir enc để chỉ đo mã hóa, ~50 phút)
& $R bench --bits 128 --sizes 100MiB --backends computed --ops 1 --ref none --runs 30 --csv "$O\bench_computed_100MiB.csv"
```

(`computed` ở 1 GiB không khả thi: ~17 phút mỗi thao tác, 30 lần lặp mã hóa và giải mã ~17 giờ. Đề ghi 1 GiB là "nếu khả thi"; báo cáo ghi rõ lý do.)

```powershell
# (4) TÙY CHỌN, bonus AES-192 và AES-256   (~5 phút mỗi lệnh)
& $R bench --bits 192 --sizes 1MiB --backends table,ttable,aesni --ref all --runs 30 --csv "$O\bench_aes192.csv"
& $R bench --bits 256 --sizes 1MiB --backends table,ttable,aesni --ref all --runs 30 --csv "$O\bench_aes256.csv"
```

Thời gian ước tính từ tốc độ đã đo thử trên máy này (pure table ~80-110 MB/s, `computed` ~1 MB/s); tổng cộng khoảng 45-60 phút mỗi hệ điều hành. Đây là **ước tính**, chưa đo đủ.

## 4. Linux: chạy benchmark (đúng cùng lệnh)

```bash
R=./build_ubuntu_gcc/aestool_ref; O=output/ubuntu/bench
$R bench --bits 128 --sizes 1MiB --backends table --ref all --runs 30 --csv $O/bench_aes128_1MiB.csv
$R bench --bits 128 --sizes 100MiB --backends table --ref all --runs 30 --csv $O/bench_aes128_100MiB.csv
$R bench --bits 128 --sizes 1GiB --backends table --ref all --runs 30 --csv $O/bench_aes128_1GiB.csv
$R bench --bits 128 --sizes 4KiB --backends table,computed --ref none --runs 30 --csv $O/bench_sbox_4KiB.csv
$R bench --bits 128 --sizes 1MiB --backends computed --ops 2 --ref none --runs 30 --csv $O/bench_computed_1MiB.csv
$R bench --bits 128 --sizes 100MiB --backends computed --ops 1 --ref none --runs 30 --csv $O/bench_computed_100MiB.csv     # ~1,7 giờ; thêm --dir enc nếu muốn ~50 phút
$R bench --bits 192 --sizes 1MiB --backends table,ttable,aesni --ref all --runs 30 --csv $O/bench_aes192.csv
$R bench --bits 256 --sizes 1MiB --backends table,ttable,aesni --ref all --runs 30 --csv $O/bench_aes256.csv
```

## 5. Ghi lại môi trường

Windows và Linux: log `env_<os>.txt` đã có trong `output/<os>/` (tạo bằng lệnh ở [TEST_EVIDENCE.md](TEST_EVIDENCE.md) mục 5). Khi đo chính thức trên máy Linux thật,
chạy lại lệnh đó để `env_ubuntu.txt` ghi đúng governor và phần cứng (bản hiện có được tạo trong WSL2: không có `cpufreq`).

## 6. Đưa kết quả về một chỗ, vẽ biểu đồ

Linux:

```bash
cd ~/crypto-labs && git add lab2/output/ubuntu && git commit -m "Lab 2: Ubuntu benchmark results" && git push origin main
```

Windows:

```powershell
git pull
python scripts\plot_bench.py output\windows\bench --out output\windows\plots
python scripts\plot_bench.py output\ubuntu\bench --out output\ubuntu\plots
python ..\scripts\csv_to_stats_table.py output\windows\bench\bench_aes128_1MiB.csv --dir enc > output\windows\stats_aes128_1MiB_enc.md
```

Sau khi có đủ CSV, mình sinh bảng thống kê, biểu đồ so sánh và `lab2-main-report.pdf`.

## 7. Danh sách kiểm tra trước khi đo

- [ ] Hai máy cùng commit (`git rev-parse HEAD`) và cùng `kat/vectors.json` (SHA-256 trùng: xem `env_*.txt`).
- [ ] `ctest` 4/4 trên cả hai hệ điều hành.
- [ ] Cùng một máy vật lý, đã cắm điện; Windows **High performance**, Linux governor **performance**.
- [ ] Không chạy việc khác trong lúc đo; chạy tuần tự từng lệnh.
- [ ] Đúng tên các file CSV ở mục 3 và 4: bắt buộc `bench_aes128_{1MiB,100MiB,1GiB}`, `bench_sbox_4KiB`, `bench_computed_{1MiB,100MiB}`; tùy chọn (bonus) `bench_aes192`, `bench_aes256`.
- [ ] Linux chạy **đúng cùng bộ lệnh** như Windows (cùng `--backends`, `--ops`, `--dir`).
- [ ] (Khuyến nghị) Đo mỗi hệ điều hành 2 lần và so hai file; nếu chênh quá vài phần trăm, kiểm tra nhiệt độ, nguồn điện, tiến trình nền.

## 8. Xử lý sự cố

| Triệu chứng | Cách xử lý |
|---|---|
| `aestool_ref` không có sau khi build | Chưa cài `libssl-dev`/`libcrypto++-dev` (Linux) hoặc `openssl`/`crypto++` (MSYS2); cài rồi chạy lại `cmake ..` |
| `computed` chạy quá lâu | Đừng bỏ `--ops 2` ở lệnh 3; lệnh 1 không có `computed` |
| Bị treo ở 1 GiB | Thiếu RAM: bỏ `1GiB` khỏi `--sizes` |
| Số đo dao động lớn | Nguồn điện, nhiệt độ, tiến trình nền; để máy nguội 1 phút rồi đo lại |
| MSVC: link lỗi `_ITERATOR_DEBUG_LEVEL` / `RuntimeLibrary` | Thêm `-DCMAKE_BUILD_TYPE=Release` khi dùng Ninja trong Developer Prompt |
