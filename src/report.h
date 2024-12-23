
typedef enum {
	LVL_DEBUG,
	LVL_INFO,
	LVL_NOTE,
	LVL_WARN,
	LVL_ERR
} reportlevel_t;

void report(reportlevel_t level, line_t line, char *fmt, ...);
