#!/usr/bin/env python3
import os
import shutil
import subprocess
import sys

USAGE = """Usage: tools/harness.py [--no-taskset] [--no-time] -- <cmd...>

Runs a command pinned to a single CPU core (taskset -c 0 if available) and optionally wrapped with /usr/bin/time -v.

Options:
  --no-taskset   Do not enforce single core
  --no-time      Do not wrap with /usr/bin/time -v
  -h, --help     Show this help and exit 0

Examples:
  tools/harness.py -- ./comp enwik9 archive
  tools/harness.py --no-time -- ./archive
"""

def show_help():
    print(USAGE, end='')

if len(sys.argv) == 1 or any(a in ('-h', '--help') for a in sys.argv[1:]):
    show_help()
    sys.exit(0)

single_core = True
use_time = True
args = []
iter_argv = iter(sys.argv[1:])
for a in iter_argv:
    if a == '--no-taskset':
        single_core = False
    elif a == '--no-time':
        use_time = False
    elif a == '--':
        # Remainder is the command
        args = list(iter_argv)
        break
    else:
        # Assume start of command without explicit '--'
        args = [a] + list(iter_argv)
        break

if not args:
    show_help()
    sys.exit(0)

prefix = []
if single_core and shutil.which('taskset'):
    prefix += ['taskset', '-c', '0']
if use_time and os.path.exists('/usr/bin/time') and os.access('/usr/bin/time', os.X_OK):
    prefix += ['/usr/bin/time', '-v']

cmd = prefix + args
print('[harness] exec:', ' '.join(cmd), flush=True)
proc = subprocess.run(cmd)
sys.exit(proc.returncode)
