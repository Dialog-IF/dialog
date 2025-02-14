# Call `make DIFF=meld`, for example, to get a fancy diff
DIFF = diff

BASEPATH = ../../..

# List of all .nolib.dg and .lib.dg files with .dg changed to .test
tests := $(patsubst %.dg,%.test,$(wildcard *.dg))
rawgold := $(patsubst %.dg,%.gold,$(wildcard *.dg))
rawinput := $(patsubst %.dg,%.in,$(wildcard *.dg))

all: touch-in-case $(tests)

touch-in-case:
	touch -a $(rawgold) $(rawinput)

# Ensure we don't have to worry about (serial number) changing by date
stdlib.dglib: $(BASEPATH)/stdlib.dg
	sed 's/(serial number)/DEBUG/g' $(BASEPATH)/stdlib.dg >stdlib.dglib

%.z5: $(BASEPATH)/src/dialogc %.dg stdlib.dglib
	$(BASEPATH)/src/dialogc $*.dg stdlib.dglib -t z5 -o $*.z5

%.aastory: $(BASEPATH)/src/dialogc %.dg stdlib.dglib
	$(BASEPATH)/src/dialogc $*.dg stdlib.dglib -t aa -o $*.aastory

%.debug: %.dg stdlib.dglib
	cat $*.dg stdlib.dglib > $*.debug

%.z.gold %.aa.gold %.d.gold: %.gold
	$(BASEPATH)/bin/striate.py $*.gold Z $*.z.gold A $*.aa.gold D $*.d.gold

%.z.in %.aa.in %.d.in: %.in
##	If we have a .in file, then split it into three
	if [ -f \*.in ]; then \
		$(BASEPATH)/bin/striate.py $*.in Z $*.z.in A $*.aa.in D $*.d.in ; \
## Otherwise, just make empty files, no input is expected
	else \
		touch $*.z.in $*.aa.in $*.d.in ; \
	fi

%.z.out: %.z5 %.z.in
	$(BASEPATH)/bin/echoing.py dfrotz -w 80 -s 1234 -h 999 -mq $*.z5 <$*.z.in >$*.z.out
	echo >> $*.z.out
## Restore the final newline

%.aa.out: %.aastory %.aa.in
	echo TODO IMPLEMENT THIS
	touch $*.aa.out

%.d.out: $(BASEPATH)/src/dgdebug %.debug %.d.in
	$(BASEPATH)/src/dgdebug -qD -w 80 -s 1234 $*.debug <$*.d.in >$*.d.out

%.test: %.z.out %.d.out %.z.gold %.d.gold
	$(DIFF) $*.z.out $*.z.gold
	$(DIFF) $*.d.out $*.d.gold

clean:
	rm -rf *.out *.z.gold *.aa.gold *.d.gold *.z.in *.aa.in *.d.in
