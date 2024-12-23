#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "rules.h"
#include "report.h"

void report(reportlevel_t level, line_t line, char *fmt, ...) {
	va_list valist;

	switch(level) {
	case LVL_ERR:
		fprintf(stderr, "Error: ");
		break;
	case LVL_WARN:
		fprintf(stderr, "Warning: ");
		break;
	case LVL_NOTE:
		fprintf(stderr, "Note: ");
		break;
	case LVL_INFO:
		if(verbose < 1) return;
		fprintf(stderr, "Info: ");
		break;
	case LVL_DEBUG:
		if(verbose < 2) return;
		fprintf(stderr, "Debug: ");
		break;
	}

	if(line) {
		fprintf(stderr, "%s, line %d: ", FILEPART(line), LINEPART(line));
	}

	va_start(valist, fmt);
	vfprintf(stderr, fmt, valist);
	va_end(valist);

	fprintf(stderr, "\n");
}
