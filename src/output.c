#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "output.h"
#include "terminal.h"
#include "unicode.h"

enum {
	SP_AUTO = 0,
	SP_INHIBIT,
	SP_SPACE,
	SP_DONESPACE,
	SP_DONELINE	// + number of blank lines printed
};

#define MAXNEST 64

enum {
	CLA_MAIN,
	CLA_STATUS,
	CLA_INTDEBUG,
	CLA_DEBUG,
	CLA_DEBUGIN,
	CLA_TRACE,
	CLA_SPAN,
	CLA_UNKNOWN
};

struct boxstate {
	uint8_t		boxclass;
	uint8_t		style;
	uint8_t		upper;
	uint8_t		visible;
	uint8_t		wrap;
};

static struct boxstate *boxstack;
static int nalloc_box;
static int boxsp;
static int space;
static uint16_t *wrapbuf;
static int delayed_spaces;
static int wrappos;
static int wrapstyle;
static int column;
static int width = 79, height = 0;
static int dfrotz_quirks;
static int force_width;

// These routines correspond to part of what the Z-machine is doing.

static void syncwrap();

static void update_size() {
	int w;

	term_get_size(&w, &height);
	if(force_width) w = force_width;
	if(w < width) syncwrap();
	width = w;
	wrapbuf = realloc(wrapbuf, (width + 1) * sizeof(uint16_t));
}

static void syncwrap() {
	int i;
	uint8_t utf8[(wrappos + 1) * 3];

	if(wrappos
	&& column + delayed_spaces + wrappos > width
	&& boxstack[boxsp].wrap) {
		delayed_spaces = 0;
		if(term_sendlf()) update_size();
		column = 0;
	}
	if(!wrapstyle) {
		term_effectstyle(0);
	}
	for(i = 0; i < delayed_spaces; i++) {
		term_sendbytes((uint8_t *) " ", 1);
		column++;
	}
	delayed_spaces = 0;
	if(wrapstyle) {
		term_effectstyle(wrapstyle);
	}
	unicode_to_utf8_n(utf8, (wrappos + 1) * 3, wrapbuf, wrappos);
	term_sendbytes(utf8, strlen((char *) utf8));
	column += wrappos;
	wrappos = 0;
}

static void sendstr_n(const char *str, int n) {
	int i;
	uint16_t ch, wstr[n + 1];

	utf8_to_unicode_n(wstr, n + 1, (uint8_t *) str, n);

	for(i = 0; wstr[i]; i++) {
		ch = wstr[i];
		if(boxstack[boxsp].upper) {
			if(ch >= 'a' && ch <= 'z') {
				ch = ch - 'a' + 'A';
			} else if(ch >= 0x80) {
				ch = unicode_to_upper(ch);
			}
			boxstack[boxsp].upper = 0;
		}
		if(wrappos >= width) {
			syncwrap();
		}
		wrapbuf[wrappos++] = ch;
		if(ch == '-') syncwrap();
	}
}

static void sendstr(const char *str) {
	sendstr_n(str, strlen(str));
}

static void sendlf() {
	syncwrap();
	if(term_sendlf()) update_size();
	column = 0;
	delayed_spaces = 0;
}

static void sendspace() {
	if(!boxstack[boxsp].visible) return;

	syncwrap();
	if(column) {
		delayed_spaces++;
	}
}

static void sendstyle(int style) {
	if(wrappos) syncwrap();
	wrapstyle = style;
}

// These routines correspond to what the runtime layer is doing.

void o_line() {
	if(!boxstack[boxsp].visible) return;

	if(space < SP_DONELINE) {
		sendlf();
		space = SP_DONELINE;
	}
}

void o_par_n(int n) {
	if(!boxstack[boxsp].visible) return;

	if(height && n > height) n = height;
	o_line();
	while(space < SP_DONELINE + n) {
		sendlf();
		space++;
	}
}

void o_par() {
	o_par_n(1);
}

void o_begin_box(char *boxclass) {
	if(boxsp == nalloc_box - 1) {
		nalloc_box = 2 * boxsp + 8;
		boxstack = realloc(boxstack, nalloc_box * sizeof(struct boxstate));
	}
	boxsp++;

	boxstack[boxsp].visible = boxstack[boxsp - 1].visible;
	boxstack[boxsp].style = 0;
	boxstack[boxsp].upper = 0;
	boxstack[boxsp].wrap = !term_handles_wrapping();
	if(!strcmp(boxclass, "span")) {
		boxstack[boxsp].boxclass = CLA_SPAN;
	} else {
		o_line();
		if(!strcmp(boxclass, "status")) {
			boxstack[boxsp].boxclass = CLA_STATUS;
			boxstack[boxsp].visible = 0;
			boxstack[boxsp].wrap = 0;
		} else if(!strcmp(boxclass, "trace")) {
			boxstack[boxsp].boxclass = CLA_TRACE;
			boxstack[boxsp].wrap = 0;
		} else if(!strcmp(boxclass, "debugger")) {
			boxstack[boxsp].boxclass = CLA_DEBUG;
			boxstack[boxsp].visible = 1;
			boxstack[boxsp].style = STYLE_DEBUG;
		} else if(!strcmp(boxclass, "intdebugger")) {
			boxstack[boxsp].boxclass = CLA_INTDEBUG;
			boxstack[boxsp].visible = term_is_interactive();
			boxstack[boxsp].style = STYLE_DEBUG;
		} else if(!strcmp(boxclass, "debuginput")) {
			boxstack[boxsp].boxclass = CLA_DEBUGIN;
			boxstack[boxsp].visible = 1;
		} else {
			boxstack[boxsp].boxclass = CLA_UNKNOWN;
		}
	}
	sendstyle(boxstack[boxsp].style);
}

