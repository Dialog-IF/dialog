CC	= gcc
CFLAGS	+= -O3 -Wall -DVERSION=\"0c/01\"

all:			dialogc

clean:
			rm -f dialogc *.o

install:		dialogc
			cp dialogc /usr/local/bin/

.PHONY:			all clean install

dialogc:		frontend.o backend_z.o runtime_z.o blorb.o report.o
			${CC} ${LDFLAGS} -o $@ $^ ${LDLIBS}

frontend.o:		frontend.c rules.h report.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

backend_z.o:		backend_z.c rules.h zcode.h blorb.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

runtime_z.o:		runtime_z.c rules.h zcode.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

blorb.o:		blorb.c blorb.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

report.o:		report.c rules.h report.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<
