import os
import re
from pathlib import Path

# 1. Update Lab 1 Report
ROOT = Path(__file__).resolve().parent.parent.parent
lab1_md = ROOT / "lab1" / "docs" / "lab1-main-report.md"
with open(lab1_md, "r", encoding="utf-8") as f:
    text1 = f.read()

# Replace any lingering github URLs with relative paths
text1 = text1.replace("https://github.com/Feph50/crypto-labs/tree/main/lab1/", "../")

# Update GUI section with screenshots
gui_old_pattern = r"(## 8\.2\. Hướng phát triển mở rộng \(\+5 điểm thưởng\).*?2\. \*\*Xây dựng Giao diện Đồ họa trực quan.*?\n)(3\. \*\*Cơ chế Streaming I/O)"
gui_new_content = r"""## 8.2. Hướng phát triển mở rộng (+5 điểm thưởng)
1. **Đóng gói lõi mã hóa thành Thư viện Đa nền tảng:**
   - Cấu trúc module [`aescore`](../src/aescore.hpp) hiện tại đã sẵn sàng để xuất thành thư viện liên kết động: `.dll` trên Windows và `.so` trên Linux.
   - Bổ sung định nghĩa `API_EXPORT` chuẩn C-linkage (`extern "C"`) cho phép các ngôn ngữ bậc cao như Python (qua `ctypes`), C# (.NET P/Invoke) hoặc Rust nạp và gọi trực tiếp các hàm mã hóa lõi mà không cần viết lại giải thuật.
2. **Xây dựng Giao diện Đồ họa trực quan (PyQt6 GUI — Đã hoàn thành):**
   - Đã phát triển ứng dụng GUI hiện đại tại [`lab1/gui/aestool_gui.py`](../gui/aestool_gui.py) bằng **PyQt6**, tích hợp hệ thống chủ đề Sáng / Tối (Light / Dark theme), hỗ trợ trực quan 8 chế độ hoạt động của AES, cảnh báo đỏ nguy hiểm khi chọn chế độ ECB, tự động tính toán kích thước tập tin thời gian thực và xem trước lệnh CLI.
   - Tab Tools độc lập cho phép: sinh khóa an toàn CSPRNG (128/192/256-bit), sinh IV ngẫu nhiên, chạy kiểm thử tự động 9 NIST KAT vectors và thực thi Benchmark thời gian thực hiển thị tiến trình.

![Giao diện trực quan PyQt6 - Form mã hóa AES đa chế độ](../output/windows/screenshots/lab1_gui_encrypt.png)
*Hình 12: Giao diện trực quan PyQt6 (Tab Encrypt/Decrypt) hỗ trợ 8 mode AES, cấu hình IV/Key, kiểm tra fail-closed và xem trước lệnh CLI.*

![Giao diện trực quan PyQt6 - Công cụ KAT & Benchmark](../output/windows/screenshots/lab1_gui_tools.png)
*Hình 13: Giao diện trực quan PyQt6 (Tab Tools) tích hợp bộ sinh khóa CSPRNG, runner kiểm chứng NIST KAT và đo đạc benchmark trực quan.*

3. **Cơ chế Streaming I/O"""

text1_updated = re.sub(r"## 8\.2\. Hướng phát triển mở rộng.*?(?=3\. \*\*Cơ chế Streaming I/O)", gui_new_content, text1, flags=re.DOTALL)
if text1_updated != text1:
    with open(lab1_md, "w", encoding="utf-8") as f:
        f.write(text1_updated)
    print("Updated Lab 1 report with GUI screenshots!")
else:
    print("Lab 1 GUI section regex did not match, writing directly...")
    # fallback search
    idx = text1.find("## 8.2. Hướng phát triển mở rộng")
    if idx != -1:
        end_idx = text1.find("3. **Cơ chế Streaming I/O", idx)
        if end_idx != -1:
            text1 = text1[:idx] + gui_new_content + text1[end_idx + len("3. **Cơ chế Streaming I/O"):]
            with open(lab1_md, "w", encoding="utf-8") as f:
                f.write(text1)
            print("Fallback updated Lab 1 report with GUI screenshots!")

print("Lab 1 Report processing completed.")
