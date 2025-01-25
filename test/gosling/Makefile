all: zmachine debugger

gosling.z8: gosling_complete_nostyles.dg
	../../src/dialogc -t z8 gosling_complete_nostyles.dg -o gosling.z8

zmachine: gosling.z8 zmachine.in
	dfrotz -m -s 1234 gosling.z8 <zmachine.in >zmachine.out
	meld zmachine.out zmachine.gold
	# Meld is just a graphical version of diff, for automation diff should be fine

debugger: gosling_complete_unicode.dg debugger.in
	../../src/dgdebug -s 1234 gosling_complete_unicode.dg <debugger.in >debugger.out
	meld debugger.out debugger.gold

clean:
	rm -f gosling.z5 zmachine.out debugger.out
