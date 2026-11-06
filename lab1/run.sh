#!/usr/bin/env bash
# ==============================================================================
# Runner for Lab 1 (Symmetric Encryption with Crypto++)
# ==============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

if [[ -d "/home/lngu/crypto-env/bin" ]]; then
    export PATH="/home/lngu/crypto-env/bin:$PATH"
fi

exec "${ROOT_DIR}/run.sh" "$@" --lab lab1