void o_end_box() {
	if(boxsp) {
		if(boxstack[boxsp].boxclass != CLA_SPAN) {
			o_line();
		}
		boxsp--;
		sendstyle(boxstack[boxsp].style);
	} else {
		o_line();
	}
}

void o_space() {
	if(!boxstack[boxsp].visible) return;

	if(space < SP_SPACE) {
		space = SP_SPACE;
	}
}

void o_space_n(int n) {
	if(!boxstack[boxsp].visible) return;

	while(n-- > 0) sendstr(" ");
	space = SP_DONESPACE;
}

void o_nospace() {
	if(!boxstack[boxsp].visible) return;

	if(space < SP_INHIBIT) {
		space = SP_INHIBIT;
	}
}

void o_sync() {
	if(!boxstack[boxsp].visible) return;

	if(space == SP_AUTO || space == SP_SPACE) {
		sendspace();
		space = SP_DONESPACE;
	}
	syncwrap();
	term_effectstyle(wrapstyle);
}

void o_set_style(int style) {
	if(!boxstack[boxsp].visible) return;

	if(style) {
		if(space == SP_AUTO || space == SP_SPACE) {
			sendspace();
			space = SP_DONESPACE;
		}
		boxstack[boxsp].style |= style;
	} else {
		boxstack[boxsp].style &= STYLE_DEBUG;
	}
	sendstyle(boxstack[boxsp].style);
}

void o_set_upper() {
	if(!boxstack[boxsp].visible) return;

	boxstack[boxsp].upper = 1;
}

void o_print_word_n(const char *utf8, int n) {
	if(!boxstack[boxsp].visible) return;

	if(n) {
		if(space == SP_SPACE) {
			sendspace();
		} else if(space == SP_AUTO && !strchr(NO_SPACE_BEFORE " ", *utf8)) {
			sendspace();
		}
		sendstr_n(utf8, n);
		space = strchr(NO_SPACE_AFTER " ", utf8[n - 1])? SP_INHIBIT : SP_AUTO;
	}
}

void o_print_word(const char *utf8) {
	o_print_word_n(utf8, strlen(utf8));
}

void o_print_opaque_word(const char *utf8) {
	if(!boxstack[boxsp].visible) return;

	if(space == SP_SPACE || space == SP_AUTO) {
		sendspace();
	}
	sendstr_n(utf8, strlen(utf8));
	space = SP_AUTO;
}

void o_print_str(const char *utf8) {
	int i, n;

	if(*utf8) {
		i = 0;
		while(utf8[i]) {
			for(n = 0; utf8[i + n] && utf8[i + n] != ' '; n++);
			o_print_word_n(utf8 + i, n);
			i += n;
			while(utf8[i] == ' ') {
				o_space();
				i++;
			}
		}
	}
}

void o_begin_link(const char *utf8) {
	uint8_t saved_upper = boxstack[boxsp].upper;
	boxstack[boxsp].upper = 0;
	o_print_str("<[");
	o_print_str(utf8);
	o_print_str("] ");
	boxstack[boxsp].upper = saved_upper;
}

void o_end_link() {
	o_print_str(">");
}

void o_begin_self_link() {
	o_print_str("<");
}

void o_end_self_link() {
	o_print_str(">");
}

void o_progress_bar(int position, int total) {
	int i;

	update_size();
	if(position >= total) {
		position = width - 3;
	} else {
		position = position * (width - 3) / total;
	}
	o_begin_box("box");
	o_set_style(STYLE_FIXED);
	o_print_str("[");
	o_nospace();
	for(i = 0; i < position; i++) {
		o_print_str("=");
		o_nospace();
	}
	if(position < width - 3) {
		o_space_n(width - 3 - position);
	}
	o_print_str("]");
	o_end_box();
}

void o_clear(int all) {
	if(!boxstack[boxsp].visible) return;

	o_sync();
	term_clear(all);
	update_size();
	space = SP_DONELINE + (dfrotz_quirks? 999 : 0);
	column = 0;
	delayed_spaces = 0;
}

void o_post_input(int external_lf) {
	update_size();
	if(external_lf) {
		space = SP_DONELINE + (dfrotz_quirks? 999 : 0);
		column = 0;
		delayed_spaces = 0;
	}
}

void o_reset(int force_w, int quirks) {
	force_width = force_w;
	if(force_width) width = force_width;
	space = SP_DONELINE + (quirks? 999 : 0);
	wrapbuf = realloc(wrapbuf, (width + 1) * sizeof(uint16_t));
	wrappos = 0;
	boxsp = 0;
	if(!nalloc_box) {
		nalloc_box = 8;
		boxstack = malloc(nalloc_box * sizeof(struct boxstate));
	}
	boxstack[boxsp].boxclass = CLA_MAIN;
	boxstack[boxsp].style = STYLE_ROMAN;
	boxstack[boxsp].upper = 0;
	boxstack[boxsp].visible = 1;
	boxstack[boxsp].wrap = !term_handles_wrapping();
	dfrotz_quirks = quirks;
}

void o_leave_all() {
	if(boxstack[boxsp].boxclass != CLA_DEBUG
	&& boxstack[boxsp].boxclass != CLA_INTDEBUG) {
		o_sync();
		boxsp = 0;
		boxstack[boxsp].boxclass = CLA_MAIN;
		boxstack[boxsp].style = STYLE_ROMAN;
		boxstack[boxsp].upper = 0;
		boxstack[boxsp].visible = 1;
		boxstack[boxsp].wrap = !term_handles_wrapping();
	}
}

void o_cleanup() {
	o_sync();
	free(wrapbuf);
	wrapbuf = 0;
	free(boxstack);
	boxstack = 0;
	nalloc_box = 0;
}

int o_get_width() {
	return width;
}
