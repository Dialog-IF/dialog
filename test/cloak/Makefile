# Call `make DIFF=meld`, for example, to get a fancy diff
DIFF = diff
DGDEBUG = ../../src/dgdebug -u --no-header
DIALOGC = ../../src/dialogc --no-header
DFROTZ = ../../bin/echofrotz.py -m
AAMRUN = ../../bin/aamrun.py
STDLIB = ../../stdlib.dg
UNIT = ../../unit.dg $(STDLIB)

all: test clean

unit: ../../src/dgdebug cloak-tests.dg cloak.dg ../../stdlib.dg ../../unit.dg
	$(DGDEBUG) cloak-tests.dg cloak.dg $(UNIT)

cloak.z8: ../../src/dialogc cloak.dg unit
	$(DIALOGC) -t z8 cloak.dg $(STDLIB) -o cloak.z8

zmachine.out: cloak.z8 cloak.in
	$(DFROTZ) -s 1234 -q cloak.z8 <cloak.in >zmachine.out

zmachine: zmachine.out
	$(DIFF) zmachine.out zmachine.gold

debugger.out: ../../src/dgdebug cloak.dg  cloak.in unit
	$(DGDEBUG) -s 1234 cloak.dg $(STDLIB) <cloak.in >debugger.out

debugger: debugger.out
	$(DIFF) debugger.out debugger.gold

cloak.aastory: ../../src/dialogc cloak.dg stdlib.dg unit
	$(DIALOGC) -t aa cloak.dg $(STDLIB) -o cloak.aastory

aamachine.out: cloak.aastory cloak.in
	$(AAMRUN) cloak.aastory <cloak.in >aamachine.out

aamachine: aamachine.out
	$(DIFF) aamachine.out aamachine.gold

test: unit debugger zmachine aamachine

clean:
	rm -f *.z8 *.aastory *.out

.PHONY:	all test clean unit bless debugger zmachine aamachine
