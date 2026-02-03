#!/usr/bin/env python3

# This started as a bash script, but I was having a hard time debugging it. The overhead of spinning up a Python interpreter isn't that high, given what we're currently doing with ./echoing.py for the Z-machine!

from pathlib import Path
from sys import argv, exit, stderr
import subprocess as sp

USAGE = f'''Usage: {argv[0]} [file.aastory]
Attempts to play an .aastory file in the Node interpreter (with consistent random seed for testing), despite varying install locations.'''

if len(argv) != 2 or argv[1] in {'--help', '-h'}:
	print(USAGE, file=stderr)
	exit(1)

BASE = Path(__file__).resolve().parent.parent # Root of the repository
TOSEARCH = [
	BASE / 'aamachine' / 'src' / 'js' / 'nodefrontend.js', # Cloned into the repo for automated builds
	BASE.parent / 'Aamachine' / 'src' / 'js' / 'nodefrontend.js', # On my local machine
]

for path in TOSEARCH:
	if path.exists():
		sp.run(['node', path, '-s', '1234', argv[1]])
		exit(0)
print('ERROR: Could not find nodefrontend.js!', file=stderr)
# And because the problem is likely something with paths
print(f'\tCurrent: {__file__}', file=stderr)
for path in TOSEARCH:
	print(f'\tTried: {path}', file=stderr)
exit(2)
