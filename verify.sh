#!/usr/bin/env bash
set -euo pipefail

# Build via bash to avoid exec-bit issues
bash build.sh

# Run transform self-tests
if [ -x ./hpzt_selftest ]; then
  ./hpzt_selftest
fi
# Run streaming fuzz (bounded iterations)
if [ -x ./hpzt_stream_fuzz ]; then
  ./hpzt_stream_fuzz 123456789 300
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

# Introspect archive (optional)
if [ -x ./hpzt_dump ]; then
  echo "[INFO] Archive details:"
  ./hpzt_dump archive || true
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

# Sizes and report (portable)
bash measure.sh
