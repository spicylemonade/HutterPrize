#!/usr/bin/env bash
set -euo pipefail
rm -f sample.bin out.bin comp archive archive.dev enwik9.out
rm -rf decompress_out
rm -f docs/results.tex docs/results.json
echo "[clean] Removed demo artifacts, decompress_out, and docs/results.*"
