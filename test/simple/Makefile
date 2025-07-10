all: language library warnings

language: ../../src/dialogc ../../src/dgdebug
	make --directory=./language all clean

library: ../../src/dialogc ../../src/dgdebug
	make --directory=./library all clean

warnings: ../../src/dialogc
	make --directory=./warnings all clean

.PHONY:		all library warnings
