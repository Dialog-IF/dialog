#define TERM_UP 129
#define TERM_DOWN 130
#define TERM_LEFT 131
#define TERM_RIGHT 132

#define STYLE_ROMAN	0
#define STYLE_REVERSE	1
#define STYLE_BOLD	2
#define STYLE_ITALIC	4
#define STYLE_FIXED	8
#define STYLE_DEBUG	16
#define STYLE_INPUT	32

typedef void (*term_int_callback_t)();

void term_init(term_int_callback_t callback);
void term_cleanup(void);
void term_quit(void);
char *term_quit_hint(void);
char *term_suspend_hint(void);
void term_ticker(void);

int term_getline(const char *prompt, uint8_t *buffer, int bufsize, int is_filename);
int term_getkey(const char *prompt);

void term_sendbytes(uint8_t *utf8, int nbyte);
int term_sendlf(); // returns true if this involved a more prompt
void term_effectstyle(int style);
void term_clear(int all);

int term_is_interactive(void);
void term_get_size(int *width, int *height);
int term_handles_wrapping(void);
