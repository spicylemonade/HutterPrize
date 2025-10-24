#!/usr/bin/env bash
set -euo pipefail

# Build comp
bash scripts/build.sh

# Create a tiny sample payload
printf 'Hello Hutter Prize\n' > sample.bin

# Produce a self-extracting archive
./comp sample.bin archive

# Run the archive; write output as out.bin to avoid clobbering any dataset
HP_OUT=out.bin ./archive

# Verify round-trip
if cmp -s sample.bin out.bin; then
  echo "[demo] Round-trip OK: archive reproduced sample.bin"
else
  echo "[demo] ERROR: archive output differs from sample.bin" >&2
  exit 1
fi

# Show sizes
bash scripts/measure.sh comp archive || true
