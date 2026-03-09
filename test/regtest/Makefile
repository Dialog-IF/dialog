REGTEST=../../bin/regtest.py -v
DIALOGC=../../src/dialogc -t zblorb

all: test

cloak.zblorb: cloak.dg
	$(DIALOGC) cloak.dg ../../stdlib.dg

test: cloak.zblorb
	$(REGTEST) --game cloak.zblorb --interpreter dfrotz cloak.regtest

clean:
	rm -f *.zblorb

.PHONY: all test clean
