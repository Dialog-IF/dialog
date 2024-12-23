#include <stdlib.h>
#include <string.h>

#include "glk.h"

#include "common.h"
#include "output.h"
#include "report.h"
#include "fs.h"

#define MAXLINE 256

int fs_writefile(char **lines, int nline, char *description) {
	strid_t s;
	frefid_t fref;
	int i;
	int retval = 0;

	fref = glk_fileref_create_by_prompt(fileusage_InputRecord|fileusage_TextMode, filemode_Write, 0);
	if(!fref) {
		report(LVL_ERR, 0, "Operation cancelled.");
		return 0;
	}

	if((s = glk_stream_open_file(fref, filemode_Write, 0))) {
		for(i = 0; i < nline; i++) {
			glk_put_string_stream(s, lines[i]);
			glk_put_char_stream(s, '\n');
		}
		glk_stream_close(s, 0);
		report(LVL_NOTE, 0, "Successfully wrote %s to file.", description);
		retval = 1;
	} else {
		report(LVL_ERR, 0, "Failed to open file.");
	}

	glk_fileref_destroy(fref);
	return retval;
}

char **fs_readfile(int *nline, char *description) {
	strid_t s;
	frefid_t fref;
	char linebuf[MAXLINE];
	char **result = 0;
	int n = 0, nalloc = 0;

	fref = glk_fileref_create_by_prompt(fileusage_InputRecord|fileusage_TextMode, filemode_Read, 0);
	if(!fref) {
		report(LVL_ERR, 0, "Operation cancelled.");
		return 0;
	}

	if((s = glk_stream_open_file(fref, filemode_Read, 0))) {
		while(glk_get_line_stream(s, linebuf, sizeof(linebuf))) {
			if(*linebuf && linebuf[strlen(linebuf) - 1] == '\n') {
				linebuf[strlen(linebuf) - 1] = 0;
			}
			if(*linebuf && linebuf[strlen(linebuf) - 1] == '\r') {
				linebuf[strlen(linebuf) - 1] = 0;
			}
			if(n >= nalloc) {
				nalloc = 2 * n + 8;
				result = realloc(result, nalloc * sizeof(char *));
			}
			result[n++] = strdup(linebuf);
		}
		glk_stream_close(s, 0);
		report(LVL_NOTE, 0, "Successfully read %s from file.", description);
		if(!n) result = malloc(sizeof(char *)); // allow caller to compare against null
		*nline = n;
	} else {
		report(LVL_ERR, 0, "Failed to open file.");
	}

	glk_fileref_destroy(fref);
	return result;
}
