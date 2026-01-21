# Call `make DIFF=meld`, for example, to get a fancy diff
DIFF = diff

all: zmachine debugger

# Ensure we don't have to worry about (serial number) changing by date
stdlib.dg: ../../stdlib.dg
	cp ../../stdlib.dg stdlib.dg
	perl -i -pe 's/\(serial number\)/<SERIAL>/g' stdlib.dg
	perl -i -pe 's/\(compiler version\)/<COMPILER>/g' stdlib.dg
	perl -i -pe 's/\s\(library version\)/\t<LIBRARY>/g' stdlib.dg

impossible.z8: ../../src/dialogc ImpossibleStairs.dg stdlib.dg
	../../src/dialogc -t z8 --no-warn-not-topic ImpossibleStairs.dg stdlib.dg -o impossible.z8

zmachine.out: impossible.z8 impossible.in
	../../bin/echoing.py dfrotz -m -s 1234 -q impossible.z8 <impossible.in >zmachine.out

zmachine: zmachine.out
	$(DIFF) zmachine.out zmachine.gold

debugger.out: ../../src/dgdebug ImpossibleStairs.dg stdlib.dg impossible.in
	../../src/dgdebug -s 1234 --no-warn-not-topic ImpossibleStairs.dg stdlib.dg <impossible.in >debugger.out

debugger: debugger.out
	$(DIFF) debugger.out debugger.gold

test: debugger zmachine

clean:
	rm -f *.z8 *.out stdlib.dg

.PHONY:		test clean debugger zmachine
