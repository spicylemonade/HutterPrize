#!/usr/bin/env bash
set -euo pipefail

bash build.sh >/dev/null

# Create a synthetic input exercising tokens and runs
cat > toy.txt << 'EOF'
[[Category:Ablation]] {{Infobox}} ---- ===== 20250101
<ref name="x"/> &amp; &lt; &gt; https://en.wikipedia.org/wiki/Compression
Paragraphs:\n\n\n          (spaces) and digits 1234567890 repeated 1234567890.
EOF

run_case() {
  local name="$1"; shift
  ./comp --method=store "$@" toy.txt toy.${name}.arc >/dev/null
  chmod -f +x toy.${name}.arc || true
  rm -f enwik9.out || true
  ./toy.${name}.arc >/dev/null
  if cmp -s toy.txt enwik9.out; then
    echo "[OK] ablate ${name}: round-trip passed"
  else
    echo "[ERROR] ablate ${name}: round-trip FAILED" >&2
    exit 1
  fi
  rm -f toy.${name}.arc enwik9.out
}

run_case all
run_case no_dict         --no-dict
run_case no_space        --no-space-run
run_case no_nl           --no-nl-run
run_case no_digits       --no-digit-run
run_case no_dash         --no-dash-run
run_case no_equals       --no-equals-run
run_case none            --no-transform

rm -f toy.txt
