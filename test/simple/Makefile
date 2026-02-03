all: language library warnings

language: ../../src/dialogc ../../src/dgdebug
	make --directory=./language all clean

library: ../../src/dialogc ../../src/dgdebug ../../stdlib.dg
	make --directory=./library all clean

warnings: ../../src/dialogc ../../stdlib.dg
	make --directory=./warnings all clean

clean:
	make -C language clean
	make -C library clean
	make -C warnings clean

.PHONY:		all language library warnings clean
