#!/usr/bin/env python3

# Unlike dgdebug, dfrotz doesn't echo each line of input to stdout when reading from a file
# So in order to get a proper transcript, we need to work around that
# Previously this was done using an awful hack involving time.sleep that created race conditions, so builds would sporadically fail for no reason
# Now we're switching to `dfrotz -r lt`, which outputs two characters before each line, indicating the interpreter's current state
# This lets us know exactly when it's waiting for input so we don't need to worry about timing any more

from sys import argv, stdin, exit
import subprocess as sp
import threading as thr
from collections import deque
import io
from time import sleep

if len(argv) == 1 or argv[1] in {'--help', '-h'}:
	print(f'Usage: {argv[0]} [options] [file]\nRuns dfrotz [options] [file], reading from stdin and writing to stdout, but also echoes each line from stdin before sending it. Uses the `-r lt` line-tagging option to detect when the interpreter is waiting for input.')
	exit(1)

def output_manager():
	global finished
	while True:
		char = wrap.read(1)
		if not char: break # EOF
		with lock:
			buffer.append(char)

proc = sp.Popen(['dfrotz', '-q', '-r', 'lt'] + argv[1:], stdin=sp.PIPE, stdout=sp.PIPE, bufsize=0)
wrap = io.TextIOWrapper(proc.stdout, encoding='utf8') # Convert bytes to str
lock = thr.Lock()
buffer = deque()
finished = False
thread = thr.Thread(target=output_manager)
thread.start()

def read_async():
	with lock:
		if not buffer: return None # No data available
		return buffer.popleft()

def send_to_subprocess(inline):
	# What we want to do for each line of stdin:
	# Print the line we got from stdin
	# Send it to the process's stdin
	# Read from our subprocess's stdout until it emits a line starting with a "waiting for input" tag
	# Give it a moment to make sure it finishes that line
	# Then return
	
	if inline is not None:
		if (st := proc.poll()) is not None: # Subprocess died
			exit(st)
		
		print(inline, end='', flush=True)
		proc.stdin.write(inline.encode('utf8'))
		proc.stdin.flush()
	
	outchars = [] # The characters we've gathered from the process's stdout
	while True: # Let the subprocess run until it's ready for input
		char = read_async()
		if char is None: # No data available yet
			if (st := proc.poll()) is not None: # Subprocess died
				exit(st)
			
			sleep(0.001) # Give the process a moment to run
			continue
		
		if char == '\n':
			outline = ''.join(outchars)
			tag, data = outline[:2], outline[2:]
			if tag[0] in {' ', '.'}: # Output
				print(data)
			outchars = []
		else:
			outchars.append(char)
		if len(outchars) >= 2 and outchars[0] in {'>', ')', 'T', 't'}: # The terp is going to block at the end of this line
			sleep(0.001) # Give the game a moment to finish printing the line
			while (c := read_async()) is not None:
				outchars.append(c)
			# Then we're done!
			outline = ''.join(outchars)
			tag, data = outline[:2], outline[2:]
			print(data, end='', flush=True)
			outchars = []
			break

send_to_subprocess(None)
for inline in stdin:
	send_to_subprocess(inline)
proc.terminate() # dfrotz doesn't exit on EOF
