#!/usr/bin/env bash
set -euo pipefail

bash build.sh >/dev/null

if [ ! -f enwik9 ]; then
  echo "[INFO] enwik9 not found; run ./verify.sh to fetch it. Skipping full ablation." >&2
  exit 0
fi

measure_case() {
  local name="$1"; shift
  local args=("$@")
  local out="archive.${name}"
  ./comp "${args[@]}" enwik9 "$out" >/dev/null
  chmod -f +x "$out" || true
  # size S2
  local S2
  if command -v stat >/dev/null 2>&1; then
    S2=$(stat -c%s "$out" 2>/dev/null || stat -f%z "$out")
  else
    S2=$(wc -c < "$out")
  fi
  echo "${name},${S2}"
  rm -f "$out"
}

echo "name,S2_bytes"
measure_case base --method=zlib
measure_case no_dict --method=zlib --no-dict
measure_case no_space --method=zlib --no-space-run
measure_case no_nl --method=zlib --no-nl-run
measure_case no_digits --method=zlib --no-digit-run
measure_case no_dash --method=zlib --no-dash-run
measure_case no_equals --method=zlib --no-equals-run
measure_case none --method=zlib --no-transform
