# Call `make DIFF=meld`, for example, to get a fancy diff
DIFF = diff

all: test

# Ensure we don't have to worry about (serial number) changing by date
stdlib.dg: ../../stdlib.dg
	cp ../../stdlib.dg stdlib.dg
	perl -i -pe 's/\(serial number\)/<SERIAL>/g' stdlib.dg
	perl -i -pe 's/\(compiler version\)/<COMPILER>/g' stdlib.dg
	perl -i -pe 's/\s\(library version\)/\t<LIBRARY>/g' stdlib.dg

impossible.aastory: ../../src/dialogc ImpossibleStairs.dg stdlib.dg
	../../src/dialogc -t aa --no-warn-not-topic ImpossibleStairs.dg stdlib.dg -o impossible.aastory

6502.out: impossible.aastory impossible.in
	../../bin/6502run.py impossible.aastory <impossible.in >6502.out

6502: 6502.out
	$(DIFF) 6502.out 6502.gold

test: 6502

clean:
	rm -f *.aastory *.out stdlib.dg

.PHONY:		all test clean 6502
