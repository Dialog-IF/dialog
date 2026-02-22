#!/usr/bin/env python3

# Take a list of abbreviations produced by ZAbbrev (in Inform format) and turn it into a Dialog source file

import re
from collections import defaultdict

INFILE = 'abbrevs.inf'
OUTFILE = 'abbrevs.inc' # Giving it a weird suffix because it's C code interpolated into another C file

MAX_ABBREVS = 96

PATTERN = r'Abbreviate "([^"]+)";\s+\!.*'

REPLACEMENTS = { '~':'\\"', '^':'\\n' } # Undo Inform string escaping (or rather replace it with C string escaping

abbrevs = []

# Step 1: get all the abbreviations
with open(INFILE, 'r') as f:
	for line in f:
		if m := re.match(PATTERN, line):
			string = m[1]
			for k,v in REPLACEMENTS.items(): # Undo escaping
				string = string.replace(k, v)
			print(f'Found abbreviation: {string}')
			abbrevs.append(string)
num_found = len(abbrevs)
print(f'Total: {num_found}/{MAX_ABBREVS}')

# Step 2: build the index
index = defaultdict(list) # first char : [longest ... shortest]
for abr in abbrevs:
	c = abr[0]
	index[c].append(abr)

# Step 3: sort each entry in the index
for v in index.values():
	v.sort(reverse=True, key=len)

def fixed_len(s): # Number of characters once backslash escapes are applied
	return len(s) - sum(1 for c in s if c == '\\')

# Step 4: convert to C code
counter = 0
count_index = {}
lengths = []
first = True
with open(OUTFILE, 'w') as f:
	f.write('// Generated automatically by bin/abbrevmaker.py\n')
	f.write('// Intended to be included directly into backend_z.c\n')
	f.write('\n')
	
	f.write(f'#define N_ABBREVS {num_found}')
	f.write('\n')
	
	f.write('const char* abbreviations[N_ABBREVS] = {\n')
	
	for k,v in index.items():
		if first: first = False
		else: f.write('\n')
		name = k if k != ' ' else "' '" # Spaces are weird
		f.write(f'\t// {name} ({ord(k)})\n')
		count_index[ord(k)] = counter
		
		for abr in v:
			counter += 1
			f.write(f'\t"{abr}",\n')
			lengths.append(fixed_len(abr))
			print(f'Writing: {abr}')
	f.write('};\n')
	f.write('\n')
	
	count_index_list = [count_index[i] if i in count_index else num_found for i in range(0, 255)] # Codepoint -> position in array
	f.write('// Maps codepoints to starting indices in the abbreviations array\n')
	f.write('const uint8_t abbrev_index[255] = {')
	f.write(', '.join(str(i) for i in count_index_list))
	f.write('};\n')
	f.write('\n')
	
	f.write('// Length of each abbreviation (precomputed)\n')
	f.write('const uint8_t abbrev_lengths[N_ABBREVS] = {')
	f.write(', '.join(str(i) for i in lengths))
	f.write('};\n')

print('Done!')
