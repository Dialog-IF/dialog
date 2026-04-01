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

tidy:
	$(MAKE) -C src tidy
	$(MAKE) -C test clean

clean: tidy
	$(MAKE) -C src clean

install: src/dialogc src/dgdebug
	$(MAKE) -C src install

uninstall:
	$(MAKE) -C src uninstall

distclean: clean
	$(MAKE) -C src distclean

.PHONY:	test clean tidy install uninstall distclean src/dialogc src/dgdebug all test-6502
