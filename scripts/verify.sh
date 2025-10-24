#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: scripts/verify.sh [options]

Options:
  -n, --dry-run     Do not download, build, or run binaries; just print the plan.
  --no-download     Skip dataset download/unzip even if enwik9 is missing.
  --no-build        Skip invoking scripts/build.sh.
  --keep            Keep outputs (no-op placeholder; outputs are kept by default).
  -h, --help        Show this help.

Environment:
  TASKSET=1             Enforce single-core with taskset -c 0 if available (default 1)
  TIME=/usr/bin/time    Path to GNU time for verbose stats (auto-detected if present)
  ENWIK9=enwik9         Path to input enwik9 (default ./enwik9)
  ARCHIVE=archive       Output self-extracting archive path (default ./archive)
  COMP=comp             Compressor executable name (default ./comp)
  OUTDIR=decompress_out Output directory for archive run (default ./decompress_out)
  TIMEOUT=              If set and `timeout` exists, wrap both comp and archive with this timeout (e.g., 1800s)
  TIMEOUT_COMP=         Override timeout for comp only
  TIMEOUT_ARCHIVE=      Override timeout for archive only

What it does:
  1) Ensures enwik9 is present (downloads/unzips if needed).
  2) Builds ./comp from src/ via scripts/build.sh (unless --no-build).
  3) Runs: ./comp enwik9 archive (optionally single-core, timed, and timed-out)
  4) Runs: ./archive in an isolated output directory (no inputs) to write enwik9 or enwik9.out
  5) Verifies bit identity, measures sizes/time on one core, prints summary.

Note: The included sample src/comp.c produces a self-extracting shell script that stores raw input. Replace with a real compressor for actual submissions.
USAGE
}

# Defaults
DRY=0
NO_DL=0
NO_BUILD=0
KEEP=1

# Parse args
while [[ $# -gt 0 ]]; do
  case "$1" in
    -n|--dry-run) DRY=1; shift;;
    --no-download) NO_DL=1; shift;;
    --no-build) NO_BUILD=1; shift;;
    --keep) KEEP=1; shift;;
    -h|--help) usage; exit 0;;
    *) echo "Unknown option: $1" >&2; usage; exit 2;;
  esac
done

ENWIK9=${ENWIK9:-enwik9}
ARCHIVE=${ARCHIVE:-archive}
COMP=${COMP:-comp}
OUTDIR=${OUTDIR:-decompress_out}
TASKSET=${TASKSET:-1}
TIME_BIN=${TIME:-}
TIMEOUT_ALL=${TIMEOUT:-}
TIMEOUT_COMP=${TIMEOUT_COMP:-}
TIMEOUT_ARCHIVE=${TIMEOUT_ARCHIVE:-}

has_cmd() { command -v "$1" >/dev/null 2>&1; }

if [[ -z "$TIME_BIN" && -x /usr/bin/time ]]; then TIME_BIN=/usr/bin/time; fi

echo "[verify] Hutter Prize enwik9 verification harness"
echo "[verify] ENWIK9=$ENWIK9  ARCHIVE=$ARCHIVE  COMP=$COMP  OUTDIR=$OUTDIR"

if [[ "$DRY" -eq 1 ]]; then
  echo "[verify] DRY RUN: will not download/build/run. Exiting after plan."
  echo "[plan] 1) Check dataset (enwik9, 1e9 bytes)."
  echo "[plan] 2) Build ./comp via scripts/build.sh (if sources exist)."
  echo "[plan] 3) Run './comp enwik9 archive' on single core if taskset available (optional timeout)."
  echo "[plan] 4) Run './archive' in an isolated directory to write enwik9 or enwik9.out (optional timeout)."
  echo "[plan] 5) Verify bit identity with cmp; measure sizes with stat; print S and thresholds."
  exit 0
fi

# 1) Dataset
if [[ ! -f "$ENWIK9" ]]; then
  if [[ "$NO_DL" -eq 1 ]]; then
    echo "[verify] ERROR: $ENWIK9 missing and --no-download was set." >&2
    exit 3
  fi
  echo "[verify] Fetching enwik9.zip..."
  if ! has_cmd wget; then echo "[verify] ERROR: wget not found." >&2; exit 3; fi
  wget -O enwik9.zip http://mattmahoney.net/dc/enwik9.zip
  echo "[verify] Unzipping enwik9.zip..."
  if ! has_cmd unzip; then echo "[verify] ERROR: unzip not found." >&2; exit 3; fi
  unzip -o enwik9.zip
