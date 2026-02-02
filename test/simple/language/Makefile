# Call `make DIFF=meld` to get a fancy diff
DIFF = diff --ignore-trailing-space

# Base of the repository
BASEPATH = ../../..

# List of all .dg files with .dg changed to .test
tests := $(patsubst %.dg,%.test,$(wildcard *.dg))
rawgold := $(patsubst %.dg,%.gold,$(wildcard *.dg))
rawinput := $(patsubst %.dg,%.in,$(wildcard *.dg))
origin := $(patsubst %.dg,%.orig_gold,$(wildcard *.dg))
explicit := $(patsubst %.dg,%.test_explicit,$(wildcard *.dg))

all: $(tests)

# This version shows meld results afterward if there were differences
explicit: $(explicit)

# Create the original gold files based on the gold-standard (i.e. unmodified) dialogc and dgdebug
%.orig_gold: %.dg %.debug %.z.in %.d.in
## Call the original dialogc and dgdebug from $PATH here, not our modified ones from $(BASEPATH)
	touch -a $*.gold
## First, make the Z-machine version
	dialogc $*.dg dummylib.dglib --no-warn-not-topic -t z5 -o $*.z5
	$(BASEPATH)/bin/echoing.py dfrotz -w 80 -s 1234 -h 999 -mq $*.z5 <$*.z.in >$*.z.out
## For consistency, strip final spaces - those are mostly inserted by the debugger, but can happen in some cases, and this makes the output align
	perl -i -pe 's/ $$//' $*.z.out
## Then, the debugger version
	dgdebug -qD -w 80 -s 1234 $*.debug <$*.d.in >$*.d.out
## Remove one space from the end of each line
	perl -i -pe 's/ $$//' $*.d.out
## And the last character, which is an excess newline
	perl -i -pe '$$d' $*.d.out
## And meld them
	meld $*.d.out $*.z.out $*.gold
## Then cleanup
	rm -f $*.z.out $*.d.out $*.z5

origin: $(origin)

%.z5: $(BASEPATH)/src/dialogc %.dg
	$(BASEPATH)/src/dialogc $*.dg dummylib.dglib --no-warn-not-topic -t z5 -o $*.z5

%.aastory: $(BASEPATH)/src/dialogc %.dg
	$(BASEPATH)/src/dialogc $*.dg dummylib.dglib --no-warn-not-topic -t aa -o $*.aastory

%.debug: %.dg
	cat $*.dg dummylib.dglib > $*.debug

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

%.aa.out: %.aastory %.aa.in
	$(BASEPATH)/bin/aamrun.py $*.aastory <$*.aa.in >$*.aa.out
## Remove trailing spaces from lines for easier diffing
	perl -i -pe 's/ $$//' $*.aa.out
## When getting character input, Node interpreter inexplicably sometimes produces CRLF instead of LF, so we need to account for that
	perl -i -pe 's/\r\n/\n/g' $*.aa.out

%.d.out: $(BASEPATH)/src/dgdebug %.debug %.d.in
	$(BASEPATH)/src/dgdebug -qD -w 80 -s 1234 $*.debug --no-warn-not-topic <$*.d.in >$*.d.out
## Remove trailing spaces from lines for easier diffing
	perl -i -pe 's/ $$//' $*.d.out

%.test: %.z.out %.d.out %.aa.out %.z.gold %.d.gold %.aa.gold
	$(DIFF) $*.z.out $*.z.gold
	$(DIFF) $*.d.out $*.d.gold
	$(DIFF) $*.aa.out $*.aa.gold

%.test_explicit: %.z.out %.d.out %.aa.out %.z.gold %.d.gold %.aa.gold
	diff $*.z.out $*.z.gold || meld $*.z.out $*.z.gold
	diff $*.d.out $*.d.gold || meld $*.d.out $*.d.gold
	diff $*.aa.out $*.aa.gold || meld $*.aa.out $*.aa.gold

clean:
	rm -rf *.out *.z.gold *.aa.gold *.d.gold *.z.in *.aa.in *.d.in *.z5 *.aastory
