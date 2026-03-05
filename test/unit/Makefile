all: test 

DEBUG=../../src/dgdebug -q --no-warn-not-topic
STDLIB=../../unit.dg ../../stdlib.dg

test: time

time:
	$(DEBUG) time-tests.dg time.dg $(STDLIB)

.PHONY: test all time
