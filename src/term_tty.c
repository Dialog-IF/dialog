#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "terminal.h"

enum {
	STATE_BARE,
	STATE_ESC,
	STATE_CSI
};

struct histentry {
	struct histentry	*prev, *next;
	int			length;
	uint16_t		*content;
};

static struct histentry *tophist;
static term_int_callback_t term_int_callback;
static int unread_lines;
static int termstyle;
static int term_height;
static int did_tcsetattr;
static struct termios tio_orig;
static uint16_t last_filename[256];

void tty_setup() {
	struct termios tio;

	if(!tcgetattr(0, &tio)) {
		tio.c_lflag |= ISIG;
		memcpy(&tio_orig, &tio, sizeof(tio_orig));
		tio.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|IXON);
		tio.c_iflag |= ICRNL;
		tio.c_lflag &= ~(ECHO|ECHONL|ICANON|IEXTEN|ISIG);
		tio.c_cflag &= ~(CSIZE|PARENB);
		tio.c_cflag |= CS8;
		tcsetattr(0, TCSANOW, &tio);
		did_tcsetattr = 1;
	} else {
		did_tcsetattr = 0;
	}
}

void tty_restore() {
	if(did_tcsetattr) {
		tcsetattr(0, TCSANOW, &tio_orig);
		did_tcsetattr = 0;
	}
}

void term_ticker() {
}

void morefunc() {
	int i;

	printf("[more]");
	fflush(stdout);
	i = term_getkey("[more]");
	printf("\r      \r");
	if(i == -1) {
		exit(0);
	} else if(i < 0) {
		exit(1);
	} else {
		if(i == 3) term_int_callback();
	}
}

void term_get_size(int *width, int *height) {
	struct winsize ws;

	if(!ioctl(0, TIOCGWINSZ, &ws)) {
		*width = (ws.ws_col >= 1)? ws.ws_col - 1 : 0;
		*height = ws.ws_row;
		term_height = ws.ws_row;
	} else {
		*width = 79;
		*height = 0;
	}
}

int term_is_interactive() {
	return isatty(1);
}

void term_sendbytes(uint8_t *utf8, int nbyte) {
	fwrite(utf8, nbyte, 1, stdout);
}

void term_effectstyle(int style) {
	if(termstyle != style && isatty(1)) {
		printf("\033[0m");
		if(style & STYLE_BOLD) printf("\033[1m");
		if(style & STYLE_ITALIC) printf("\033[4m");
		if(style & STYLE_REVERSE) printf("\033[7m");
		if(style & STYLE_DEBUG) printf("\033[36m");
	}
	termstyle = style;
}

int term_sendlf() {
	fputc('\n', stdout);
	unread_lines++;
	if(term_height > 1 && isatty(1)) {
		if(unread_lines >= term_height - 1) {
			term_effectstyle(0);
			fflush(stdout);
			morefunc();
			unread_lines = 0;
			return 1;
		}
	} else {
		unread_lines = 0;
	}
	return 0;
}

void term_clear(int all) {
	if(unread_lines && isatty(1)) {
		term_effectstyle(0);
		fflush(stdout);
		morefunc();
		printf("\033c");
		unread_lines = 0;
	}
}

static int render(uint8_t *bytes, uint16_t ch) {
	if(ch < 0x80) {
		bytes[0] = ch;
		return 1;
	} else if(ch < 0x800) {
		bytes[0] = 0xc0 | (ch >> 6);
		bytes[1] = 0x80 | ((ch >> 0) & 0x3f);
		return 2;
	} else {
		bytes[0] = 0xe0 | (ch >> 12);
		bytes[1] = 0x80 | ((ch >> 6) & 0x3f);
		bytes[2] = 0x80 | ((ch >> 0) & 0x3f);
		return 3;
	}
}

static void charout(uint16_t ch) {
	uint8_t bytes[3];

	write(1, bytes, render(bytes, ch));
}

static void stepcursor(int n) {
	char buf[8];

	if(n > 0) {
		snprintf(buf, sizeof(buf), "\033[%dC", n);
		write(1, buf, strlen(buf));
	} else if(n < 0) {
		snprintf(buf, sizeof(buf), "\033[%dD", -n);
		write(1, buf, strlen(buf));
	}
}

