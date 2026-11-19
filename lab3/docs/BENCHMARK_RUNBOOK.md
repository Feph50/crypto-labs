# Lab 3 — Runbook benchmark (Windows và Ubuntu 24.04, cùng cấu hình)

Bạn chạy benchmark trên **Windows** rồi sang **Ubuntu 24.04** với **đúng cùng lệnh và tham số**. Mọi lệnh gõ trực tiếp (PowerShell / bash), không dùng file `.sh` hay `.bat`.
Chạy từ thư mục `lab3` của repo. Chạy **tuần tự**, không chạy hai benchmark cùng lúc, không làm việc khác trên máy.

## 0. Đề yêu cầu gì về benchmark

| Nội dung | Nguồn |
|---|---|
| RSA: thời gian sinh khóa, mã hóa, giải mã; so sánh 3072 và 4096 | Đề Lab 3 |
| Chế độ lai: thông lượng AES-GCM; thời gian mã hóa lai cho 1 KiB, 1 MiB, 100 MiB | Đề Lab 3 |
| Phân tích: giải mã RSA so với mã hóa RSA, cỡ khóa so với hiệu năng, ưu thế của hybrid, vì sao đối xứng áp đảo thông lượng, bộ nhớ và CPU; biểu đồ độ trễ và thông lượng | Đề Lab 3 |
| Warm-up 1-2 s; khối ~1000 thao tác; 30-100 lần lặp; mean, median, SD, CI 95%; bảng phần cứng/OS; governor / power plan | Quy tắc chung của đề |
| Lệnh cụ thể và tham số bên dưới | **Lựa chọn của dự án** (đề không quy định) |

Độ lệch protocol có chủ ý (ghi trong báo cáo): sinh khóa RSA tốn thời gian ngẫu nhiên nên đo **một khóa mỗi lần lặp** (30 khóa), không theo khối 1000 thao tác;
các thao tác chậm (giải mã RSA, hybrid) dùng khối khoảng 1,5 giây (ít hơn 1000 thao tác); số thao tác thực tế ghi ở cột `ops` của CSV.

## 1. Hợp đồng đo (giống hệt giữa hai hệ điều hành)

| Hạng mục | Windows | Ubuntu |
|---|---|---|
| Mã nguồn | commit `X` | **cùng commit `X`** (`git rev-parse HEAD`) |
| Compiler | MinGW-w64 GCC (binary tĩnh dựng sẵn) | GCC (apt), `Release` (`-O3 -DNDEBUG`) |
| Crypto++ | 8.9.0 | 8.9.0 |
| Số lần lặp | `--reps 30` | `--reps 30` |
| Tham số khác | mặc định (warm-up 1,5 s, khối 1,5 s tối đa 1000 thao tác) | giống Windows |
| Phần cứng | **cùng một máy vật lý** | cùng máy (dual boot), **Ubuntu 24.04 cài trực tiếp**, không WSL2 |
| Nguồn điện | cắm điện, **High performance** | cắm điện, governor **performance** |

RAM: các ca 100 MiB cần khoảng 0,5 GiB trống mỗi ca (bản rõ, bản mã và bản sao phát sinh).

## 2. Windows (PowerShell)

Chuẩn bị (không cần build lại: dùng binary tĩnh dựng sẵn `release\windows\rsabench_mingw.exe`):

```powershell
powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c      # High performance; cắm sạc, đóng ứng dụng nặng
cd "D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs-Lnguyen\lab3"
git rev-parse HEAD                                            # ghi lại, phải trùng với Ubuntu
$R = ".\release\windows\rsabench_mingw.exe"; $O = "output\windows\bench"; mkdir -Force $O
& $R --help | Select-Object -First 3                          # phải in phần đầu của hướng dẫn
```

Bốn lệnh đo (mỗi lệnh ghi một CSV; thời gian là **ước tính** từ lần chạy thử nhanh, chưa đo đủ):

```powershell
# (1) Sinh khóa RSA 3072 và 4096: 30 khóa mỗi cỡ   (~1-2 phút)
& $R --suites keygen --reps 30 --out "$O\bench_keygen.csv" --env-out "$O\bench_env.txt"
```

```powershell
# (2) RSA-OAEP mã hóa và giải mã, 3072 và 4096   (~3-4 phút)
& $R --suites rsa --reps 30 --out "$O\bench_rsa.csv"
```

