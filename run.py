#!/usr/bin/env python3
"""
================================================================================
Unified Cross-Platform Runner Script for NT219 Crypto Labs (All Labs)
Supports: Ubuntu LTS / Linux (GCC & Clang) and Windows 10/11 (MSVC & MinGW)
================================================================================
"""

import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
IS_WIN = sys.platform.startswith("win")
OS_NAME = "windows" if IS_WIN else "ubuntu"

def get_bin(lab: str, name: str = "aestool") -> Path | None:
    exe_name = f"{name}.exe" if IS_WIN else name
    candidates = [
        ROOT / "build" / lab / exe_name,
        ROOT / "build" / lab / "Release" / exe_name,
        ROOT / lab / f"build_{OS_NAME}_gcc" / exe_name,
        ROOT / lab / f"build_{OS_NAME}_clang" / exe_name,
        ROOT / lab / "build" / exe_name,
        ROOT / lab / "build" / "bin" / exe_name,
        ROOT / lab / "release" / OS_NAME / (f"{name}_gcc" if not IS_WIN else f"{name}.exe"),
    ]
    for p in candidates:
        if p.is_file() and os.access(p, os.X_OK if not IS_WIN else os.F_OK):
            return p
    return None

def run_cmd(cmd, cwd=None) -> bool:
    cmd_str = " ".join(str(c) for c in cmd) if isinstance(cmd, list) else cmd
    print(f"\n[RUNNING] {cmd_str}")
    res = subprocess.run(cmd, cwd=cwd or ROOT, shell=isinstance(cmd, str))
    if res.returncode != 0:
        print(f"[ERROR] Command exited with code {res.returncode}", file=sys.stderr)
        return False
    return True

# --------------------------------------------------------------------------- Actions
def action_build(clean=False, build_type="Release") -> bool:
    build_dir = ROOT / "build"
    if clean and build_dir.exists():
        print(f"[CLEAN] Removing {build_dir}...")
        shutil.rmtree(build_dir, ignore_errors=True)

    build_dir.mkdir(parents=True, exist_ok=True)
    cmake_cfg = ["cmake", "..", f"-DCMAKE_BUILD_TYPE={build_type}"]
    if not run_cmd(cmake_cfg, cwd=build_dir):
        return False

    cmake_build = ["cmake", "--build", ".", "--config", build_type, "-j"]
    if not run_cmd(cmake_build, cwd=build_dir):
        return False

    print("\n[SUCCESS] Out-of-source build completed successfully.")
    return True

def action_test() -> bool:
    build_dir = ROOT / "build"
    if not build_dir.exists():
        print("[INFO] Build directory does not exist, building first...")
        if not action_build():
            return False

    ctest_cmd = ["ctest", "--output-on-failure"]
    if not run_cmd(ctest_cmd, cwd=build_dir):
        return False

    print("\n[SUCCESS] All unit tests across all labs passed (100%).")
    return True

def action_kat(lab=None) -> bool:
    labs = [lab] if lab else ["lab1", "lab2"]
    success = True
    for l in labs:
        exe = get_bin(l)
        if not exe:
            print(f"[ERROR] Executable for {l} not found. Please run './run.sh build' first.")
            return False
        kat_file = ROOT / l / "kat" / "vectors.json"
        print(f"\n=== Known Answer Tests (KAT) for {l.upper()} ===")
        if not run_cmd([str(exe), "--kat", str(kat_file)]):
            success = False
    return success

def action_demo(lab=None) -> bool:
    labs = [lab] if lab else ["lab1", "lab2"]
    success = True
    for l in labs:
        print(f"\n=== Running Negative Tests & Security Demo for {l.upper()} ===")
        if l == "lab1":
            # Lab 1 negative CLI tests via cmake script
            exe = get_bin("lab1")
            work = ROOT / "build" / "lab1_neg"
            script = ROOT / "lab1" / "tests" / "cli_negative.cmake"
            cmd = ["cmake", f"-DTOOL={exe}", f"-DWORK={work}", "-P", str(script)]
            if not run_cmd(cmd):
                success = False
        elif l == "lab2":
            script = ROOT / "lab2" / "scripts" / "run_kat_and_neg.py"
            if not run_cmd([sys.executable, str(script)]):
                success = False
    return success

