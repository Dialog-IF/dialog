# Call `make DIFF=meld`, for example, to get a fancy diff
# If the build fails on a Mac, change --ignore-trailing-space to --ignore-space-change
DIFF = diff --ignore-trailing-space

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
	perl -i -pe 's/\(serial number\)/<SERIAL>/g' stdlib.dglib
	perl -i -pe 's/\(compiler version\)/<COMPILER>/g' stdlib.dglib
	perl -i -pe 's/\s\(library version\)/\t<LIBRARY>/g' stdlib.dglib
## The \s is to ensure we don't wreck the *declaration* of (library version), which would crash the compiler

# Create the original gold files based on the gold-standard (i.e. unmodified) dialogc and dgdebug
%.orig_gold: %.dg %.debug %.z.in %.d.in stdlib.dglib
## Call the original dialogc and dgdebug from $PATH here, not our modified ones from $(BASEPATH)
	touch -a $*.gold
## First, make the Z-machine version
	dialogc $*.dg stdlib.dglib -t z5 -o $*.z5 --no-warn-not-topic
	$(BASEPATH)/bin/echoing.py dfrotz -w 80 -s 1234 -h 999 -mq $*.z5 <$*.z.in >$*.z.out
## For consistency, strip final spaces - those are mostly inserted by the debugger, but can happen in some cases, and this makes the output align
	perl -i -pe 's/ $$//' $*.z.out
## Then, the debugger version
	dgdebug -qD -w 80 -s 1234 $*.debug --no-warn-not-topic <$*.d.in >$*.d.out
## Remove one space from the end of each line
	perl -i -pe 's/ $$//' $*.d.out
## And the last character, which is an excess newline
##	sed -i '$$d' $*.d.out
## And meld them
	meld $*.d.out $*.z.out $*.gold
## Then cleanup
	rm -f $*.z.out $*.d.out $*.z5

origin: $(origin)

%.z5: $(BASEPATH)/src/dialogc %.dg stdlib.dglib
	$(BASEPATH)/src/dialogc $*.dg stdlib.dglib -t z5 -o $*.z5 --no-warn-not-topic

%.aastory: $(BASEPATH)/src/dialogc %.dg stdlib.dglib
	$(BASEPATH)/src/dialogc $*.dg stdlib.dglib -t aa -o $*.aastory --no-warn-not-topic

%.debug: %.dg stdlib.dglib
	cat $*.dg stdlib.dglib > $*.debug

%.z.gold %.aa.gold %.d.gold: %.gold
	$(BASEPATH)/bin/striate.py $*.gold Z $*.z.gold A $*.aa.gold D $*.d.gold
## dgdebug prints an extra final newline, and it's easier to just account for that than to remove it
	echo >> $*.d.gold

%.z.in %.aa.in %.d.in:
## If we have a .in file, then split it into three
## Otherwise, just make empty files, no input is expected
	if [ -f $*.in ]; then \
		$(BASEPATH)/bin/striate.py $*.in Z $*.z.in A $*.aa.in D $*.d.in ; \
	else \
		touch $*.z.in $*.aa.in $*.d.in ; \
	fi

%.z.out: %.z5 %.z.in
	$(BASEPATH)/bin/echoing.py dfrotz -w 80 -s 1234 -h 999 -mq $*.z5 <$*.z.in >$*.z.out
## Remove trailing spaces from lines for easier diffing
	perl -i -pe 's/ $$//' $*.z.out
	echo >>$*.z.out

%.aa.out: %.aastory %.aa.in
	echo TODO IMPLEMENT THIS
	touch $*.aa.out

%.d.out: $(BASEPATH)/src/dgdebug %.debug %.d.in
	$(BASEPATH)/src/dgdebug -qD -w 80 -s 1234 $*.debug --no-warn-not-topic <$*.d.in >$*.d.out
## Remove trailing spaces from lines for easier diffing
	perl -i -pe 's/ $$//' $*.d.out
	echo >>$*.d.out

%.test: %.z.out %.d.out %.z.gold %.d.gold
	$(DIFF) $*.z.out $*.z.gold
	$(DIFF) $*.d.out $*.d.gold

%.test_explicit: %.z.out %.d.out %.z.gold %.d.gold
	diff $*.z.out $*.z.gold || meld $*.z.out $*.z.gold
	diff $*.d.out $*.d.gold || meld $*.d.out $*.d.gold

clean:
	rm -rf *.out *.z.gold *.aa.gold *.d.gold *.z.in *.aa.in *.d.in stdlib.dglib *.z5
