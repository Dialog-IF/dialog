#!/usr/bin/env python3

# Equivalent to aamrun.py but for 6502 terp

from pathlib import Path
from sys import argv, exit, stderr
import subprocess as sp

USAGE = f'''Usage: {argv[0]} [file.aastory]
Attempts to play an .aastory file in the 6502 interpreter (with consistent random seed for testing), despite varying install locations.'''

if len(argv) != 2 or argv[1] in {'--help', '-h'}:
	print(USAGE, file=stderr)
	exit(1)

BASE = Path(__file__).resolve().parent.parent # Root of the repository
TOSEARCH = [
	BASE / 'aamachine' / 'src' / '6502' / 'aambox6502', # Cloned into the repo for automated builds
	BASE.parent / 'Aamachine' / 'src' / '6502' / 'aambox6502', # On my local machine
]

for path in TOSEARCH:
	if path.exists():
		sp.run([path, '-s', '1234', path.parent/'aambox_frontend.bin', argv[1]])
		exit(0)
print('ERROR: Could not find aambox!', file=stderr)
# And because the problem is likely something with paths
print(f'\tCurrent: {__file__}', file=stderr)
for path in TOSEARCH:
	print(f'\tTried: {path}', file=stderr)
exit(2)
