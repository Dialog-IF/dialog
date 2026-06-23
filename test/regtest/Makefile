REGTEST=../../bin/regtest.py -v
DIALOGC=../../src/dialogc 
STDLIB=../../stdlib.dg
AAMRUN=../../bin/aamrun.py
AAMBOX=../../bin/6502run.py

all: test clean

test: test-zmachine test-web test-c64

%.z5: %.dg
	$(DIALOGC) -t z5 $< $(STDLIB) -o $@

%.aastory: %.dg
	$(DIALOGC) -t aa $< $(STDLIB) -o $@

test-zmachine: cloak.z5 cloak.regtest
	$(REGTEST) --interpreter dfrotz --game $^

test-web: cloak.aastory cloak.regtest
	$(REGTEST) --interpreter $(AAMRUN) --game $^

test-c64: cloak.aastory cloak.regtest
	$(REGTEST) --interpreter $(AAMBOX) --game $^

clean:
	rm -f *.z5 *.aastory

.PHONY: all test clean
