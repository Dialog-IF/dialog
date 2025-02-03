# Call `make DIFF=meld`, for example, to get a fancy diff
DIFF = meld

# List of all .nolib.dg and .lib.dg files with .dg changed to .test
nolib := $(patsubst %.dg,%.test,$(wildcard *.nolib.dg))
lib := $(patsubst %.dg,%.test,$(wildcard *.lib.dg))
rawgold := $(patsubst %.dg,%.gold,$(wildcard *lib.dg))
rawinput := $(patsubst %.dg,%.in,$(wildcard *lib.dg))

all: touch-in-case $(nolib) $(lib) 

touch-in-case:
	touch -a $(rawgold) $(rawinput)

# Ensure we don't have to worry about (serial number) changing by date
stdlib.dg: ../../stdlib.dg
	sed 's/(serial number)/DEBUG/g' ../../stdlib.dg >stdlib.dg

%.nolib.z5: ../../src/dialogc %.nolib.dg
	../../src/dialogc $*.nolib.dg dummylib.dg -t z5 -o $*.nolib.z5

%.lib.z5: ../../src/dialogc %.lib.dg stdlib.dg
	../../src/dialogc $*.lib.dg stdlib.dg -t z5 -o $*.lib.z5

%.nolib.aastory: ../../src/dialogc %.nolib.dg
	../../src/dialogc $*.nolib.dg dummylib.dg -t aa -o $*.nolib.aastory

%.lib.aastory: ../../src/dialogc %.lib.dg stdlib.dg
	../../src/dialogc $*.lib.dg stdlib.dg -t aa -o $*.lib.aastory

%.nolib.debug: %.nolib.dg
	cat $*.nolib.dg dummylib.dg > $*.nolib.debug

%.lib.debug: %.lib.dg stdlib.dg
	cat $*.lib.dg stdlib.dg > $*.lib.debug

%.z.gold %.aa.gold %.d.gold: %.gold
	../../bin/striate.py $*.gold Z $*.z.gold A $*.aa.gold D $*.d.gold

%.z.in %.aa.in %.d.in: %.in
	../../bin/striate.py $*.in Z $*.z.in A $*.aa.in D $*.d.in

%.z.out: %.z5 %.z.in
	../../bin/echoing.py dfrotz -w 80 -s 1234 -h 999 -m $*.z5 <$*.z.in >$*.z.out
	sed -i 1,2d $*.z.out
## Remove the first two lines, they're just metadata
	echo >> $*.z.out
## But restore the final newline

%.aa.out: %.aastory %.aa.in
	echo TODO IMPLEMENT THIS
	touch $*.aa.out

%.d.out: ../../src/dgdebug %.debug %.d.in
	../../src/dgdebug -qD -w 80 -s 1234 $*.debug <$*.d.in >$*.d.out

%.test: %.z.out %.d.out %.z.gold %.d.gold
	$(DIFF) $*.z.out $*.z.gold
	$(DIFF) $*.d.out $*.d.gold

clean:
	rm -rf *.out *.z.gold *.aa.gold *.d.gold *.z.in *.aa.in *.d.in stdlib.in stdlib.gold dummylib.in dummylib.gold
