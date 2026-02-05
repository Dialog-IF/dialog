# Call `make DIFF=meld`, for example, to get a fancy diff
DIFF = diff

all: test

# Previously, the Z-machine had problems with Unicode, so we maintained a separate ASCII version of the source. Now, that problem is fixed, so we can use the Unicode version on all platforms. Separately, dfrotz had a problem with inline style changes, so we maintained a version that didn't style anything. That problem is also fixed. So gosling_complete_nostyles.dg is now vestigial, and we can use gosling_complete_unicode.dg for everything.

gosling.z8: ../../src/dialogc gosling_complete_unicode.dg
	../../src/dialogc -t z8 gosling_complete_unicode.dg -o gosling.z8 --no-warn-not-topic

zmachine.out: gosling.z8 gosling.in
	dfrotz -m -s 1234 gosling.z8 <gosling.in >zmachine.out
	echo "" >> zmachine.out

zmachine: zmachine.out
	$(DIFF) zmachine.out zmachine.gold

debugger.out: ../../src/dgdebug gosling_complete_unicode.dg gosling.in
	../../src/dgdebug -s 1234 gosling_complete_unicode.dg --no-warn-not-topic <gosling.in >debugger.out

debugger: debugger.out
	$(DIFF) debugger.out debugger.gold

# Building an aastory file requires that hints.html and map.png be present and nonempty, but they're not used in the testing, so the versions here are just dummy files - if you actually want to play the game, get the real thing from IFDB!
gosling.aastory: ../../src/dialogc gosling_complete_unicode.dg
	../../src/dialogc -t aa gosling_complete_unicode.dg -o gosling.aastory --no-warn-not-topic

aamachine.out: gosling.aastory gosling.in
	../../bin/aamrun.py gosling.aastory <gosling.in >aamachine.out

aamachine: aamachine.out
	$(DIFF) aamachine.out aamachine.gold

test: debugger zmachine aamachine

clean:
	rm -f gosling.z8 gosling.aastory zmachine.out debugger.out aamachine.out

.PHONY:		all test clean debugger zmachine aamachine
