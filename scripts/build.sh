#!/usr/bin/env bash
set -euo pipefail

CC=${CC:-cc}
CXX=${CXX:-g++}
CFLAGS=${CFLAGS:--O3 -pipe -s}
CXXFLAGS=${CXXFLAGS:--O3 -pipe -s}
LDFLAGS=${LDFLAGS:-}

echo "[build] Starting build..."

built=0
if [[ -f src/comp.c ]]; then
  echo "[build] Compiling comp from src/comp.c"
  $CC $CFLAGS -o comp src/comp.c $LDFLAGS
  built=1
fi
if [[ -f src/comp.cpp ]]; then
  echo "[build] Compiling comp from src/comp.cpp"
  $CXX $CXXFLAGS -o comp src/comp.cpp $LDFLAGS
  built=1
fi

if [[ -f src/archive.c ]]; then
  echo "[build] Compiling dev decompressor (not used by prize run) from src/archive.c -> archive.dev"
  $CC $CFLAGS -o archive.dev src/archive.c $LDFLAGS || true
fi
if [[ -f src/archive.cpp ]]; then
  echo "[build] Compiling dev decompressor (not used by prize run) from src/archive.cpp -> archive.dev"
  $CXX $CXXFLAGS -o archive.dev src/archive.cpp $LDFLAGS || true
fi

if [[ "$built" -eq 0 ]]; then
  echo "[build] No comp sources found. Add src/comp.c or src/comp.cpp."
fi

echo "[build] Done."
