# If your build fails on a Mac, look at the comments in the Makefiles in
# test/simple/language, test/simple/library, and test/simple/warnings, and
# change the --ignore-trailing-spaces flag to --ignore-space-change in each.

all: src/dialogc src/dgdebug test

src/dialogc:
	$(MAKE) -C src dialogc

src/dgdebug:
	$(MAKE) -C src dgdebug

test: src/dgdebug
	$(MAKE) -C test all
	echo All normal tests successful!

# Splitting this one out because making the prereqs is hard on Mac
test-6502:
	$(MAKE) -C test/6502 6502 clean
	echo 6502 tests successful!

stdlib.unicode.dg: stdlib.dg Makefile
	## This runs a bunch of regexes on stdlib.dg to replace straight quotes with curly
	rm -f stdlib.unicode.dg
	echo "%% Auto-generated from stdlib.dg: straight quotes changed to curly" | cat - stdlib.dg > stdlib.unicode.dg
	## First: double-quotes between whitespace and non-whitespace
	perl -pi -e "s/(\S)\"(\s|$$)/\1”\2/g" stdlib.unicode.dg
	perl -pi -e "s/(\s)\"(\S)/\1“\2/g" stdlib.unicode.dg
	## Second: single-quotes between whitespace and non-whitespace
	perl -pi -e "s/(\s)\'(\S)/\1‘\2/g" stdlib.unicode.dg
	## The following one is more broad to hit "they're" etc
	perl -pi -e "s/(\S)\'/\1’/g" stdlib.unicode.dg
	## Third: double-quotes in parentheses
	perl -pi -e "s/\"\)/”\)/g" stdlib.unicode.dg
	perl -pi -e "s/\(\"/\(“/g" stdlib.unicode.dg
	## Fourth: double-quotes between words and any other punctuation
	perl -pi -e "s/(\w)\"(\W)/\1”\2/g" stdlib.unicode.dg
	perl -pi -e "s/(\W)\"(\w)/\1“\2/g" stdlib.unicode.dg
	## Not currently covered: something like " (print words $) " or "(print words $)"
	## Fortunately, this does not currently occur in the library
	grep "[\"']" stdlib.unicode.dg && exit 1 || echo Success!

tidy:
	$(MAKE) -C src tidy
	$(MAKE) -C test clean

clean: tidy
	$(MAKE) -C src clean
	rm -f stdlib.unicode.dg

install: src/dialogc src/dgdebug
	$(MAKE) -C src install

uninstall:
	$(MAKE) -C src uninstall

distclean: clean
	$(MAKE) -C src distclean

.PHONY:	test clean tidy install uninstall distclean src/dialogc src/dgdebug all test-6502
