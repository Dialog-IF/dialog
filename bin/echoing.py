#!/usr/bin/env python3

# This script is an awful hack around a limitation of dumbfrotz
# Unlike dgdebug, it doesn't echo each line of input to stdout when reading from a file
# So we need to manually insert those lines back in for the automated tests to work
# This has horribly glaring race conditions which occasionally cause the tests to fail
# So hopefully dumbfrotz can get fixed at some point so we no longer need this

from sys import argv, stdin
import subprocess as sp
from time import sleep

if len(argv) == 1 or argv[1] in {'--help', '-h'}:
	print(f'Usage: {argv[0]} [COMMAND]\nExecutes COMMAND, reading from stdin and writing to stdout, but also echoes each line from stdin before sending it.')
	exit(1)

proc = sp.Popen(argv[1:], stdin=sp.PIPE, text=True, bufsize=1)
for line in stdin:
	sleep(0.01) # Tiny delay to sync, assuming the game won't take longer than this to run each turn on modern hardware
	# Ideally we could detect when dumbfrotz wants input instead - possibly with the line tagging option? TODO
	print(line, end='', flush=True)
	proc.stdin.write(line)
	proc.stdin.flush()
