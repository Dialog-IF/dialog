all: test 

DEBUG=../../src/dgdebug -q --no-warn-not-topic
STDLIB=../../unit.dg ../../stdlib.dg | tee log.txt | grep "tests passed successfully"

test: time

time:
	$(DEBUG) time-tests.dg time.dg $(STDLIB)

clean:
	rm log.txt || exit 0

.PHONY: test all time clean
