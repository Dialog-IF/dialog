all: test 

DEBUG=dgdebug -q
STDLIB=testrunner.dg ../../stdlib.dg | tee log.txt \
	| grep "tests passed successfully"

test: time

time:
	$(DEBUG) time-tests.dg time.dg $(STDLIB)

.PHONY: test all time