static int addhist(uint16_t *str, int len) {
	struct histentry *he;

	if(len) {
		if(tophist
		&& tophist->length == len
		&& !memcmp(str, tophist->content, len * sizeof(uint16_t))) {
			return 0;
		}

		he = malloc(sizeof(*he));
		he->length = len;
		he->content = malloc(len * sizeof(uint16_t));
		memcpy(he->content, str, len * sizeof(uint16_t));
		he->prev = tophist;
		he->next = 0;
		if(tophist) tophist->next = he;
		tophist = he;
		return 1;
	} else {
		return 0;
	}
}

static void readhist(struct histentry *he, uint16_t *edbuf, int edbufsz, int *edlen, int *edx) {
	int i, len;

	stepcursor(-*edx);
	for(i = 0; i < *edlen; i++) {
		charout(' ');
	}
	stepcursor(-*edlen);
	if(he) {
		len = he->length;
		if(len > edbufsz - 1) len = edbufsz - 1;
		memcpy(edbuf, he->content, len * sizeof(uint16_t));
		*edlen = len;
	} else {
		*edlen = 0;
	}
	for(i = 0; i < *edlen; i++) {
		charout(edbuf[i]);
	}
	*edx = *edlen;
}

static int getkey() {
	uint8_t inbuf;
	int nexpected = 0;
	uint16_t accum = 0;
	ssize_t ssz;
	int state = STATE_BARE;

	for(;;) {
		ssz = read(0, &inbuf, 1);
		if(ssz < 0) {
			if(errno != EINTR && errno != EAGAIN) {
				fprintf(stderr, "%s\n", strerror(errno));
				return -2;
			}
		} else if(ssz == 0) {
			return -1;
		} else if((inbuf & 0xf0) == 0xe0) {
			accum = inbuf & 0x0f;
			nexpected = 2;
		} else if((inbuf & 0xe0) == 0xc0) {
			accum = inbuf & 0x1f;
			nexpected = 1;
		} else if(inbuf & 0x80) {
			if(nexpected) {
				accum <<= 6;
				accum |= inbuf & 0x3f;
				if(!--nexpected) {
					state = STATE_BARE;
					if(accum == 0x9b) {
						state = STATE_CSI;
					} else if(accum >= 0xa0) {
						return accum;
					}
				}
			}
		} else if(inbuf == 27) {
			state = STATE_ESC;
		} else if(state == STATE_ESC && inbuf == '[') {
			state = STATE_CSI;
		} else if(state == STATE_CSI) {
			state = STATE_BARE;
			switch(inbuf) {
				case 'A': return TERM_UP;
				case 'B': return TERM_DOWN;
				case 'C': return TERM_RIGHT;
				case 'D': return TERM_LEFT;
			}
		} else {
			return inbuf;
		}
	}
}

static void sighandler(int sig) {
	if(sig == SIGINT) {
		term_int_callback();
	}
}

void term_init(term_int_callback_t callback) {
	term_int_callback = callback;
	if(isatty(0)) {
		if(signal(SIGINT, sighandler) == SIG_ERR) {
			fprintf(stderr, "Failed to install signal handler for ^C.\n");
			exit(1);
		}
	}

}

void term_cleanup() {
	struct histentry *he, *next;

	for(he = tophist; he; he = next) {
		next = he->prev;
		free(he->content);
		free(he);
	}
	tophist = 0;
}

void term_quit() {
	exit(0);
}

char *term_quit_hint() {
	return "You may also press ^D to quit the debugger.";
}

char *term_suspend_hint() {
	return
		"You can press ^C to suspend the program while it is running. "
		"At the the \"suspended>\" prompt, you can run queries and "
		"debugging commands. A blank line of input resumes the program.";
}

int term_handles_wrapping() {
	return 0;
}

static void suspend() {
	tty_restore();
	kill(0, SIGSTOP);
	tty_setup();
}

int term_getkey(const char *prompt) {
	int ch;
	int i;

	fflush(stdout);

	if(!isatty(0)) {
		ch = fgetc(stdin);
		return (ch == EOF)? -1 : ch;
	}

	tty_setup();

	for(;;) {
		ch = getkey();

		if(ch != 26) {
			break;
		}

		if(prompt) {
			charout('\r');
			charout('\n');
			suspend();
			charout('\r');
			charout('\n');
			write(1, prompt, strlen(prompt));
		} else {
			suspend();
		}
	}

	if(prompt) {
		charout('\r');
		for(i = 0; prompt[i]; i++) charout(' ');
		charout('\r');
	}

	tty_restore();

	unread_lines = 0;

	return ch;
}

