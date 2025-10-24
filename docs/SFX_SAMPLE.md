Sample self-extracting archive (no compression)

This repository includes a minimal example compressor at src/comp.c. It demonstrates the exact interface required by the Hutter Prize:
- ./comp <input> <archive> produces a self-extracting ./archive.
- ./archive (run with no inputs) writes the original data to a file (default enwik9.out, overridable via HP_OUT).

Important: This example does not compress; it simply stores the input bytes inside the archive. It exists solely to illustrate the contract and provide a working baseline. Replace it with a real compressor to compete.

Quick demo (tiny sample file; does not download enwik9):
  bash scripts/demo.sh

What happens:
- scripts/build.sh compiles src/comp.c into ./comp.
- ./comp sample.bin archive creates a POSIX sh self-extracting archive by appending sample.bin after a small shell header.
- Running ./archive extracts the appended payload into out.bin and verifies the size.
- scripts/measure.sh reports S1 (comp), S2 (archive), and S.

Notes:
- The self-extractor relies on standard Unix tools (sh, awk, tail, wc, dd). For a production submission, implement a standalone binary decompressor.
- The verification harness (scripts/verify.sh) expects ./archive to regenerate enwik9 bit-identically when run with no inputs. The sample SFX defaults to writing enwik9.out.