def action_bench(lab=None) -> bool:
    labs = [lab] if lab else ["lab1", "lab2"]
    for l in labs:
        exe = get_bin(l)
        if not exe:
            print(f"[ERROR] Executable for {l} not found. Please run './run.sh build' first.")
            return False

        if l == "lab1":
            print("\n=== Running Benchmark for Lab 1 (Multi-mode AES) ===")
            out_csv = ROOT / "lab1" / "output" / OS_NAME / "benchmarks" / f"bench_{OS_NAME}_gcc.csv"
            out_csv.parent.mkdir(parents=True, exist_ok=True)
            run_cmd([str(exe), "bench", "--quick", "--out", str(out_csv)])
            plot_script = ROOT / "lab1" / "scripts" / "plot_bench.py"
            run_cmd([sys.executable, str(plot_script), "--auto"], cwd=ROOT / "lab1")
        elif l == "lab2":
            print("\n=== Running Benchmark for Lab 2 (Pure C++ AES-128 CTR) ===")
            out_csv = ROOT / "lab2" / "Output" / OS_NAME / "bench" / f"bench_lab2_{OS_NAME}.csv"
            out_csv.parent.mkdir(parents=True, exist_ok=True)
            run_cmd([str(exe), "bench", "--sizes", "1MiB,100MiB", "--runs", "10", "--csv", str(out_csv)])
            vis_script = ROOT / "lab2" / "scripts" / "generate_lab2_rich_visuals.py"
            run_cmd([sys.executable, str(vis_script)], cwd=ROOT / "lab2")
    return True

def action_gui(lab="1") -> bool:
    lab_dir = ROOT / f"lab{lab}"
    gui_script = lab_dir / "gui" / "aestool_gui.py"
    if not gui_script.exists():
        print(f"[ERROR] GUI script not found: {gui_script}")
        return False
    print(f"\n[INFO] Launching PyQt6 GUI for Lab {lab}...")
    return run_cmd([sys.executable, str(gui_script)], cwd=lab_dir)

def action_clean() -> bool:
    print("[CLEAN] Cleaning all build artifacts and caches...")
    dirs_to_clean = [
        ROOT / "build",
        ROOT / "lab1" / "build",
        ROOT / "lab1" / "build_ubuntu_gcc",
        ROOT / "lab1" / "build_ubuntu_clang",
        ROOT / "lab2" / "build",
        ROOT / "lab2" / "build_ubuntu_gcc",
        ROOT / "lab2" / "build_ubuntu_clang",
    ]
    for d in dirs_to_clean:
        if d.exists():
            print(f"  Removing {d}")
            shutil.rmtree(d, ignore_errors=True)
    print("[SUCCESS] Clean complete.")
    return True

def main():
    parser = argparse.ArgumentParser(description="Unified Runner for NT219 Crypto Labs")
    parser.add_argument("action", choices=["build", "test", "kat", "bench", "demo", "gui", "clean", "help"],
                        nargs="?", default="help", help="Action to execute")
    parser.add_argument("--lab", choices=["lab1", "lab2", "1", "2"], default=None,
                        help="Target specific lab (lab1 or lab2)")
    parser.add_argument("--clean", action="store_true", help="Clean before build")
    parser.add_argument("--type", default="Release", help="Build type (Release/Debug)")

    args = parser.parse_args()
    target_lab = args.lab
    if target_lab in ("1", "2"):
        target_lab = f"lab{target_lab}"

    if args.action == "help":
        print("""
================================================================================
  NT219 CRYPTO LABS - UNIFIED CLI RUNNER
================================================================================
  Usage: ./run.sh [action] [options]
         python run.py [action] [options]

  Actions:
    build     Build all labs using CMake out-of-source (build/)
    test      Run all unit tests via ctest
    kat       Execute NIST Known Answer Tests for Lab 1 & Lab 2
    demo      Run Negative Tests, Fail-closed checks & Malleability attacks
    bench     Run performance benchmark and generate visual plots
    gui [1|2] Launch interactive PyQt6 GUI for Lab 1 or Lab 2
    clean     Clean all build directories

  Options:
    --lab lab1|lab2   Execute action for a specific lab only
    --clean           Clean build/ directory before building
    --type Release    Set CMAKE_BUILD_TYPE (default: Release)
================================================================================
""")
        return 0

    if args.action == "build":
        return 0 if action_build(clean=args.clean, build_type=args.type) else 1
    elif args.action == "test":
        return 0 if action_test() else 1
    elif args.action == "kat":
        return 0 if action_kat(target_lab) else 1
    elif args.action == "demo":
        return 0 if action_demo(target_lab) else 1
    elif args.action == "bench":
        return 0 if action_bench(target_lab) else 1
    elif args.action == "gui":
        lab_num = "2" if target_lab == "lab2" else "1"
        return 0 if action_gui(lab_num) else 1
    elif args.action == "clean":
        return 0 if action_clean() else 1

if __name__ == "__main__":
    sys.exit(main())
