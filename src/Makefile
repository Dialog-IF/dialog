CC		= gcc
CFLAGS		+= -O3 -Wall -DVERSION=\"0m/01\"
WININCLUDE	= winglk
WINLIB		= ../prebuilt/win32

MINGW32		= i686-w64-mingw32-gcc
WINDRES		= i686-w64-mingw32-windres

all:			dialogc dgdebug

clean:
			rm -f dialogc dgdebug dialogc.exe dgdebug.exe *.o

install:		dialogc dgdebug
			cp dialogc /usr/local/bin/
			cp dgdebug /usr/local/bin/

.PHONY:			all clean install dialogc.exe dgdebug.exe

dialogc:		frontend.o backend_z.o runtime_z.o blorb.o dumb_output.o dumb_report.o arena.o ast.o parse.o compile.o eval.o accesspred.o unicode.o backend.o aavm.o backend_aa.o crc32.o
			${CC} ${LDFLAGS} -o $@ $^ ${LDLIBS}

dgdebug:		debugger.o frontend.o report.o arena.o ast.o parse.o compile.o eval.o term_tty.o accesspred.o output.o unicode.o fs_tty.o
			${CC} ${LDFLAGS} -o $@ $^ ${LDLIBS}

dialogc.exe:		frontend.c backend_z.c runtime_z.c blorb.c dumb_output.c dumb_report.c arena.c ast.c parse.c compile.c eval.c accesspred.c unicode.c backend.c aavm.c backend_aa.c crc32.c
			${MINGW32} ${CFLAGS} -o $@ $^

dgdebug.exe:		debugger.c frontend.c report.c arena.c ast.c parse.c compile.c eval.c accesspred.c output.c unicode.c term_winglk.c winglk-res.o fs_winglk.c
			${MINGW32} -L ${WINLIB} -I ${WININCLUDE} ${CFLAGS} -o $@ $^ -lGlk

winglk-res.o:		winglk-res.rc winglk-res.manifest
			${WINDRES} $< $@

frontend.o:		frontend.c compile.h arena.h ast.h frontend.h parse.h report.h eval.h accesspred.h unicode.h common.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

ast.o:			ast.c ast.h arena.h report.h common.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

backend.o:		backend.c backend_z.h backend_aa.h common.h arena.h ast.h frontend.h compile.h report.h unicode.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

backend_z.o:		backend_z.c ast.h frontend.h zcode.h blorb.h report.h common.h compile.h eval.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

backend_aa.o:		backend_aa.c backend_aa.h ast.h common.h arena.h report.h unicode.h compile.h eval.h aavm.h crc32.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

aavm.o:			aavm.c aavm.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

runtime_z.o:		runtime_z.c zcode.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

crc32.o:		crc32.c crc32.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

blorb.o:		blorb.c blorb.h common.h arena.h ast.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

report.o:		report.c report.h output.h terminal.h common.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

arena.o:		arena.c arena.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

parse.o:		parse.c arena.h ast.h parse.h common.h unicode.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

compile.o:		compile.c arena.h ast.h eval.h compile.h common.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

debugger.o:		debugger.c arena.h ast.h frontend.h report.h compile.h eval.h terminal.h output.h unicode.h common.h fs.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

eval.o:			eval.c arena.h ast.h compile.h eval.h report.h output.h terminal.h common.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

term_tty.o:		term_tty.c terminal.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

fs_tty.o:		fs_tty.c fs.h terminal.h output.h report.h common.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

accesspred.o:		accesspred.c arena.h ast.h accesspred.h common.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

output.o:		output.c output.h terminal.h unicode.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

unicode.o:		unicode.c unicode.h report.h common.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

dumb_report.o:		dumb_report.c report.h common.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<

dumb_output.o:		dumb_output.c report.h common.h Makefile
			${CC} -c ${CFLAGS} -o $@ $<
