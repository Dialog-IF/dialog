all: test 

DEBUG=../../src/dgdebug -q --no-warn-not-topic
STDLIB=../../unit.dg ../../stdlib.dg

test: time

time:
	$(DEBUG) time-tests.dg time.dg $(STDLIB)

clean:
# nothing to do

.PHONY: test all time clean
