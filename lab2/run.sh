#!/usr/bin/env bash
# ==============================================================================
# Runner for Lab 2 (Pure C++ AES-128 CTR)
# ==============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

exec "${ROOT_DIR}/run.sh" "$@" --lab lab2
