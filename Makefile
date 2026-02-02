all: src/dialogc src/dgdebug test

src/dialogc:
	$(MAKE) -C src dialogc

src/dgdebug:
	$(MAKE) -C src dgdebug

test/regtest/%.zblorb: src/dialogc test/regtest/%.dg stdlib.dg
	$+

test: test/regtest/cloak.zblorb src/dgdebug
	bin/regtest.py -v --game test/regtest/cloak.zblorb --interpreter dfrotz test/regtest/cloak.regtest
	$(MAKE) --directory=./test/gosling test clean
	$(MAKE) --directory=./test/impossible test clean
	$(MAKE) --directory=./test/simple all
	$(MAKE) -C test/unit test
	bin/test.py doc

distclean: clean

clean:
	$(MAKE) -C src clean
	$(MAKE) -C test/gosling clean
	$(MAKE) -C test/impossible clean

.PHONY:	test clean distclean src/dialogc src/dgdebug all
