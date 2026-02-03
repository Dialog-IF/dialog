all: language library warnings

language: ../../src/dialogc ../../src/dgdebug
	make --directory=./language all clean

library: ../../src/dialogc ../../src/dgdebug
	make --directory=./library all clean

warnings: ../../src/dialogc
	make --directory=./warnings all clean

clean:
	make -C language clean
	make -C library clean
	make -C warnings clean

.PHONY:		all library warnings clean
