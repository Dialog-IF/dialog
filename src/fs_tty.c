#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "output.h"
#include "report.h"
#include "terminal.h"
#include "fs.h"

#define MAXLINE 256

int fs_writefile(char **lines, int nline, char *description) {
	uint8_t termbuf[MAXLINE];
	int success;
	FILE *fp;
	int i;

	o_end_box();
	o_sync();
	success = term_getline("filename> ", termbuf, MAXLINE, 1);
	o_post_input(1);
	o_begin_box("debugger");
	if(success && *termbuf && (fp = fopen((char *) termbuf, "w"))) {
		for(i = 0; i < nline; i++) {
			fprintf(fp, "%s\n", lines[i]);
		}
		fclose(fp);
		report(LVL_NOTE, 0, "Successfully wrote %s to \"%s\".", description, (char *) termbuf);
		return 1;
	} else {
		if(!(success && !*termbuf)) {
			report(LVL_ERR, 0, "\"%s\": %s", termbuf, strerror(errno));
		}
		return 0;
	}
}

char **fs_readfile(int *nline, char *description) {
	uint8_t termbuf[MAXLINE];
	char linebuf[MAXLINE];
	int success;
	FILE *fp;
	char **result = 0;
	int n = 0, nalloc = 0;

	o_end_box();
	o_sync();
	success = term_getline("filename> ", termbuf, MAXLINE, 1);
	o_post_input(1);
	o_begin_box("debugger");
	if(success && *termbuf && (fp = fopen((char *) termbuf, "r"))) {
		while(fgets(linebuf, sizeof(linebuf), fp)) {
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
		fclose(fp);
		report(LVL_NOTE, 0, "Successfully read %s from \"%s\"\n", description, (char *) termbuf);
		if(!n) result = malloc(sizeof(char *)); // allow caller to compare against null
		*nline = n;
		return result;
	} else {
		if(!(success && !*termbuf)) {
			report(LVL_ERR, 0, "\"%s\": %s", termbuf, strerror(errno));
		}
		return 0;
	}
}
