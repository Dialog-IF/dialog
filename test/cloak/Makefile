# Call `make DIFF=meld`, for example, to get a fancy diff
DIFF = diff

all: test clean

# Ensure we don't have to worry about (serial number) changing by date
stdlib.dg: ../../stdlib.dg
	cp ../../stdlib.dg stdlib.dg
	perl -i -pe 's/\(serial number\)/<SERIAL>/g' stdlib.dg
	perl -i -pe 's/\(compiler version\)/<COMPILER>/g' stdlib.dg
	perl -i -pe 's/\s\(library version\)/\t<LIBRARY>/g' stdlib.dg

cloak.z8: ../../src/dialogc cloak.dg stdlib.dg
	../../src/dgdebug -u cloak-tests.dg cloak.dg ../../unit.dg stdlib.dg
	../../src/dialogc -t z8 --no-warn-not-topic cloak.dg stdlib.dg -o cloak.z8

zmachine.out: cloak.z8 cloak.in
	../../bin/echofrotz.py -m -s 1234 -q cloak.z8 <cloak.in >zmachine.out

zmachine: zmachine.out
	$(DIFF) zmachine.out zmachine.gold

debugger.out: ../../src/dgdebug cloak.dg stdlib.dg cloak.in
	../../src/dgdebug -u cloak-tests.dg cloak.dg ../../unit.dg stdlib.dg
	../../src/dgdebug -s 1234 --no-warn-not-topic cloak.dg stdlib.dg <cloak.in >debugger.out

debugger: debugger.out
	$(DIFF) debugger.out debugger.gold

cloak.aastory: ../../src/dialogc cloak.dg stdlib.dg
	../../src/dgdebug -u cloak-tests.dg cloak.dg ../../unit.dg stdlib.dg
	../../src/dialogc -t aa --no-warn-not-topic cloak.dg stdlib.dg -o cloak.aastory

aamachine.out: cloak.aastory cloak.in
	../../bin/aamrun.py cloak.aastory <cloak.in >aamachine.out

aamachine: aamachine.out
	$(DIFF) aamachine.out aamachine.gold

test: debugger zmachine aamachine

clean:
	rm -f *.z8 *.aastory *.out stdlib.dg

.PHONY:	all test clean bless debugger zmachine aamachine
