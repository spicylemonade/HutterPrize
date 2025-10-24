#!/usr/bin/env python3
import os
import sys
import json

USAGE = """Usage: tools/sizer.py [--L=N] [--L1=N] [--json] <comp> <archive>\n\nCompute S1, S2, and S = S1 + S2 (bytes). L is the current record to beat; L1 is the ≥1% improvement threshold.\n\nOptions:\n  --L=N       Record to beat (bytes). Default: 110793128\n  --L1=N      ≥1% improvement threshold (bytes). Default: 109685196\n  --json      Output JSON instead of text\n  -h, --help  Show this help and exit 0\n\nExamples:\n  python3 tools/sizer.py comp archive\n  python3 tools/sizer.py --json --L=110793128 --L1=109685196 comp archive\n"""

def show_help() -> None:
    print(USAGE, end="")

# Pre-handle help and empty invocation to avoid argparse SystemExit traces
if len(sys.argv) == 1 or any(a in ("-h", "--help") for a in sys.argv[1:]):
    show_help()
    sys.exit(0)

# Minimal manual parsing to avoid argparse side-effects
L = 110_793_128
L1 = 109_685_196
json_out = False
positional = []
for a in sys.argv[1:]:
    if a.startswith("--L="):
        val = a.split("=", 1)[1]
        try:
            L = int(val)
        except ValueError:
            print("error: --L must be an integer", file=sys.stderr)
            sys.exit(2)
    elif a.startswith("--L1="):
        val = a.split("=", 1)[1]
        try:
            L1 = int(val)
        except ValueError:
            print("error: --L1 must be an integer", file=sys.stderr)
            sys.exit(2)
    elif a == "--json":
        json_out = True
        continue
    else:
        positional.append(a)

if len(positional) != 2:
    # Friendly help on misuse or wrong arity
    show_help()
    sys.exit(0)

comp, archive = positional

def size_or_err(p: str) -> int:
    try:
        return os.path.getsize(p)
    except OSError as e:
        print(f"error: cannot stat {p}: {e}", file=sys.stderr)
        return -1

s1 = size_or_err(comp)
s2 = size_or_err(archive)
if s1 < 0 or s2 < 0:
    sys.exit(1)
S = s1 + s2

if json_out:
    print(json.dumps({
        "comp": comp,
        "archive": archive,
        "S1": s1,
        "S2": s2,
        "S": S,
        "L": L,
        "L_1pct": L1,
        "pass_record": S < L,
        "pass_prize_1pct": S <= L1,
    }, indent=2))
else:
    print(f"S1 (comp):    {s1} bytes")
    print(f"S2 (archive): {s2} bytes")
    print(f"S total:      {S} bytes")
    print(f"Record to beat (L):      {L}")
    print(f"≥1% improvement (L_1%): {L1}")
    print("Status vs record: " + ("PASS" if S < L else "FAIL"))
    print("Status vs ≥1%:   " + ("PASS" if S <= L1 else "FAIL"))
