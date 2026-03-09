REGTEST=../../bin/regtest.py -v

all: test

%.zblorb: src/dialogc %.dg stdlib.dg
	$+

test: cloak.zblorb
	$(REGTEST) --game cloak.zblorb --interpreter dfrotz cloak.regtest

clean:

.PHONY: all test clean
