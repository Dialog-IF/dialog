#define TERM_UP 129
#define TERM_DOWN 130
#define TERM_LEFT 131
#define TERM_RIGHT 132
#define TERM_DELETE 133

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
int term_sendfakelf(); // Do all the stuff surrounding a newline, but assume the newline was printed by something else already
void term_effectstyle(int style);
void term_clear(int all);

int term_is_interactive(void);
void term_get_size(int *width, int *height, int force_w, int force_h);
int term_handles_wrapping(void);
