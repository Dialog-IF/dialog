REGTEST=../../bin/regtest.py -v

all: test

test: cloak.zblorb
	$(REGTEST) --game cloak.zblorb --interpreter dfrotz cloak.regtest

clean:

.PHONY: all test clean
