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
