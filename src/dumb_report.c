#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "common.h"
#include "report.h"

int verbose = 0;

void report(reportlevel_t level, line_t line, char *fmt, ...) {
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

	if(prefix) {
		fprintf(stderr, "%s ", prefix);
	}
	if(line) {
		fprintf(stderr, "%s, line %d: ", FILEPART(line), LINEPART(line));
	}
	va_start(valist, fmt);
	vfprintf(stderr, fmt, valist);
	va_end(valist);
	fprintf(stderr, "\n");
}
