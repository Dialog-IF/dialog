# Call `make DIFF=meld` to get a fancy diff
# If the build fails on a Mac, change --ignore-trailing-space to --ignore-space-change
DIFF = diff --ignore-trailing-space

# Base of the repository
BASEPATH = ../../..
DGDEBUG = $(BASEPATH)/src/dgdebug

# List of all .dg files with .dg changed to .test
tests := $(patsubst %.dg,%.test,$(wildcard *.dg))
explicit := $(patsubst %.dg,%.test_explicit,$(wildcard *.dg))

all: $(tests) clean

test: $(tests)

# This version shows meld results afterward if there were differences
explicit: $(explicit)

# Ensure we don't have to worry about (serial number) changing by date
# This is the only difference between this Makefile and the one in ../language/
# So if you edit one, make sure to edit the other to keep them in sync!
# (Also that the following rules rely on this one)
stdlib.dglib: $(BASEPATH)/stdlib.dg
	cp $(BASEPATH)/stdlib.dg stdlib.dglib
	perl -i -pe 's/\(serial number\)/<SERIAL>/g' stdlib.dglib
	perl -i -pe 's/\(compiler version\)/<COMPILER>/g' stdlib.dglib
	perl -i -pe 's/\s\(library version\)/\t<LIBRARY>/g' stdlib.dglib
## The \s is to ensure we don't wreck the *declaration* of (library version), which would crash the compiler

%.debug: %.dg stdlib.dglib
	cat $*.dg stdlib.dglib > $*.debug

linetags.out: $(DGDEBUG) linetags.debug linetags.in
	$(DGDEBUG) -qD -w 80 -s 1234 linetags.debug --no-header --tag-lines <linetags.in >linetags.out
	perl -i -pe 's/ $$//' linetags.out

numbered.out: $(DGDEBUG) numbered.dg dummylib.dglib
	$(DGDEBUG) -qD -w 80 -s 1234 numbered.dg dummylib.dglib --numbered >numbered.out
	perl -i -pe 's/ $$//' numbered.out

%.out: $(DGDEBUG) %.debug %.in
	$(DGDEBUG) -qD -w 80 -s 1234 $*.debug <$*.in >$*.out
## Remove trailing spaces from lines for easier diffing
	perl -i -pe 's/ $$//' $*.out

%.test: %.out %.gold $(DGDEBUG)
	$(DIFF) $*.out $*.gold

%.test_explicit: %.out %.gold $(DGDEBUG)
	diff $*.out $*.gold || meld $*.out $*.gold

clean:
	rm -rf *.out *.debug stdlib.dglib

.PHONY: all test clean
