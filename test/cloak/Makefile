# Call `make DIFF=meld`, for example, to get a fancy diff
DIFF = diff
DGDEBUG = ../../src/dgdebug -u
DIALOGC = ../../src/dialogc 
AAMBUNDLE = aambundle
DFROTZ = ../../bin/echofrotz.py -m
AAMRUN = ../../bin/aamrun.py
6502RUN = ../../bin/6502run.py
STDLIB = ../../stdlib.dg
UNIT = ../../unit.dg $(STDLIB)

all: test clean

unit: cloak-tests.dg cloak.dg 
	$(DGDEBUG) $^ $(UNIT)

%.z5: %.dg ../../src/dialogc unit
	$(DIALOGC) -t z5 $< no-banner.dg $(STDLIB) -o $@

%.aastory: %.dg ../../src/dialogc unit
	$(DIALOGC) -t aa $< no-banner.dg $(STDLIB) -o $@

debugger: win-debugger lose-debugger

win-debugger.out: cloak.dg win.in ../../src/dgdebug 
	$(DGDEBUG) $< no-banner.dg $(STDLIB) <win.in >$@

lose-debugger.out: cloak.dg lose.in ../../src/dgdebug 
	$(DGDEBUG) $< no-banner.dg $(STDLIB) <lose.in >$@

win-debugger: win-debugger.out 
	$(DIFF) win-debugger.out win-debugger.gold

lose-debugger: lose-debugger.out 
	$(DIFF) lose-debugger.out lose-debugger.gold

zmachine: win-zmachine lose-zmachine

win-zmachine.out: cloak.z5 win.in
	$(DFROTZ) -q $< <win.in >$@

lose-zmachine.out: cloak.z5 lose.in 
	$(DFROTZ) -q $< <lose.in >$@

win-zmachine: win-zmachine.out
	$(DIFF) win-zmachine.out win-zmachine.gold

lose-zmachine: lose-zmachine.out
	$(DIFF) lose-zmachine.out lose-zmachine.gold

web: win-web lose-web

win-web.out: cloak.aastory win.in
	$(AAMRUN) $< <win.in >$@

lose-web.out: cloak.aastory lose.in
	$(AAMRUN) $< <lose.in >$@

win-web: win-web.out
	$(DIFF) win-web.out win-web.gold

lose-web: lose-web.out
	$(DIFF) lose-web.out lose-web.gold

c64: win-c64 lose-c64

win-c64.out: cloak.aastory win.in
	$(6502RUN) $< <win.in >$@

lose-c64.out: cloak.aastory lose.in
	$(6502RUN) $< <lose.in >$@

win-c64: win-c64.out
	$(DIFF) win-c64.out win-c64.gold

lose-c64: lose-c64.out
	$(DIFF) lose-c64.out lose-c64.gold

test: unit debugger zmachine web c64

clean:
	rm -f *.z5 *.aastory *.d64 *.out

.PHONY:	all test clean unit debugger zmachine web win-debugger lose-debugger
.PHONY: win-zmachine lose-zmachine win-web lose-web win-c64 lose-c64 c64
