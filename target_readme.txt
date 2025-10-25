Hutter Prize (enwik9) — README for a Solver
TL;DR: Build a compressor that turns the exact 1,000,000,000-byte Wikipedia file enwik9 into a self-extracting archive, and beat the current record total size. Your total is the size of your compressor binary (S1) plus the size of your archive (S2); call this S = S1 + S2.
 As of Oct 23, 2025, the record to beat is L = 110,793,128 bytes (“fx2-cmix”, Sept 3, 2024). To claim prize money, you must improve by ≥1%, i.e. S ≤ 109,685,196 bytes (strictly < 109,685,197). (Wikipedia)

What exactly you must do
Input data: Use the canonical enwik9 file — the first 10^9 bytes of the English Wikipedia dump enwiki-20060303-pages-articles.xml. A zipped copy is ~322,592,222 bytes (enwik9.zip). (Matt Mahoney's Home Page)

Build two executables (Windows or Linux):
- comp.exe (or comp on Linux) of size S1: reads enwik9 and produces archive.exe (or archive) of size S2.
- archive.exe when run without any input must write out a file bit-identical to enwik9, i.e. exactly 10^9 bytes. (No downloads, no external files, no internet.)

Total size: S := S1 + S2 must be < L to set a new record (today: L = 110,793,128 bytes), and ≤ 0.99 * L (≤ 109,685,196) to earn prize money. Prize formula: €500,000 × (1 − S/L).

Performance & platform constraints: single core, <10GB RAM, <100GB disk, ≲50h wall clock. Open-source code must be published; a 30-day public comment window applies.

Build & run
- Build: bash build.sh
- Compress: ./comp enwik9 archive
- Decompress: ./archive  (writes ./enwik9.out)
- Verify: cmp enwik9 enwik9.out

CLI options
- --method=zlib|store  (default: zlib if available, else store)
- --no-transform       (disable all transforms; no HPZT header)
- --transforms=LIST    where LIST is 'all' (default), 'none', or a comma-separated subset of {dict,space,nl,digits}. The compressor writes HPZT v2 with the active flag mask; the decoder enforces these flags and verifies the dictionary checksum if dict is enabled.

Transforms (streaming, strictly invertible)
- dict: dictionary tokenization (two-byte tokens)
- space: runs of spaces ≥4
- nl: runs of newlines ≥2
- digits: runs of digits ≥3 followed by the digits themselves
- Escape: literal 0x00 as 0x00,0x00.

Notes
- The transform dictionary is centralized in src/transform_dict.h and guarded by a checksum in HPZT v2.
- verify.sh includes unit-like round-trip tests (STORE/ZLIB) and transform ablations.
- With zlib/deflate, S2 will be around gzip levels and not competitive with the record; a stronger backend is required for the Hutter Prize level.
