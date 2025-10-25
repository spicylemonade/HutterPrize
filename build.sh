#!/usr/bin/env bash
set -euo pipefail

CXX=${CXX:-g++}
CFLAGS="-Os -pipe -s -DNDEBUG -D_FILE_OFFSET_BITS=64 -std=c++17 -Wall -Wextra -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -flto"
LDFLAGS="-s -flto -Wl,--gc-sections -ldl"

mkdir -p src src/third_party docs

${CXX} ${CFLAGS} -Isrc -o archive_stub src/archive_main.cpp src/dlz.cpp ${LDFLAGS}
${CXX} ${CFLAGS} -Isrc -o comp         src/comp.cpp         src/dlz.cpp ${LDFLAGS}

chmod +x comp archive_stub || true

echo "[OK] Built comp and archive_stub (dynamic zlib; STORE fallback)."
