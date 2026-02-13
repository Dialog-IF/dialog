# If your build fails on a Mac, look at the comments in the Makefiles in
# test/simple/language, test/simple/library, and test/simple/warnings, and
# change the --ignore-trailing-spaces flag to --ignore-space-change in each.

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

tidy:
	$(MAKE) -C src tidy
	$(MAKE) -C test/gosling clean
	$(MAKE) -C test/impossible clean

clean: tidy
	$(MAKE) -C src clean

install: test
	$(MAKE) -C src install

uninstall:
	$(MAKE) -C src uninstall

distclean: clean
	$(MAKE) -C src distclean

.PHONY:	test clean tidy install uninstall distclean src/dialogc src/dgdebug all
