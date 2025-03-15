# Call `make DIFF=meld`, for example, to get a fancy diff
DIFF = diff

BASEPATH = ../../..

# List of all .nolib.dg and .lib.dg files with .dg changed to .test
tests := $(patsubst %.dg,%.test,$(wildcard *.dg))
rawgold := $(patsubst %.dg,%.gold,$(wildcard *.dg))
rawinput := $(patsubst %.dg,%.in,$(wildcard *.dg))
origin := $(patsubst %.dg,%.orig_gold,$(wildcard *.dg))
explicit := $(patsubst %.dg,%.test_explicit,$(wildcard *.dg))

all: $(tests)

# This version shows meld results afterward if there were differences
explicit: $(explicit)

# Ensure we don't have to worry about (serial number) changing by date
# This is the only difference between this Makefile and the one in ../language/
# So if you edit one, make sure to edit the other to keep them in sync!
# (Also that the following rules rely on this one)
stdlib.dglib: $(BASEPATH)/stdlib.dg
	cp $(BASEPATH)/stdlib.dg stdlib.dglib
	sed -i 's/(serial number)/<SERIAL>/g' stdlib.dglib
	sed -i 's/(compiler version)/<COMPILER>/g' stdlib.dglib
	sed -i 's/\s(library version)/\t<LIBRARY>/g' stdlib.dglib
## The \s is to ensure we don't wreck the *declaration* of (library version), which would crash the compiler

# Create the original gold files based on the gold-standard (i.e. unmodified) dialogc and dgdebug
%.orig_gold: %.dg stdlib.dglib
## Call the original dialogc and dgdebug from $PATH here, not our modified ones from $(BASEPATH)
	touch -a $*.gold
## Take the errors from the Z-machine version
	dialogc $*.dg stdlib.dglib -t z5 -o $*.z5 >$*.out 2>&1 || :
## And meld them
	meld $*.out $*.gold
## Then cleanup
	rm -f $*.out $*.z5

origin: $(origin)

%.out: $(BASEPATH)/src/dialogc %.dg stdlib.dglib
	$(BASEPATH)/src/dialogc $*.dg stdlib.dglib -t z5 -o $*.z5 >$*.out 2>&1 || :

%.test: %.out %.gold
	$(DIFF) $*.out $*.gold

%.test_explicit: %.out %.gold
	diff $*.out $*.gold || meld $*.out $*.gold

clean:
	rm -rf *.out stdlib.dglib
