# Simple make wrapper to avoid executable-bit reliance on scripts
# Usage:
#   make            # default: build
#   make build
#   make verify
#   make measure
#   make clean

SHELL := /bin/bash

.PHONY: all prep build verify measure clean

all: build

prep:
	@chmod +x build.sh verify.sh measure.sh 2>/dev/null || true

build: prep
	@echo "[MAKE] Running build.sh via bash"
	bash ./build.sh

verify: build prep
	@echo "[MAKE] Running verify.sh via bash"
	bash ./verify.sh

measure: prep
	@echo "[MAKE] Running measure.sh via bash"
	bash ./measure.sh

clean:
	@echo "[MAKE] Cleaning build artifacts"
	rm -f comp archive archive_stub enwik9.out