fi

if [[ ! -f "$ENWIK9" ]]; then
  echo "[verify] ERROR: $ENWIK9 not found after unzip." >&2
  exit 3
fi

bytes=$(wc -c < "$ENWIK9" || true)
if [[ "$bytes" != "1000000000" ]]; then
  echo "[verify] ERROR: $ENWIK9 has $bytes bytes, expected 1000000000." >&2
  exit 4
fi

# 2) Build
if [[ "$NO_BUILD" -ne 1 ]]; then
  if [[ -x scripts/build.sh ]]; then
    bash scripts/build.sh
  else
    echo "[verify] WARN: scripts/build.sh not found; skipping build."
  fi
fi

if [[ ! -x "./$COMP" ]]; then
  echo "[verify] ERROR: $COMP not found or not executable. Implement src/comp.c or src/comp.cpp and run scripts/build.sh." >&2
  exit 5
fi

# Helper to form prefixes
RUNPREFIX=()
if [[ "$TASKSET" = "1" ]] && has_cmd taskset; then
  RUNPREFIX+=(taskset -c 0)
fi
TIMEPREFIX=()
if [[ -n "$TIME_BIN" ]]; then
  TIMEPREFIX+=("$TIME_BIN" -v)
fi
maybe_timeout() {
  local secs="$1"
  if [[ -n "$secs" ]] && has_cmd timeout; then
    echo timeout "$secs"
  else
    echo
  fi
}

# 3) Run comp (single core, optional timeout)
T_COMP=${TIMEOUT_COMP:-${TIMEOUT_ALL:-}}
TP_COMP=( $(maybe_timeout "$T_COMP") )

set -x
"${RUNPREFIX[@]}" "${TP_COMP[@]}" "${TIMEPREFIX[@]}" "./$COMP" "$ENWIK9" "$ARCHIVE"
set +x

if [[ ! -x "./$ARCHIVE" ]]; then
  echo "[verify] ERROR: $ARCHIVE was not created or not executable by comp." >&2
  exit 6
fi

# 4) Run archive (no inputs) in isolated directory
rm -rf "$OUTDIR"
mkdir -p "$OUTDIR"
pushd "$OUTDIR" >/dev/null

T_ARCH=${TIMEOUT_ARCHIVE:-${TIMEOUT_ALL:-}}
TP_ARCH=( $(maybe_timeout "$T_ARCH") )

set -x
"${RUNPREFIX[@]}" "${TP_ARCH[@]}" "${TIMEPREFIX[@]}" "../$ARCHIVE"
set +x

OUT=""
if [[ -f enwik9 ]]; then OUT="enwik9"; fi
if [[ -z "$OUT" && -f enwik9.out ]]; then OUT="enwik9.out"; fi
if [[ -z "$OUT" ]]; then
  if command -v find >/dev/null 2>&1; then
    cand=$(find . -maxdepth 1 -type f -printf '%s\t%p\n' 2>/dev/null | sort -nr | head -n1 | cut -f2- | sed 's#^./##') || true
    if [[ -n "${cand:-}" && -f "$cand" ]]; then OUT="$cand"; fi
  fi
fi
if [[ -z "$OUT" ]]; then
  echo "[verify] ERROR: archive did not produce enwik9 or enwik9.out (or any file)." >&2
  popd >/dev/null
  exit 7
fi

# 5) Verify bit identity
if ! cmp -s "../$ENWIK9" "$OUT"; then
  echo "[verify] ERROR: Output ($OUT) differs from ../$ENWIK9." >&2
  popd >/dev/null
  exit 8
fi
popd >/dev/null

echo "[verify] OK: $OUTDIR/$OUT is bit-identical to $ENWIK9."

# 6) Sizes
if ! command -v stat >/dev/null 2>&1; then
  echo "[verify] WARN: stat not found; skipping size measurement."
  exit 0
fi
S1=$(stat -c%s "$COMP")
S2=$(stat -c%s "$ARCHIVE")
S=$((S1 + S2))
L=110793128
L99=109685196
status_record="FAIL"
status_prize="FAIL"
[[ $S -lt $L ]] && status_record="PASS"
[[ $S -le $L99 ]] && status_prize="PASS"

echo "[verify] Sizes: S1=$S1 bytes (comp), S2=$S2 bytes (archive), S=$S bytes (total)"
echo "[verify] Thresholds: L=$L; 1% target=$L99"
echo "[verify] Record: $status_record; Prize≥1%: $status_prize"

echo "[verify] Done."
