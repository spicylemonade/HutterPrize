Hutter Prize (enwik9) — Solver Scaffold

This repository is a ready-to-use scaffold for building and validating an entry for the Hutter Prize (enwik9). It includes:
- A clear spec (what you must deliver and how it’s verified),
- One-click scripts to fetch enwik9, build, run, verify, and measure sizes (S1, S2, S),
- A tiny harness to enforce single-core execution and collect resource stats,
- A LaTeX paper template for documenting your solution,
- A reporting script that generates a LaTeX snippet with measured results.

Note: This scaffold does not include a competitive compressor. A minimal sample compressor is included at src/comp.c which produces a self-extracting archive that stores the input (no compression). Replace with your real compressor.

TL;DR of the task
- Build two executables on Linux or Windows:
  - comp (size S1): reads enwik9 (exactly 1,000,000,000 bytes) and produces archive (size S2).
  - archive: when run without any input, writes out a file bit-identical to enwik9 (10^9 bytes).
- Total score S := S1 + S2. As of Oct 23, 2025, record L = 110,793,128 bytes. To earn prize money, you must improve by ≥1%: S ≤ 109,685,196 bytes (strictly < 109,685,197).
- Constraints on organizers’ machine: single CPU core, <10 GB RAM, <100 GB disk, ≈ ≤50 hours wall time.
- The decompressor must be self-contained; no network or external files at run time.
- Open-source requirement since 2017 (OSI/“free software” license) and 30-day public comment period.

Current record (to beat)
- 110,793,128 bytes total (fx2-cmix), Kaido Orav & Byron Knoll, Sept 3, 2024.
- Target bytes for prize: ≤ 109,685,196 bytes (≥1% improvement).

Repository layout
- scripts/build.sh: builds comp from src/comp.c or src/comp.cpp. Optionally builds src/archive.* to archive.dev (for local testing only).
- scripts/verify.sh: fetches/validates enwik9, builds comp, runs comp → archive, runs archive in an isolated out dir, verifies identical output, measures sizes/time on one core. Optional timeouts.
- scripts/measure.sh: computes S1, S2, S and checks thresholds.
- scripts/demo.sh: demonstrates the sample self-extracting archive on a tiny file (no enwik9).
- scripts/report.sh: generates docs/results.tex and docs/results.json with sizes, thresholds, and optional SHA-1 hashes.
- scripts/clean.sh: removes demo outputs and temp directories.
- tools/sizer.py: JSON size calculator for S1, S2, S.
- tools/harness.py: single-core/time wrapper for arbitrary commands.
- docs/paper.tex: LaTeX starter for your write-up; docs/build.sh builds it. paper.tex includes results.tex if present.
- docs/SFX_SAMPLE.md: docs for the included minimal SFX sample.
- src/: put your compressor sources here.

Quick start (Linux)
1) Dry-run (no download/build):
   bash scripts/verify.sh -n

2) Get the official enwik9 (or let verify.sh do it):
   wget -O enwik9.zip http://mattmahoney.net/dc/enwik9.zip
   unzip -o enwik9.zip   # produces ./enwik9
   wc -c enwik9          # must print 1000000000

3) Implement your compressor in src/ (examples):
   - src/comp.c or src/comp.cpp → comp
   - comp enwik9 archive         # produces ./archive
   - ./archive                   # must write out a file bit-identical to enwik9 (commonly enwik9 or enwik9.out)

4) Build, run, verify:
   bash scripts/build.sh
   bash scripts/verify.sh      # downloads enwik9 if needed, enforces single core, verifies output

5) Measure sizes:
   bash scripts/measure.sh

Reporting & Paper
- Generate report artifacts (sizes, thresholds, optional SHA-1) for inclusion into the paper:
  bash scripts/report.sh
  # Produces docs/results.tex and docs/results.json

- Build the paper PDF including results.tex if present:
  bash docs/build.sh
  # Output: docs/paper.pdf

Verification protocol (reference)
- Data acquisition:
  wget -O enwik9.zip http://mattmahoney.net/dc/enwik9.zip
  unzip enwik9.zip    # -> ./enwik9
  wc -c enwik9        # must be 1000000000
- Build & run:
  ./comp enwik9 archive
  ./archive           # writes enwik9 or enwik9.out
  cmp enwik9 enwik9.out  # (or cmp enwik9 enwik9)
- Measure sizes:
  stat -c%s comp    # S1
  stat -c%s archive # S2
  S = S1 + S2; check S < L; optionally S ≤ 109,685,196 for ≥1% improvement.
- Performance: single core, <10 GB RAM, <100 GB disk, ≈ ≤50h wall-time.
- Integrity: sha1sum enwik9 enwik9.out (optional; bit identity is required).

Rules (spirit)
- Allowed: any modeling, preprocessing, transforms, learned models, as long as the decompressor is fully self-contained (no external data at run time).
- Disallowed: network I/O or hidden resources during decompression.
- Open-source: release under an OSI license; 30-day public comment period before award.

Implementer checklist
- comp exists, executable: ./comp enwik9 archive
- archive exists, executable; running it writes a file bit-identical to enwik9
- Single-core limits satisfied; memory/disk usage bounded
- Sizes: S1+S2 = S meets thresholds
- Source is free/OSI-licensed; build steps and environment documented

Paper (LaTeX)
- docs/paper.tex is a stub that will include docs/results.tex if generated.
- Build with: bash docs/build.sh (requires a LaTeX distribution).

License
- This scaffold is MIT-licensed. Your solver must also be released under a free/OSI license to qualify for the prize.

References
- Official rules and winners (Wikipedia)
- Dataset definition (Matt Mahoney’s LTCB / enwik9)
- Community threads (Hacker News) and prior winners’ repositories (cmix derivatives, fx2-cmix)
