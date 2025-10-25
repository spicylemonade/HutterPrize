#!/usr/bin/env bash
set -euo pipefail

# Build via bash to avoid exec-bit dependency
bash build.sh >/dev/null

# Create a tiny input exercising tokens, runs, and 0x00 escapes
cat > tiny.txt << 'EOF'
Hello
[[Category:Testing]] and {{Infobox}} entries.
Five spaces here:     end.
Two newlines follow:

Then three newlines:


Digits: 20250101 and 1234567890.
Horizontal rule: ------
Heading bar: ======
A NUL follows -> 
EOF
# Append a literal NUL byte and some text after it
printf '\x00AFTER-NUL' >> tiny.txt
printf '\n' >> tiny.txt

# Compress with STORE to isolate transforms
./comp --method=store tiny.txt tiny.arc >/dev/null
chmod +x tiny.arc

# Decompress (writes enwik9.out)
./tiny.arc >/dev/null

# Compare
if cmp -s tiny.txt enwik9.out; then
  echo "[OK] Transform round-trip (STORE) passed"
else
  echo "[ERROR] Transform round-trip failed" >&2
  exit 1
fi

# Cleanup artifacts specific to this test
rm -f tiny.arc tiny.txt enwik9.out
