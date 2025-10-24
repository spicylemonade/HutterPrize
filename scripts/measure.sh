#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: scripts/measure.sh [comp_path] [archive_path]

Computes S1, S2, and S = S1+S2, and checks thresholds:
  L = 110,793,128 (record to beat)
  L_1% = 109,685,196 (≥1% improvement)

If files are missing, prints a helpful message and exits 0.
USAGE
}

if [[ ${1-} == "-h" || ${1-} == "--help" ]]; then
  usage; exit 0
fi

COMP=${1:-comp}
ARCHIVE=${2:-archive}

if ! command -v stat >/dev/null 2>&1; then
  echo "stat not found; cannot measure sizes." >&2
  exit 1
fi

if [[ ! -f "$COMP" ]]; then
  echo "Missing: $COMP (build your compressor)."
  exit 0
fi
if [[ ! -f "$ARCHIVE" ]]; then
  echo "Missing: $ARCHIVE (run './$COMP enwik9 $ARCHIVE' to produce it)."
  exit 0
fi

S1=$(stat -c%s "$COMP")
S2=$(stat -c%s "$ARCHIVE")
S=$((S1 + S2))
L=110793128
L99=109685196

echo "S1 (comp):    ${S1} bytes"
echo "S2 (archive): ${S2} bytes"
echo "S total:      ${S} bytes"

echo "Record to beat (L):      ${L}"
echo "≥1% improvement (L_1%): ${L99}"

if (( S < L )); then echo "Status vs record: PASS"; else echo "Status vs record: FAIL"; fi
if (( S <= L99 )); then echo "Status vs ≥1%:   PASS"; else echo "Status vs ≥1%:   FAIL"; fi
