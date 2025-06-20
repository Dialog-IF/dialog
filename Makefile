src/%:
	make --directory=./src $%

test/regtest/%.zblorb: src/dialogc test/regtest/%.dg stdlib.dg
	$+

test: test/regtest/cloak.zblorb src/dgdebug
	bin/regtest.py -v --game test/regtest/cloak.zblorb --interpreter dfrotz test/regtest/cloak.regtest
	make --directory=./test/gosling test clean
	make --directory=./test/impossible test clean
	make --directory=./test/simple all
	bin/test.py doc

.PHONY:		test src/%
