REGTEST=../../bin/regtest.py -v
DIALOGC=../../src/dialogc 
STDLIB=../../stdlib.dg
AAMRUN=../../bin/aamrun.py
AAMBOX=../../bin/6502run.py

all: test clean

test: test-zmachine test-web test-c64

%.zblorb: %.dg
	$(DIALOGC) -t zblorb $< $(STDLIB)

%.aastory: %.dg
	$(DIALOGC) -t aa $< $(STDLIB)

test-zmachine: cloak.zblorb cloak.regtest
	$(REGTEST) --interpreter dfrotz --game $^

test-web: cloak.aastory cloak.regtest
	$(REGTEST) --interpreter $(AAMRUN) --game $^

test-c64: cloak.aastory cloak.regtest
	$(REGTEST) --interpreter $(AAMBOX) --game $^

clean:
	rm -f *.zblorb *.aastory

.PHONY: all test clean test-zmachine test-web test-c64
