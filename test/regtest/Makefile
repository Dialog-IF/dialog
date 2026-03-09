REGTEST=../../bin/regtest.py -v
DIALOGC=../../src/dialogc -t zblorb

all: test

cloak.zblorb: cloak.dg
	$(DIALOGC) cloak.dg ../../stdlib.dg

test: cloak.zblorb
	$(REGTEST) --game cloak.zblorb --interpreter dfrotz cloak.regtest

clean:
	rm *.zblorb *.z8

.PHONY: all test clean
