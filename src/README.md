# src/

Place your compressor sources here. The build script looks for:
- src/comp.c or src/comp.cpp -> builds ./comp
- Optionally src/archive.c or src/archive.cpp -> builds ./archive.dev (for local testing only; the prize run expects ./archive to be produced by ./comp)

Included sample:
- src/comp.c is a minimal self-extracting archive builder for Linux that stores raw input (no compression). It generates a POSIX sh SFX which, when run with no inputs, writes the original data to enwik9.out (or $HP_OUT if set). This is for demonstration only; replace it with your real compressor.

Minimal contract your comp must honor:
- ./comp enwik9 archive   # reads 1,000,000,000-byte enwik9 and writes a self-extracting ./archive
- ./archive               # when run without inputs, writes out a file bit-identical to enwik9

Performance budget (organizers enforce): single core, <10 GB RAM, <100 GB disk, ≈ ≤50 hours.
