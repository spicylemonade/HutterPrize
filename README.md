Hutter Prize (enwik9) — README for a Solver
TL;DR: Build a compressor that turns the exact 1,000,000,000-byte Wikipedia file enwik9 into a self-extracting archive, and beat the current record total size. Your total is the size of your compressor binary (S1) plus the size of your archive (S2); call this S = S1 + S2.
 As of Oct 23, 2025, the record to beat is L = 110,793,128 bytes (“fx2-cmix”, Sept 3, 2024). To claim prize money, you must improve by ≥1%, i.e. S ≤ 109,685,196 bytes (strictly < 109,685,197). (Wikipedia)

What exactly you must do
Input data: Use the canonical enwik9 file — the first 10^9 bytes of the English Wikipedia dump enwiki-20060303-pages-articles.xml. A zipped copy is ~322,592,222 bytes (enwik9.zip). (Matt Mahoney's Home Page)


Build two executables (Windows or Linux):


comp.exe (or comp on Linux) of size S1: reads enwik9 and produces archive.exe (or archive) of size S2.


archive.exe when run without any input must write out a file bit-identical to enwik9, i.e. exactly 10^9 bytes. (No downloads, no external files, no internet.) (I Programmer)


Total size: S := S1 + S2 must be


< L to set a new record (today: L = 110,793,128 bytes), and


≤ 0.99 * L (i.e. ≤ 109,685,196 bytes) to earn at least the €5,000 minimum award (≥1% improvement). Prize formula: €500,000 × (1 − S/L). (Wikipedia)


Performance & platform constraints (tested on the organizers’ machine):


Wall-clock time: ≈ ≤ 50 hours,


CPU: single core only (no GPUs),


RAM: < 10 GB,


Disk usage (including temps): < 100 GB.
 These limits are stated on the prize site and reiterated by organizers in public threads. (Hacker News)


Publication & review: You must publish the code (OSI/“free software” license requirement since 2017). There is a 30-day public comment period before award. (Wikipedia)



Current record (to beat)
110,793,128 bytes total (program fx2-cmix), Kaido Orav & Byron Knoll, Sept 3, 2024.
 Prior enwik9 winners: Orav (Feb 2, 2024) 112,578,322; Saurabh Kumar (Jul 16, 2023) 114,156,155; Margaritov (May 31, 2021) 115,352,938; Rhatushnyak (2019) 116,673,681 (pre-prize for enwik9). (Wikipedia)


Target bytes for prize: With L = 110,793,128, 1% improvement ⇒ ≤ 109,685,196 bytes (i.e., strictly < 109,685,197).

Allowed vs. disallowed (spirit of the rules)
Allowed: Any modeling, preprocessing, transforms, or learned models as long as everything needed at decompression time is inside your delivered binaries (S1+S2). Self-extracting archive is standard. (I Programmer)


Disallowed: Any “outside information” at decompression time (network, hidden resources, OS-bundled data, installed corpora). The decompressor must fully regenerate enwik9 by itself. (I Programmer)


Open-source requirement: Since 2017, entries must release source code under a free software license. (Wikipedia)



Verification protocol (what your entry must pass)
Data acquisition
# Get the official data
wget -O enwik9.zip http://mattmahoney.net/dc/enwik9.zip
unzip enwik9.zip    # produces ./enwik9
wc -c enwik9        # must print: 1000000000

(Definition of enwik9 and its origin are documented by Matt Mahoney / LTCB.) (Matt Mahoney's Home Page)
Build & run (example interface)
# Build your binaries (names are guidelines)
# ./comp enwik9 -> produces ./archive
./comp enwik9 archive

# Decompress (no inputs allowed)
./archive    # writes ./enwik9.out
cmp enwik9 enwik9.out   # must output nothing (files identical)

Measure size & limits
# Byte-accurate sizes
stat -c%s comp     # -> S1
stat -c%s archive  # -> S2
python3 - << 'PY'
S1 = int(open('/proc/self/fd/0').read())  # placeholder if you prefer manual
PY
# Or just add them: S = S1+S2; confirm S < L and (optionally) S ≤ floor(0.99*L).

# Time / memory on single core (organizers will enforce)
taskset -c 0 /usr/bin/time -v ./comp enwik9 archive
taskset -c 0 /usr/bin/time -v ./archive

Integrity check (optional but common)
sha1sum enwik9 enwik9.out   # digests must match

(SHA-1 is commonly used here to verify identical output files; cryptographic strength isn’t the point—bit-identical is.) (Wikipedia)
Submission & review
Provide binaries and source code (free/OSI license), document exact build steps, OS, and runtime resource footprint.


Organizers announce a 30-day comment window before awarding. (Wikipedia)



Practical spec (hand this to your implementer)
Deliverables
comp (Linux) or comp.exe (Windows), size S1.


archive (Linux) or archive.exe (Windows), size S2.


S = S1+S2 must be < 110,793,128 to set a new record; ≤ 109,685,196 to earn prize money. (Wikipedia)


One-click build script and README with: OS/version, compiler & flags, exact commands to produce archive from enwik9, and any expected temp files.


Runtime contract
comp enwik9 archive must finish within ~50h on one CPU core, using <10GB RAM and <100GB disk total.


archive must not read the network, environment files, or any extras; it just writes enwik9 (10^9 bytes) to the working directory. (Hacker News)


Validation checklist
wc -c enwik9 → 1000000000. (Matt Mahoney's Home Page)


./comp enwik9 archive exits OK under limits.


./archive writes enwik9.out with wc -c = 1000000000.


cmp enwik9 enwik9.out shows equality (or identical sha1sum).


stat -c%s comp + stat -c%s archive = S meets the threshold.


Source published under a free/OSI license. 30-day comment window observed. (Wikipedia)



Prior art (to scope the problem)
The last winners (2021–2024) are variants combining heavy text transforms (dictionary/wikitext preprocessing, article reordering) with context-mixing compressors (e.g., cmix derivatives). See fx2-cmix notes and discussions. This gives a sense of where the bar is today. (GitHub)



Authoritative references
Official overview / winners list (record numbers and dates). (Wikipedia)


Formal rules & FAQ (site text; includes decompressor self-containment, prize formula, review period). (I Programmer)


Dataset definition & LTCB notes (what enwik9 precisely is). (Matt Mahoney's Home Page)



Notes & gotchas
Some blog posts mirror outdated limits (e.g., 100h); follow the current prize page language—≈≤50h, single core, <10GB RAM, <100GB disk—which recent winners satisfy. (Hacker News)


Earlier pages or third-party READMEs sometimes contain a typo “109 byte file” — it’s 10^9 bytes (1,000,000,000). (I Programmer)



If you want, I can turn this into a repo-ready README.md with sample scripts (verify.sh, measure.sh) and a tiny harness to compute S, enforce taskset, and print a pass/fail summary.
DO NOT just CREATE A SCAFFOLD, SOLVE THE PROBLEM. DONT STOP TILL YOU ACHIEVE RECORD
