typedef uint32_t line_t;

#define MKLINE(file, line) (((file) << 24) | (line))
#define FILENUMPART(line) ((line) >> 24)
#define FILEPART(line) (sourcefile[FILENUMPART(line)])
#define LINEPART(line) ((line) & 0xffffff)

extern char **sourcefile;
extern int nsourcefile;

extern int verbose;

#define NO_SPACE_BEFORE ".,:;!?)]}>-%/| \t"
#define NO_SPACE_AFTER "([{<-/#@| \t"

#define STYLE_ROMAN	0
#define STYLE_REVERSE	1
#define STYLE_BOLD	2
#define STYLE_ITALIC	4
#define STYLE_FIXED	8
#define STYLE_DEBUG	16
#define STYLE_INPUT	32
