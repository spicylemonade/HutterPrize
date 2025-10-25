#!/usr/bin/env bash
set -euo pipefail

# Ensure build exists
if [ ! -x ./comp ] || [ ! -x ./archive_stub ]; then
  if command -v bash >/dev/null 2>&1; then bash ./build.sh; else sh ./build.sh; fi
fi

# Build final archive if missing
if [ ! -f ./archive ]; then
  echo "[INFO] No archive found; creating a dummy pass to compute sizes requires an actual run (use verify.sh for full pipeline)." >&2
fi

# Platform-agnostic stat
stat_size() {
  if stat -c%s "$1" >/dev/null 2>&1; then stat -c%s "$1"; else stat -f%z "$1"; fi
}

if [ ! -f comp ] || [ ! -f archive ]; then
  echo "[WARN] comp or archive missing. Run: ./comp enwik9 archive first (or ./verify.sh)." >&2
fi

S1=$(stat_size comp 2>/dev/null || echo 0)
S2=$(stat_size archive 2>/dev/null || echo 0)
S=$((S1+S2))
L=110793128
T=109685196

echo "comp size (S1):   ${S1}"
echo "archive size (S2): ${S2}"
echo "Total S:           ${S}"

if [ ${S} -lt ${L} ]; then echo "[INFO] S < L (record threshold)"; else echo "[INFO] S >= L (not yet a record)"; fi
if [ ${S} -le ${T} ]; then echo "[INFO] S <= 0.99*L (>=1% improvement)."; else echo "[INFO] S > 0.99*L (not yet prize-eligible)."; fi
