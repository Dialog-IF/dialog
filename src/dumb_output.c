#include <stdint.h>
#include <stdlib.h>

#include "common.h"
#include "report.h"

static int force_width;

static void bail_out() {
	report(LVL_ERR, 0, "Output is not allowed during initial value evaluation.");
	exit(1);
}

void o_line()				{ bail_out(); }
void o_par_n(int n)			{ bail_out(); }
void o_par()				{ bail_out(); }
void o_begin_box(char *boxclass) 	{ bail_out(); }
void o_end_box()			{ bail_out(); }
void o_space()				{ bail_out(); }
void o_space_n(int n)			{ bail_out(); }
void o_nospace()			{ bail_out(); }
void o_sync()				{ bail_out(); }
void o_set_style(int style)		{ bail_out(); }
void o_set_upper()			{ bail_out(); }
void o_print_word(const char *utf8)	{ bail_out(); }
void o_print_str(const char *utf8)	{ bail_out(); }
void o_begin_link(const char *utf8)	{ bail_out(); }
void o_end_link()			{ bail_out(); }
void o_progress_bar(int a, int b)	{ bail_out(); }
void o_clear(int all)			{ bail_out(); }
void o_post_input(int external_lf)	{ bail_out(); }

void o_reset(int force_w, int quirks) {
	force_width = force_w;
}

void o_cleanup() {
}

int o_get_width() {
	return force_width? force_width : 79;
}
