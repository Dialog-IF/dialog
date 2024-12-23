#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "common.h"
#include "report.h"
#include "output.h"
#include "terminal.h"

int verbose = 0;

void report(reportlevel_t level, line_t line, char *fmt, ...) {
	char buf[1024];
	char *prefix = 0;

	va_list valist;

	switch(level) {
	case LVL_ERR:
		prefix = "Error:";
		break;
	case LVL_WARN:
		prefix = "Warning:";
		break;
	case LVL_NOTE:
		prefix = "Note:";
		break;
	case LVL_INFO:
		if(verbose < 1) return;
		prefix = "Info:";
		break;
	case LVL_DEBUG:
		if(verbose < 2) return;
		prefix = "Debug:";
		break;
	}

	o_begin_box("debugger");
	if(prefix) {
		o_set_style(STYLE_BOLD);
		o_print_word(prefix);
		o_set_style(STYLE_ROMAN);
	}
	if(line) {
		o_space();
		o_print_word(FILEPART(line));
		snprintf(buf, sizeof(buf), ", line %d:", LINEPART(line));
		o_print_str(buf);
	}
	va_start(valist, fmt);
	vsnprintf(buf, sizeof(buf), fmt, valist);
	va_end(valist);
	o_print_str(buf);
	o_end_box();
}
