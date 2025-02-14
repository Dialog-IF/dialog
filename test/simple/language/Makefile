# Call `make DIFF=meld`, for example, to get a fancy diff
DIFF = diff

BASEPATH = ../../..

# List of all .nolib.dg and .lib.dg files with .dg changed to .test
tests := $(patsubst %.dg,%.test,$(wildcard *.dg))
rawgold := $(patsubst %.dg,%.gold,$(wildcard *.dg))
rawinput := $(patsubst %.dg,%.in,$(wildcard *.dg))
origin := $(patsubst %.dg,%.orig_gold,$(wildcard *.dg))

# Create the original gold files based on the gold-standard (i.e. unmodified) dialogc and dgdebug
%.orig_gold: %.dg %.debug %.z.in %.d.in
## Call the original dialogc and dgdebug from $PATH here, not our modified ones from $(BASEPATH)
	touch -a $*.gold
## First, make the Z-machine version
	dialogc $*.dg dummylib.dglib -t z5 -o $*.z5
	$(BASEPATH)/bin/echoing.py dfrotz -w 80 -s 1234 -h 999 -mq $*.z5 <$*.z.in >$*.z.out
## Then, the debugger version
	dgdebug -qD -w 80 -s 1234 $*.debug <$*.d.in >$*.d.out
	sed -i 's/ $$//' $*.d.out
## And meld them
	meld $*.d.out $*.z.out $*.gold
## Then cleanup
	rm -f $*.z.out $*.d.out $*.z5

origin: $(origin)

all: touch-in-case $(tests)

touch-in-case:
	touch -a $(rawgold) $(rawinput)

%.z5: $(BASEPATH)/src/dialogc %.dg
	$(BASEPATH)/src/dialogc $*.dg dummylib.dglib -t z5 -o $*.z5

%.aastory: $(BASEPATH)/src/dialogc %.dg
	$(BASEPATH)/src/dialogc $*.dg dummylib.dglib -t aa -o $*.aastory

%.debug: %.dg
	cat $*.dg dummylib.dglib > $*.debug

%.z.gold %.aa.gold %.d.gold: %.gold
	$(BASEPATH)/bin/striate.py $*.gold Z $*.z.gold A $*.aa.gold D $*.d.gold
## dgdebug prints an extra final newline, and it's easier to just account for that than to remove it
	echo >> $*.d.gold

%.z.in %.aa.in %.d.in:
## If we have a .in file, then split it into three
## Otherwise, just make empty files, no input is expected
	if [ -f \*.in ]; then \
		$(BASEPATH)/bin/striate.py $*.in Z $*.z.in A $*.aa.in D $*.d.in ; \
	else \
		touch $*.z.in $*.aa.in $*.d.in ; \
	fi

%.z.out: %.z5 %.z.in
	$(BASEPATH)/bin/echoing.py dfrotz -w 80 -s 1234 -h 999 -mq $*.z5 <$*.z.in >$*.z.out

%.aa.out: %.aastory %.aa.in
	echo TODO IMPLEMENT THIS
	touch $*.aa.out

%.d.out: $(BASEPATH)/src/dgdebug %.debug %.d.in
	$(BASEPATH)/src/dgdebug -qD -w 80 -s 1234 $*.debug <$*.d.in >$*.d.out
## Remove trailing spaces from lines for easier diffing
	sed -i 's/ $$//' $*.d.out

%.test: %.z.out %.d.out %.z.gold %.d.gold
	$(DIFF) $*.z.out $*.z.gold
	$(DIFF) $*.d.out $*.d.gold

clean:
	rm -rf *.out *.z.gold *.aa.gold *.d.gold *.z.in *.aa.in *.d.in
