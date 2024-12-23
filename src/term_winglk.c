#include <assert.h>
#include <stdint.h>
#include <windows.h>
#include "glk.h"
#include "WinGlk.h"

#include "common.h"
#include "unicode.h"
#include "terminal.h"

int InitGlk(unsigned int iVersion);

static winid_t mainwin;
static int argc = 1;
static char **argv;
static term_int_callback_t term_int_callback;
static int termstyle;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	if(InitGlk(0x00000601) == 0) exit(0);
	if(winglk_startup_code(lpCmdLine) != 0) {
		glk_main();
		glk_exit();
	}
	return 0;
}

int winglk_startup_code(const char* cmdline) {
	int i, j;
	char *argbuf;
	
	argbuf = malloc(strlen(cmdline) + 1);
	strcpy(argbuf, cmdline);

	for(i = 0; argbuf[i]; ) {
		while(argbuf[i] == ' ') i++;
		if(argbuf[i]) {
			argc++;
			while(argbuf[i] && argbuf[i] != ' ') i++;
		}
	}

	argv = malloc((argc + 1) * sizeof(char *));
	argv[0] = "dgdebug";
	j = 1;
	for(i = 0; argbuf[i]; ) {
		if(argbuf[i] == ' ') i++;
		if(argbuf[i]) {
			argv[j++] = &argbuf[i];
			while(argbuf[i] && argbuf[i] != ' ') i++;
			if(argbuf[i]) argbuf[i++] = 0;
		}
	}
	if(j != argc) return 0;
	argv[argc] = 0;
#if 0
	printf("cmdline \"%s\"\n", cmdline);
	for(i = 0; i < argc; i++) {
		printf(" %d: \"%s\"\n", i, argv[i]);
	}
#endif
	winglk_app_set_name("Dialog Interactive Debugger");
	winglk_set_about_text("Dialog Interactive Debugger " VERSION " by Linus Akesson");
	winglk_set_menu_name("&Debug");
	return 1;
}

int debugger(int, char **);

void glk_main() {
	(void) debugger(argc, argv);
}

void term_init(term_int_callback_t callback) {
	term_int_callback = callback;

	glk_stylehint_set(wintype_AllTypes, style_User1, stylehint_TextColor, 0x226688);
	glk_stylehint_set(wintype_AllTypes, style_User2, stylehint_TextColor, 0x226688);
	glk_stylehint_set(wintype_AllTypes, style_User1, stylehint_Weight, 0);
	glk_stylehint_set(wintype_AllTypes, style_User2, stylehint_Weight, 1);

	mainwin = glk_window_open(0, 0, 0, wintype_TextBuffer, 0);
	glk_set_window(mainwin);
}

void term_cleanup() {
}

void term_quit() {
	glk_exit();
}

char *term_quit_hint() {
	return "";
}

char *term_suspend_hint() {
	return "";
}

int term_handles_wrapping() {
	return 1;
}

int term_getline(const char *prompt, uint8_t *buffer, int bufsize, int is_filename) {
	uint8_t latin1[253];
	uint16_t chars[254];
	event_t ev;
	int i;

	assert(!is_filename);

	glk_request_line_event(mainwin, (char *) latin1, sizeof(latin1), 0);
	do {
		glk_select(&ev);
	} while(ev.type != evtype_LineInput || ev.win != mainwin);

	for(i = 0; i < ev.val1; i++) {
		chars[i] = latin1[i];
	}
	chars[i] = 0;
	(void) unicode_to_utf8(buffer, bufsize, chars);

	return 1;
}

int term_getkey(const char *prompt) {
	event_t ev;

	for(;;) {
		glk_request_char_event(mainwin);
		do {
			glk_select(&ev);
		} while(ev.type != evtype_CharInput || ev.win != mainwin);

		switch(ev.val1) {
		case keycode_Left:	return TERM_LEFT;
		case keycode_Right:	return TERM_RIGHT;
		case keycode_Up:	return TERM_UP;
		case keycode_Down:	return TERM_DOWN;
		case keycode_Return:	return 13;
		case keycode_Delete:	return 8;
		default:
			if(ev.val1 > 0 && ev.val1 < 256) return ev.val1;
		}
	}
}

void term_get_size(int *width, int *height) {
	*width = 79;
	*height = 0;
}

int term_is_interactive() {
	return 1;
}

void term_ticker() {
	glk_tick();
}

void term_sendbytes(uint8_t *utf8, int nbyte) {
	uint16_t chars[nbyte + 1];
	uint8_t latin1[nbyte + 1];
	int i;

	(void) utf8_to_unicode_n(chars, nbyte + 1, utf8, nbyte);
	for(i = 0; chars[i]; i++) {
		if(chars[i] >= 32 && chars[i] < 0x100) {
			latin1[i] = chars[i];
		} else {
			latin1[i] = '?';
		}
	}
	latin1[i] = 0;
	glk_put_string((char *) latin1);
}

void term_clear(int all) {
	glk_window_clear(mainwin);
}

int term_sendlf() {
	glk_put_string("\n");
	return 0;
}

void term_effectstyle(int style) {
	if(style != termstyle) {
		if(!style) {
			glk_set_style(style_Normal);
		} else if(style & STYLE_ITALIC) {
			glk_set_style(style_Note);
		} else if(style & STYLE_REVERSE) {
			glk_set_style(style_Alert);
		} else if(style & STYLE_FIXED) {
			glk_set_style(style_Preformatted);
		} else if(style & STYLE_INPUT) {
			glk_set_style(style_Input);
		} else if(style & STYLE_DEBUG) {
			if(style & STYLE_BOLD) {
				glk_set_style(style_User2);
			} else {
				glk_set_style(style_User1);
			}
		} else if(style & STYLE_BOLD) {
			glk_set_style(style_Emphasized);
		}
		termstyle = style;
	}
}
