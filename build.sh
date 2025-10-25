#!/usr/bin/env bash
set -euo pipefail

CXX=${CXX:-g++}
CFLAGS="-Os -pipe -s -DNDEBUG -D_FILE_OFFSET_BITS=64 -std=c++17 -Wall -Wextra -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -flto"
LDFLAGS="-s -flto -Wl,--gc-sections -ldl"

mkdir -p src src/third_party docs

${CXX} ${CFLAGS} -Isrc -o archive_stub     src/archive_main.cpp src/dlz.cpp ${LDFLAGS}
${CXX} ${CFLAGS} -Isrc -o comp             src/comp.cpp         src/dlz.cpp ${LDFLAGS}
# Self-test binaries for transform layer
${CXX} ${CFLAGS} -Isrc -o hpzt_selftest    src/hpzt_selftest.cpp ${LDFLAGS}
${CXX} ${CFLAGS} -Isrc -o hpzt_stream_fuzz src/hpzt_stream_fuzz.cpp ${LDFLAGS}
# Archive introspection tool
${CXX} ${CFLAGS} -Isrc -o hpzt_dump        src/hpzt_dump.cpp     src/dlz.cpp ${LDFLAGS}

echo "[OK] Built comp, archive_stub, hpzt_selftest, hpzt_stream_fuzz, and hpzt_dump (dynamic zlib; STORE fallback)."