```powershell
# (3) AES-256-GCM, 1 KiB / 1 MiB / 100 MiB, mã hóa và giải mã   (~5 phút)
& $R --suites aesgcm --reps 30 --out "$O\bench_aesgcm.csv"
```

```powershell
# (4) Mã hóa lai (khóa AES mới + AES-GCM + RSA-OAEP), 1 KiB / 1 MiB / 100 MiB, RSA 3072 và 4096, mã hóa và giải mã   (~10 phút)
& $R --suites hybrid --reps 30 --out "$O\bench_hybrid.csv"
```

Tổng khoảng 20 đến 25 phút. Mỗi lệnh in bảng tóm tắt ra màn hình khi chạy.

## 3. Ubuntu 24.04 (bash)

Lấy mã, build và kiểm tra (xem `UBUNTU_FINISH.md` mục 1 và 2 cho cài đặt đầy đủ):

```bash
cd ~/crypto-labs && git pull && git rev-parse HEAD            # phải TRÙNG commit ghi ở Windows
cd lab3
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
mkdir -p build_ubuntu_gcc && cd build_ubuntu_gcc
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
ctest --output-on-failure                                     # mong đợi: 100% tests passed, 0 tests failed out of 3
cd ..
R=./build_ubuntu_gcc/rsabench; O=output/ubuntu/bench; mkdir -p $O
```

Bốn lệnh đo, đúng cùng tham số:

```bash
$R --suites keygen --reps 30 --out $O/bench_keygen.csv --env-out $O/bench_env.txt
$R --suites rsa --reps 30 --out $O/bench_rsa.csv
$R --suites aesgcm --reps 30 --out $O/bench_aesgcm.csv
$R --suites hybrid --reps 30 --out $O/bench_hybrid.csv
```

## 4. Thống kê và biểu đồ

Sau khi có CSV (trên máy nào cũng được; cần `matplotlib`):

```bash
python3 scripts/analyze_bench.py output/windows/bench/bench_keygen.csv output/windows/bench/bench_rsa.csv output/windows/bench/bench_aesgcm.csv output/windows/bench/bench_hybrid.csv --label Windows --out-dir output/windows/analysis
python3 scripts/analyze_bench.py output/ubuntu/bench/bench_keygen.csv output/ubuntu/bench/bench_rsa.csv output/ubuntu/bench/bench_aesgcm.csv output/ubuntu/bench/bench_hybrid.csv --label Ubuntu --out-dir output/ubuntu/analysis
```

Kết quả: `summary.csv`, `summary.md` (bảng mean / median / SD / CI95 để dán vào báo cáo) và 5 biểu đồ có thanh sai số.
Sau đó chỉ cần báo mình để viết báo cáo.

## 5. Danh sách kiểm tra trước khi đo

- [ ] Hai máy cùng commit và cùng `kat/vectors.json` (`git show HEAD:lab3/kat/vectors.json | sha256sum` trùng).
- [ ] `ctest` 3/3 trên cả hai hệ điều hành.
- [ ] Cùng một máy vật lý, đã cắm điện; Windows **High performance**, Linux governor **performance**.
- [ ] Chạy tuần tự từng lệnh; không việc khác trên máy. Bị gián đoạn thì xóa CSV của lệnh đó và chạy lại.
- [ ] Đúng tên các file CSV: `bench_keygen.csv`, `bench_rsa.csv`, `bench_aesgcm.csv`, `bench_hybrid.csv`, `bench_env.txt`.
- [ ] Linux chạy **đúng cùng bộ lệnh** như Windows.

## 6. Xử lý sự cố

| Triệu chứng | Cách xử lý |
|---|---|
| `Permission denied` khi chạy binary trên Linux | dùng binary vừa build ở `build_ubuntu_gcc`, không cần `chmod` |
| Sinh khóa 4096 có thời gian dao động lớn | bình thường: thời gian sinh khóa là biến ngẫu nhiên (tìm số nguyên tố); 30 lần lặp cho khoảng tin cậy rộng |
| Thiếu RAM ở ca 100 MiB | đóng ứng dụng nặng; hoặc chạy `--sizes 1KiB,1MiB` (ghi rõ trong báo cáo) |
| Số đo dao động lớn | nguồn điện, nhiệt độ, tiến trình nền; để máy nguội 1 phút rồi chạy lại |
