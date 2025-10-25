#!/usr/bin/env bash
set -euo pipefail

# Build via bash to avoid exec-bit dependency
bash build.sh >/dev/null

# Generate ~3 MiB synthetic file with patterns that cross 1 MiB chunk boundaries
python3 - << 'PY'
import os
IN_CHUNK = 1<<20
out = bytearray()
pat = [
  b'[[Category:BoundaryTest]]',
  b'https://en.wikipedia.org/wiki/Main_Page',
  b'\x3cref name="foo"/\x3e',
  b'== References ==',
  b'          ',            # 10 spaces
  b'\n\n\n\n\n',           # 5 newlines
  b'1234567890123',        # 13 digits
  b'----------------------',   # long dashes
  b'========================'  # long equals
]
# Helper to force a token to straddle the next chunk boundary roughly in half
for k in range(3):
    for t in pat:
        half = len(t)//2
        pad = (IN_CHUNK - ((len(out) + half) % IN_CHUNK)) % IN_CHUNK
        out += b'X' * pad
        out += t
        out += b'YZZ'
with open('boundary.dat','wb') as f:
    f.write(out)
print(len(out))
PY

# 1) STORE: isolate transforms
./comp --method=store boundary.dat boundary.arc >/dev/null
chmod +x boundary.arc
rm -f enwik9.out || true
./boundary.arc >/dev/null
cmp -s boundary.dat enwik9.out && echo "[OK] Boundary test (STORE) passed" || { echo "[ERROR] Boundary test (STORE) failed" >&2; exit 1; }
rm -f boundary.arc enwik9.out

# 2) ZLIB path (falls back to STORE if zlib missing)
./comp --method=zlib boundary.dat boundary.zarc >/dev/null || true
chmod +x boundary.zarc || true
rm -f enwik9.out || true
./boundary.zarc >/dev/null || true
cmp -s boundary.dat enwik9.out && echo "[OK] Boundary test (ZLIB/fallback) passed" || { echo "[ERROR] Boundary test (ZLIB/fallback) failed" >&2; exit 1; }
rm -f boundary.zarc boundary.dat enwik9.out
