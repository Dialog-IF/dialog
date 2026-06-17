# Call `make DIFF=meld`, for example, to get a fancy diff
DIFF = diff
DGDEBUG = ../../src/dgdebug -u
DIALOGC = ../../src/dialogc 
DFROTZ = ../../bin/echofrotz.py -m
AAMRUN = ../../bin/aamrun.py
STDLIB = ../../stdlib.dg
UNIT = ../../unit.dg $(STDLIB)

all: test clean

unit: cloak-tests.dg cloak.dg 
	$(DGDEBUG) $^ $(UNIT)

%.z8: %.dg ../../src/dialogc unit
	$(DIALOGC) -t z8 $< no-banner.dg $(STDLIB) -o $@

%.aastory: %.dg ../../src/dialogc unit
	$(DIALOGC) -t aa $< no-banner.dg $(STDLIB) -o $@

debugger: win-debugger lose-debugger

win-debugger.out: cloak.dg win.in ../../src/dgdebug 
	$(DGDEBUG) $< no-banner.dg $(STDLIB) <win.in >win-debugger.out

lose-debugger.out: cloak.dg lose.in ../../src/dgdebug 
	$(DGDEBUG) $< no-banner.dg $(STDLIB) <lose.in >lose-debugger.out

win-debugger: win-debugger.out 
	$(DIFF) win-debugger.out win-debugger.gold

lose-debugger: lose-debugger.out 
	$(DIFF) lose-debugger.out lose-debugger.gold

zmachine: win-zmachine lose-zmachine

win-zmachine.out: cloak.z8 win.in
	$(DFROTZ) -q $< <win.in >win-zmachine.out

lose-zmachine.out: cloak.z8 lose.in 
	$(DFROTZ) -q $< <lose.in >lose-zmachine.out

win-zmachine: win-zmachine.out
	$(DIFF) win-zmachine.out win-zmachine.gold

lose-zmachine: lose-zmachine.out
	$(DIFF) lose-zmachine.out lose-zmachine.gold

web: win-web lose-web

win-web.out: cloak.aastory win.in
	$(AAMRUN) $< <win.in >win-web.out

lose-web.out: cloak.aastory lose.in
	$(AAMRUN) $< <lose.in >lose-web.out

win-web: win-web.out
	$(DIFF) win-web.out win-web.gold

lose-web: lose-web.out
	$(DIFF) lose-web.out lose-web.gold

test: unit debugger zmachine web

clean:
	rm -f *.z8 *.aastory *.out

.PHONY:	all test clean unit debugger zmachine web win-debugger lose-debugger
.PHONY: win-zmachine lose-zmachine win-web lose-web win-c64 lose-c64
