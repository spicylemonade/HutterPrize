#!/usr/bin/env bash
set -euo pipefail

if [ ! -x comp ] || [ ! -x archive ]; then
  echo "[ERROR] Build artifacts not found. Run ./build.sh and then ./comp enwik9 archive." >&2
  exit 1
fi
S1=$(stat -c%s comp)
S2=$(stat -c%s archive)
S=$((S1+S2))
L=110793128
T=109685196

echo "comp size (S1):   ${S1}"
echo "archive size (S2): ${S2}"
echo "Total S:           ${S}"

if [ ${S} -lt ${L} ]; then
  echo "[INFO] S < L (record threshold)"
else
  echo "[INFO] S >= L (not yet a record)"
fi
if [ ${S} -le ${T} ]; then
  echo "[INFO] S <= 0.99*L (>=1% improvement)."
else
  echo "[INFO] S > 0.99*L (not yet prize-eligible)."
fi