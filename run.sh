#!/usr/bin/env bash
# ==============================================================================
# Linux/Ubuntu Runner Wrapper for NT219 Crypto Labs
# ==============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Ensure crypto-env is prioritized if present
if [[ -d "/home/lngu/crypto-env/bin" ]]; then
    export PATH="/home/lngu/crypto-env/bin:$PATH"
fi

if command -v python3 >/dev/null 2>&1; then
    exec python3 "${SCRIPT_DIR}/run.py" "$@"
elif command -v python >/dev/null 2>&1; then
    exec python "${SCRIPT_DIR}/run.py" "$@"
fi

echo "[ERROR] Python 3 is required to run the unified test and benchmark runner." >&2
exit 1
