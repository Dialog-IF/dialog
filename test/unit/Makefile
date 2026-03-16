all: test 

DEBUG=../../src/dgdebug -q --no-warn-not-topic
STDLIB=../../unit.dg ../../stdlib.dg | grep "tests passed successfully"

test: time

time:
	$(DEBUG) time-tests.dg time.dg $(STDLIB)

clean:
	rm -f log.txt

.PHONY: test all time clean
