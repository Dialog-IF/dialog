# Call `make DIFF=meld`, for example, to get a fancy diff
DIFF = diff
DGDEBUG = ../../src/dgdebug -u --no-header
DIALOGC = ../../src/dialogc 
DFROTZ = ../../bin/echofrotz.py -m
AAMRUN = ../../bin/aamrun.py
STDLIB = no-banner.dg ../../stdlib.dg
UNIT = ../../unit.dg $(STDLIB)

all: test clean

unit: ../../src/dgdebug cloak-tests.dg cloak.dg ../../unit.dg
	$(DGDEBUG) cloak-tests.dg cloak.dg $(UNIT)

%.z8: %.dg ../../src/dialogc unit
	$(DIALOGC) -t z8 $< $(STDLIB) -o $@

%.aastory: %.dg ../../src/dialogc unit
	$(DIALOGC) -t aa $< $(STDLIB) -o $@

debugger: win-debugger lose-debugger

win-debugger.out: ../../src/dgdebug cloak.dg win.in 
	$(DGDEBUG) cloak.dg $(STDLIB) <win.in >win-debugger.out

lose-debugger.out: ../../src/dgdebug cloak.dg lose.in 
	$(DGDEBUG) cloak.dg $(STDLIB) <lose.in >lose-debugger.out

win-debugger: win-debugger.out 
	$(DIFF) win-debugger.out win-debugger.gold

lose-debugger: lose-debugger.out 
	$(DIFF) lose-debugger.out lose-debugger.gold

zmachine: win-zmachine lose-zmachine

win-zmachine.out: cloak.z8 win.in
	$(DFROTZ) -q cloak.z8 <win.in >win-zmachine.out

lose-zmachine.out: cloak.z8 lose.in 
	$(DFROTZ) -q cloak.z8 <lose.in >lose-zmachine.out

win-zmachine: win-zmachine.out
	$(DIFF) win-zmachine.out win-zmachine.gold

lose-zmachine: lose-zmachine.out
	$(DIFF) lose-zmachine.out lose-zmachine.gold

aamachine: win-aamachine lose-aamachine

win-aamachine.out: cloak.aastory win.in
	$(AAMRUN) cloak.aastory <win.in >win-aamachine.out

lose-aamachine.out: cloak.aastory lose.in
	$(AAMRUN) cloak.aastory <lose.in >lose-aamachine.out

win-aamachine: win-aamachine.out
	$(DIFF) win-aamachine.out win-aamachine.gold

lose-aamachine: lose-aamachine.out
	$(DIFF) lose-aamachine.out lose-aamachine.gold

test: unit debugger zmachine aamachine

clean:
	rm -f *.z8 *.aastory *.out

.PHONY:	all test clean unit debugger zmachine aamachine win-debugger 
.PHONY: lose-debugger win-zmachine lose-zmachine win-aamachine lose-aamachine