int term_getline(const char *prompt, uint8_t *buffer, int bufsize, int is_filename) {
	int i, j, ch;
	int retval = -1;
	int xpos = 0, len = 0;
	uint16_t *buf;
	struct histentry *currhist = 0;

	fflush(stdout);

	if(!isatty(0)) {
		if(fgets((char *) buffer, bufsize, stdin)) {
			len = strlen((char *) buffer);
			if(len && buffer[len - 1] == '\n') buffer[--len] = 0;
			if(len && buffer[len - 1] == '\r') buffer[--len] = 0;
			printf("%s\n", (char *) buffer);
			return 1;
		} else {
			printf("\n");
			return 0;
		}
	}

	tty_setup();

	// assume the prompt has already been printed

	buf = malloc(bufsize * sizeof(uint16_t));

	if(is_filename) {
		write(1, prompt, strlen(prompt));

		for(i = 0; i < (bufsize - 1) && last_filename[i]; i++) {
			charout(last_filename[i]);
			buf[len++] = last_filename[i];
			xpos++;
		}
	}

	while(retval < 0) {
		ch = getkey();
		if(ch < 0) {
			retval = 0;
			break;
		}
		switch(ch) {
		case 4:
			retval = !!len;
			break;
		case 8:
		case 127:
			if(xpos) {
				if(xpos != len) {
					memmove(buf + xpos - 1, buf + xpos, (len - xpos) * sizeof(uint16_t));
				}
				xpos--;
				len--;
				stepcursor(-1);
				for(i = xpos; i < len; i++) charout(buf[i]);
				charout(' ');
				stepcursor(xpos - len - 1);
			}
			break;
		case 12:
			charout('\033');
			charout('c');
			charout('\r');
			write(1, prompt, strlen(prompt));
			for(i = 0; i < len; i++) {
				charout(buf[i]);
			}
			stepcursor(xpos - len);
			break;
		case 21: // ^U
			stepcursor(-xpos);
			for(i = 0; i < len; i++) {
				charout(' ');
			}
			stepcursor(-len);
			xpos = 0;
			len = 0;
			break;
		case 26:
			charout('\r');
			charout('\n');
			suspend();
			charout('\r');
			charout('\n');
			write(1, prompt, strlen(prompt));
			for(i = 0; i < len; i++) {
				charout(buf[i]);
			}
			stepcursor(xpos - len);
			break;
		case TERM_RIGHT:
			if(xpos < len) {
				charout(buf[xpos++]);
			}
			break;
		case TERM_LEFT:
			if(xpos) {
				xpos--;
				charout(8);
			}
			break;
		case TERM_UP:
			if(!is_filename) {
				if(currhist) {
					if(currhist->prev) {
						currhist = currhist->prev;
					}
				} else {
					if(addhist(buf, len)) {
						currhist = tophist->prev;
					} else {
						currhist = tophist;
					}
				}
				readhist(currhist, buf, bufsize, &len, &xpos);
			}
			break;
		case TERM_DOWN:
			if(!is_filename && currhist) {
				currhist = currhist->next;
				readhist(currhist, buf, bufsize, &len, &xpos);
			}
			break;
		case 10:
			retval = 1;
			break;
		default:
			if(ch >= 32) {
				if(len < bufsize - 1) {
					if(xpos != len) {
						memmove(buf + xpos + 1, buf + xpos, (len - xpos) * sizeof(uint16_t));
					}
					buf[xpos++] = ch;
					len++;
					for(i = xpos - 1; i < len; i++) charout(buf[i]);
					stepcursor(xpos - len);
				}
			}
		}
	}

	if(retval > 0) {
		if(is_filename) {
			for(i = 0; i < len && i < (sizeof(last_filename) / sizeof(uint16_t)) - 1; i++) {
				last_filename[i] = buf[i];
			}
			if(len) last_filename[i] = 0;
		} else {
			(void) addhist(buf, len);
		}
	}

	charout('\r');
	charout('\n');

	tty_restore();

	j = 0;
	for(i = 0; i < len && j < bufsize - 3 - 1; i++) {
		j += render(buffer + j, buf[i]);
	}
	buffer[j] = 0;

	unread_lines = 0;

	free(buf);
	return retval;
}

int debugger(int, char **);

int main(int argc, char **argv) {
	return debugger(argc, argv);
}
