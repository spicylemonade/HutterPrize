#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
if ! command -v pdflatex >/dev/null 2>&1; then
  echo "pdflatex not found. Please install a LaTeX distribution (e.g., TeX Live)." >&2
  exit 1
fi
pdflatex -interaction=nonstopmode -halt-on-error paper.tex >/dev/null
pdflatex -interaction=nonstopmode -halt-on-error paper.tex >/dev/null
echo "Built docs/paper.pdf"
