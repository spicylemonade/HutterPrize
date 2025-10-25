#!/usr/bin/env bash
set -euo pipefail

# Build using bash (avoid exec-bit issues)
if command -v bash >/dev/null 2>&1; then
  bash ./build.sh
else
  sh ./build.sh
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

# Transform round-trip tests (unit-like), including chunk-boundary cases and ablations.
CHUNK=$((1<<20))
TMPDIR=$(mktemp -d -t hpzt-rt.XXXXXX)
cleanup() { rm -rf "$TMPDIR" 2>/dev/null || true; }
trap cleanup EXIT

python3 - "$TMPDIR" "$CHUNK" << 'PY'
import os, sys
outdir = sys.argv[1]
CHUNK = int(sys.argv[2])
# t1: Tokens, escapes, long runs
with open(os.path.join(outdir, 't1.bin'), 'wb') as f:
    f.write(b'Hello\x00World\x00\x00End\n')
    f.write(b'[[Category:Test]]{{Infobox}}<ref>12345</ref>')
    f.write(b' ' * 300 + b'X' + b'\n' * 260)
    f.write(b'1234567890' * 40 + b'\n\n')
    f.write(b'|author|title|url|publisher|date|accessdate')
# t2: Boundary straddle for dictionary token and runs
with open(os.path.join(outdir, 't2.bin'), 'wb') as f:
    pre = b'A' * (CHUNK - 6)
    f.write(pre)
    f.write(b'[[Cate')            # end of chunk
    f.write(b'gory:Science]]\n') # begin next chunk
    f.write(b' ' * 300 + b'\n' * 260)
    f.write(b'0' * 300)
# t3: 0x00 heavy to test escaping + hex runs
with open(os.path.join(outdir, 't3.bin'), 'wb') as f:
    f.write(b'\x00' * 10 + b'END' + b'\n')
    f.write(b'deadBEEFCAFEBABE0123ABCDEFabcdef')
print(outdir)
PY

pushd "$TMPDIR" >/dev/null
for f in t1.bin t2.bin t3.bin; do
  for TF in all dict space nl digits hex "space,nl,digits" "digits,hex" none; do
    # Compose transform args
    TARG="--transforms=${TF}"
    if [ "$TF" = "none" ]; then TARG="--no-transform"; fi
    for M in store zlib; do
      echo "[TEST] Round-trip $f (method=$M, transforms=$TF)" >&2
      ../comp --method=$M $TARG "$f" "$f.$M.$(echo "$TF" | tr ',' '_').arc"
      ARC="$f.$M.$(echo "$TF" | tr ',' '_').arc"
      chmod +x "$ARC" || true
      if [ -n "$TIME_CMD" ]; then $TS_PREFIX $TIME_CMD "./$ARC"; else $TS_PREFIX "./$ARC"; fi
      if ! cmp -s "$f" enwik9.out; then echo "[ERROR] Round-trip mismatch on $f (method=$M, transforms=$TF)" >&2; exit 1; fi
      rm -f enwik9.out
    done
  done
  echo "[OK] $f round-trips under multiple transform ablations (STORE/ZLIB)" >&2
done
popd >/dev/null

# Compress -> archive (full enwik9)
echo "[INFO] Running compressor -> archive" >&2
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
S1=$(stat -c%s comp 2>/dev/null || stat -f%z comp)
S2=$(stat -c%s archive 2>/dev/null || stat -f%z archive)
S=$((S1+S2))
L=110793128
T=109685196

echo "S1 (comp):    ${S1}"
echo "S2 (archive): ${S2}"
echo "S  (total):   ${S} (threshold L=${L}, 1%=${T})"
if [ ${S} -lt ${L} ]; then echo "[INFO] S < L (record threshold)"; else echo "[INFO] S >= L (not yet a record)"; fi
if [ ${S} -le ${T} ]; then echo "[INFO] S <= 0.99*L (>=1% improvement)."; else echo "[INFO] S > 0.99*L (not yet prize-eligible)."; fi
