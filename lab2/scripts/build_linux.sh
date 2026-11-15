#!/usr/bin/env bash
# build_linux.sh - Build + test Lab 2 (aestool) on Linux (Ubuntu LTS / Debian).
#
# Usage (from anywhere):
#     lab2/scripts/build_linux.sh [gcc|clang] [--clean] [--install-deps]
#
# What it does: configure (out-of-source, build dir "build_ubuntu_<compiler>"), build, ctest, run the KAT file.
# It is equivalent to:   mkdir build && cd build && cmake .. && cmake --build . && ctest
#
# Dependencies (Ubuntu):   sudo apt install -y build-essential cmake libssl-dev libcrypto++-dev catch2
#                          (--install-deps runs exactly this; catch2 is optional: CMake downloads it if missing;
#                           libssl-dev / libcrypto++-dev are ONLY for aestool_ref = cross-validation + comparison)
# No path needs editing. Optional overrides (environment variables):
#     CRYPTOPP_ROOT_DIR   install prefix of a self-built Crypto++ (headers in include/, library in lib/)
#     OPENSSL_ROOT_DIR    install prefix of a self-built OpenSSL
#     CMAKE_GENERATOR     e.g. Ninja (default: CMake's default, normally "Unix Makefiles")

set -euo pipefail

LAB_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$LAB_DIR"

COMPILER="gcc"
CLEAN=0
INSTALL=0
for arg in "$@"; do
    case "$arg" in
        gcc|clang) COMPILER="$arg" ;;
        --clean) CLEAN=1 ;;
        --install-deps) INSTALL=1 ;;
        -h|--help) sed -n '2,15p' "$0"; exit 0 ;;
        *) echo "[ERROR] Unknown argument: $arg"; exit 1 ;;
    esac
done

if [[ $INSTALL -eq 1 ]]; then
    echo "[INFO] Installing dependencies (needs sudo)..."
    sudo apt update
    sudo apt install -y build-essential cmake libssl-dev libcrypto++-dev catch2 pkg-config
    [[ "$COMPILER" == "clang" ]] && sudo apt install -y clang
fi

command -v cmake >/dev/null || { echo "[ERROR] cmake not found. Run with --install-deps"; exit 1; }

BUILD_DIR="build_ubuntu_${COMPILER}"
CXX_ARG=()
if [[ "$COMPILER" == "clang" ]]; then
    command -v clang++ >/dev/null || { echo "[ERROR] clang++ not found"; exit 1; }
    CXX_ARG=(-DCMAKE_CXX_COMPILER=clang++)
fi
ROOT_ARG=()
[[ -n "${CRYPTOPP_ROOT_DIR:-}" ]] && ROOT_ARG+=(-DCRYPTOPP_ROOT_DIR="$CRYPTOPP_ROOT_DIR")
[[ -n "${OPENSSL_ROOT_DIR:-}" ]] && ROOT_ARG+=(-DOPENSSL_ROOT_DIR="$OPENSSL_ROOT_DIR")
GEN_ARG=()
[[ -n "${CMAKE_GENERATOR:-}" ]] && GEN_ARG=(-G "$CMAKE_GENERATOR")

[[ $CLEAN -eq 1 ]] && rm -rf "$BUILD_DIR"

echo "== 1/4 Configure ($COMPILER, build dir: $BUILD_DIR) =="
cmake -S . -B "$BUILD_DIR" "${GEN_ARG[@]}" "${CXX_ARG[@]}" "${ROOT_ARG[@]}" -DCMAKE_BUILD_TYPE=Release

echo "== 2/4 Build =="
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "== 3/4 Test (ctest) =="
(cd "$BUILD_DIR" && ctest --output-on-failure)

echo "== 4/4 KAT (Known Answer Tests) =="
"$BUILD_DIR/aestool" --kat kat/vectors.json

if [[ -x "$BUILD_DIR/aestool_ref" ]]; then
    echo "== 4b/4 Cross-validation vs OpenSSL / Crypto++ (aestool_ref) =="
    "$BUILD_DIR/aestool_ref" xval --ref all
fi

echo
echo "SUCCESS! Binary: $LAB_DIR/$BUILD_DIR/aestool"
