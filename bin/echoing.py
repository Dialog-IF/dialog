#!/usr/bin/env python3

from sys import argv, stdin
import subprocess as sp
from time import sleep

if len(argv) == 1 or argv[1] in {'--help', '-h'}:
	print(f'Usage: {argv[0]} [COMMAND]\nExecutes COMMAND, reading from stdin and writing to stdout, but also echoes each line from stdin before sending it.')
	exit(1)

proc = sp.Popen(argv[1:], stdin=sp.PIPE, text=True, bufsize=1)
for line in stdin:
	sleep(0.01) # Tiny delay to sync, assuming the game won't take longer than this to run each turn on modern hardware
	print(line, end='', flush=True)
	proc.stdin.write(line)
	proc.stdin.flush()
