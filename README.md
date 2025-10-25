Hutter Prize (enwik9) — README for a Solver
TL;DR: Build a compressor that turns the exact 1,000,000,000-byte Wikipedia file enwik9 into a self-extracting archive, and beat the current record total size. Your total is the size of your compressor binary (S1) plus the size of your archive (S2); call this S = S1 + S2.
 As of Oct 23, 2025, the record to beat is L = 110,793,128 bytes (“fx2-cmix”, Sept 3, 2024). To claim prize money, you must improve by ≥1%, i.e. S ≤ 109,685,196 bytes (strictly < 109,685,197).

What exactly you must do
Use the canonical enwik9 file (first 10^9 bytes of enwiki-20060303 pages). Build two executables:
- comp: reads enwik9 and produces a self-extracting archive.
- archive: when executed with no inputs, writes a file bit-identical to enwik9 (10^9 bytes).

Total size S = S1 + S2 must be < L to set a new record, and ≤ 0.99·L to earn prize money. Single core, <10GB RAM, <100GB disk, ≲50h wall clock. Source must be OSI-licensed.

Build & run
- Build: bash build.sh
- Compress: ./comp enwik9 archive
- Decompress: ./archive  (writes ./enwik9.out)
- Verify: cmp enwik9 enwik9.out

CLI options
- --method=zlib|store  (default: zlib if available, else store)
- --no-transform       (disable all transforms; no HPZT header)
- --transforms=LIST    where LIST is 'all' (default), 'none', or a comma-separated subset of {dict,space,nl,digits,hex}.
  The compressor writes HPZT v2 with the active flag mask; the decoder enforces these flags and verifies the dictionary checksum if dict is enabled.

Transforms (streaming, strictly invertible)
- dict: dictionary tokenization (two-byte tokens for frequent XML/Wikitext substrings)
- space: runs of spaces ≥4
- nl: runs of newlines ≥2
- digits: runs of digits ≥3 followed by the digit bytes
- hex: runs of hex digits [0-9A-Fa-f] ≥3 followed by the run bytes
- Escape: literal 0x00 encoded as 0x00,0x00.

Notes
- Shared dictionary lives in src/transform_dict.h and is guarded by a checksum in HPZT v2.
- verify.sh runs round-trip tests (STORE/ZLIB) including transform ablations and chunk-boundary cases.
- With zlib/deflate, S2 is gzip-like and not competitive with the record; a stronger backend is required (future work).
