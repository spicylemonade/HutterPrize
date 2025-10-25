#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
if ! command -v pdflatex >/dev/null 2>&1; then
  echo "[WARN] pdflatex not found; install TeX Live to build the paper." >&2
  exit 0
fi
pdflatex -interaction=nonstopmode paper.tex >/dev/null || { echo "[ERROR] pdflatex failed" >&2; exit 1; }
echo "[OK] Built docs/paper.pdf"
