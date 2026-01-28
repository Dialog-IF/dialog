all: src/dialogc src/dgdebug test

src/dialogc:
	$(MAKE) -C src dialogc

src/dgdebug:
	$(MAKE) -C src dgdebug

test/regtest/%.zblorb: src/dialogc test/regtest/%.dg stdlib.dg
	$+

test: test/regtest/cloak.zblorb src/dgdebug
	bin/regtest.py -v --game test/regtest/cloak.zblorb --interpreter dfrotz test/regtest/cloak.regtest
	make --directory=./test/gosling test clean
	make --directory=./test/impossible test clean
	make --directory=./test/simple all
	make --directory=./test/unit test
	bin/test.py doc

.PHONY:		test src/dialogc src/dgdebug all
