#!/usr/bin/env bash
set -euo pipefail

# Always invoke build via bash to avoid permission-bit issues
bash ./build.sh

# Run lightweight self-tests first
if [ -x ./selftest ]; then
  echo "[INFO] Running selftest..." >&2
  ./selftest
  echo "[OK] selftest passed" >&2
fi

# Acquire data if missing
if [ ! -f enwik9 ]; then
  echo "[INFO] Downloading enwik9.zip (~323MB) ..." >&2
  wget -O enwik9.zip http://mattmahoney.net/dc/enwik9.zip
  unzip -o enwik9.zip
fi

# Verify size
size=$(wc -c < enwik9)
if [ "$size" -ne 1000000000 ]; then
  echo "[ERROR] enwik9 size mismatch ($size)" >&2
  exit 1
fi

# Determine time command
TIME_CMD=""
if [ -x /usr/bin/time ]; then
  TIME_CMD="/usr/bin/time -v"
elif command -v gtime >/dev/null 2>&1; then
  TIME_CMD="$(command -v gtime) -v"
fi

# Determine taskset availability
TS_PREFIX=""
if command -v taskset >/dev/null 2>&1; then
  TS_PREFIX="taskset -c 0"
fi

# Compress -> archive
echo "[INFO] Running compressor -> archive"
if [ -n "$TIME_CMD" ]; then
  $TS_PREFIX $TIME_CMD ./comp enwik9 archive
else
  $TS_PREFIX ./comp enwik9 archive
fi

# Decompress
rm -f enwik9.out || true
if [ -n "$TIME_CMD" ]; then
  $TS_PREFIX $TIME_CMD ./archive
else
  $TS_PREFIX ./archive
fi

# Compare
if cmp -s enwik9 enwik9.out; then
  echo "[OK] enwik9.out matches enwik9"
else
  echo "[ERROR] Output mismatch" >&2
  exit 1
fi

# Sizes
if stat -c%s comp >/dev/null 2>&1; then
  S1=$(stat -c%s comp)
  S2=$(stat -c%s archive)
else
  S1=$(stat -f%z comp)
  S2=$(stat -f%z archive)
fi
S=$((S1+S2))
L=110793128
T=109685196

echo "S1 (comp):    ${S1}"
echo "S2 (archive): ${S2}"
echo "S  (total):   ${S} (threshold L=${L}, 1%=${T})"
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
