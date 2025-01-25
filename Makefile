src/%:
	make --directory=./src $%

test/regtest/%.zblorb: src/dialogc test/regtest/%.dg stdlib.dg
	$+

test: test/regtest/cloak.zblorb
	bin/regtest.py -v --game test/regtest/cloak.zblorb --interpreter dfrotz test/regtest/cloak.regtest

.PHONY:		test src/%
