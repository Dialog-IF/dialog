all: test 

DEBUG=../../src/dgdebug -q --no-warn-not-topic
STDLIB=../../unit.dg ../../stdlib.dg

test: time utils

time:
	$(DEBUG) time-tests.dg time.dg $(STDLIB)

utils:
	$(DEBUG) utils-tests.dg utils.dg $(STDLIB) | grep "3 TESTS FAILED."

clean:

.PHONY: test all time clean
