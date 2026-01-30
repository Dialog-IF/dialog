# Call `make DIFF=meld`, for example, to get a fancy diff
DIFF = diff

all: zmachine debugger

gosling.z8: ../../src/dialogc gosling_complete_nostyles.dg
	../../src/dialogc -t z8 gosling_complete_nostyles.dg -o gosling.z8 --no-warn-not-topic

zmachine.out: gosling.z8 gosling.in
	dfrotz -m -s 1234 gosling.z8 <gosling.in >zmachine.out
	echo "" >> zmachine.out

zmachine: zmachine.out
	$(DIFF) zmachine.out zmachine.gold

debugger.out: ../../src/dgdebug gosling_complete_unicode.dg gosling.in
	../../src/dgdebug -s 1234 gosling_complete_unicode.dg --no-warn-not-topic <gosling.in >debugger.out

debugger: debugger.out
	$(DIFF) debugger.out debugger.gold

test: debugger zmachine

clean:
	rm -f gosling.z8 zmachine.out debugger.out

.PHONY:		test clean debugger zmachine
