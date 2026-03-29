all: test 

DEBUG=../../src/dgdebug -u
STDLIB=../../unit.dg ../../stdlib.dg

test: time utils pass-1 fail-1

time:
	$(DEBUG) time-tests.dg time.dg $(STDLIB)

utils:
	$(DEBUG) utils-tests.dg utils.dg $(STDLIB) \
		 | grep "3 TESTS FAILED." > /dev/null

pass-1:
	$(DEBUG) pass-1.dg $(STDLIB) \
		 | grep "1 test passed successfully." > /dev/null

fail-1:
	$(DEBUG) fail-1.dg $(STDLIB) \
		 | grep "1 TEST FAILED." > /dev/null

clean:

.PHONY: test all time clean
