#!/usr/bin/env python3

from sys import argv
import re
from pathlib import Path

if len(argv) < 4 or len(argv) % 2: # Need at least four params, an even number of them
	print('''
Usage: striate.py INPUT CHAR1 OUT1 [CHAR2 OUT2...]

Divides INPUT file into several OUTn files.
Any line in INPUT that doesn't begin with an opening bracket, some uppercase letters, then a closing bracket will be copied to each output file.
Otherwise, the line (minus the bracketed part) will be copied to only the output files whose characters are included in the brackets.
A single whitespace character after the bracketed portion will be ignored.
If you need a line starting with [A] or such in the output file itself, [*] indicates that the line should be copied to all output files, regardless of command-line options. This should only be needed in edge cases.

For example: striate.py INPUT A OUT1 B OUT2

This line will be copied to all files
[A] This line will only be copied to file 1
[B] This line will only be copied to file 2
[AB] This line will be copied to both file 1 and file 2
[*] This line will also be copied to all files
[#] This line is a comment that will be copied to no files
[C] This line will also be copied to no files
''')
	exit(1)

infile = argv[1]
results = {}
regex = re.compile(r''' ^				# Start of line
						\[ ([A-Z]+) \]	# Group: one or more bracketed uppercase letters
						\s?				# Optional single whitespace
						(.*)			# Rest of the line
						$				# End of line
					''', re.X)
comment = re.compile(r' ^ \[ \# \]', re.X) # [#] at the start of a line
starline = re.compile(r' ^ \[ \* \] \s? (.*) $', re.X) # [*] at the start of a line

for char, fn in zip(argv[2::2], argv[3::2]):
	if char in results:
		raise KeyError(f'Char "{c}" appeared multiple times')
	if not re.fullmatch(r'[A-Z]', char):
		raise KeyError(f'"{c}" is not a single uppercase ASCII letter')
	results[char] = Path(fn).open('w') # Will raise exceptions if anything goes wrong

with open(infile, 'r') as inf:
	for line in inf:
		line = line[:-1] # Remove final newline
		if comment.match(line): # [#] writes to no files
			continue
		elif m := starline.fullmatch(line): # [*] writes to all files, for edge cases
			rest = m.group(1)
			for v in results.values():
				v.write(line + '\n')
		elif m := regex.fullmatch(line):
			chars, rest = m.group(1, 2)
	#		print(f'C: {chars} R: {rest}')
			for k, v in results.items():
				if k in chars:
					v.write(rest + '\n')
		else:
			for v in results.values():
				v.write(line + '\n')

for v in results.values():
	v.close()
