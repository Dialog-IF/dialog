all: test 

DEBUG=../../src/dgdebug -u
STDLIB=../../unit.dg ../../stdlib.dg

test: time utils pass-1 fail-1 zero-tests fatal-error

time:
	$(DEBUG) time-tests.dg time.dg $(STDLIB)

utils:
	$(DEBUG) utils-tests.dg utils.dg $(STDLIB) \
		 | grep "3 TESTS FAILED" > /dev/null

pass-1:
	$(DEBUG) pass-1.dg $(STDLIB) \
		 | grep "1 test passed successfully" > /dev/null

fail-1:
	$(DEBUG) fail-1.dg $(STDLIB) \
		 | grep "1 TEST FAILED" > /dev/null

zero-tests:
	$(DEBUG) zero-tests.dg $(STDLIB) \
		 | grep "0 tests passed successfully" > /dev/null

fatal-error:
	$(DEBUG) fatal-error.dg $(STDLIB) \
		 | grep "FATAL ERROR" > /dev/null

clean:
	rm -f *~ \#*\#

.PHONY: test all clean time utils pass-1 fail-1 fatal-error
