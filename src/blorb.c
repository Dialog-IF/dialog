#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "arena.h"
#include "ast.h"
#include "blorb.h"
#include "report.h"

static char *strbuf;
static int strsize, strpos;

static void addstr(char *str) {
	int len = strlen(str);

	if(strpos + len > strsize) {
		strsize = (strpos + len + 8) * 2;
		strbuf = realloc(strbuf, strsize);
	}

	memcpy(strbuf + strpos, str, len);
	strpos += len;
}

static void addstr_escape(char *str, int allow_par) {
	int n = 0, i, pend_space;
	char *buf;

	for(i = 0; str[i]; i++) {
		if(str[i] == '&') {
			n += 5;
		} else if(str[i] == '<' || str[i] == '>') {
			n += 4;
		} else if(str[i] == '\n' && str[i + 1] == '\n') {
			i++;
			n += 5;
		} else {
			n++;
		}
	}

	buf = malloc(n + 1);
	n = 0;
	pend_space = 0;
	for(i = 0; str[i]; i++) {
		if(str[i] == '&') {
			if(pend_space) {
				if(n) buf[n++] = ' ';
				pend_space = 0;
			}
			memcpy(buf + n, "&amp;", 5);
			n += 5;
		} else if(str[i] == '<') {
			if(pend_space) {
				if(n) buf[n++] = ' ';
				pend_space = 0;
			}
			memcpy(buf + n, "&lt;", 4);
			n += 4;
		} else if(str[i] == '>') {
			if(pend_space) {
				if(n) buf[n++] = ' ';
				pend_space = 0;
			}
			memcpy(buf + n, "&gt;", 4);
			n += 4;
		} else if(allow_par && str[i] == '\n' && str[i + 1] == '\n') {
			do {
				i++;
			} while(str[i + 1] == '\n');
			memcpy(buf + n, "<br/>", 5);
			n += 5;
			pend_space = 0;
		} else if(str[i] == ' ' || str[i] == '\n') {
			pend_space = 1;
		} else {
			if(pend_space) {
				if(n) buf[n++] = ' ';
				pend_space = 0;
			}
			buf[n++] = str[i];
		}
	}
	buf[n] = 0;
	addstr(buf);
	free(buf);
}

static void build_ifiction(struct program *prg, uint8_t *imgdata, int imgwidth, int imgheight, char *coveralt, int zver) {
	char buf[64];

	addstr("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	addstr("<ifindex version=\"1.0\" xmlns=\"http://babel.ifarchive.org/protocol/iFiction/\">\n");
	addstr("  <story>\n");
	addstr("    <identification>\n");
	addstr("      <ifid>");
	addstr(prg->meta_ifid);
	addstr("</ifid>\n");
	addstr("      <format>zcode</format>\n");
	addstr("    </identification>\n");
	addstr("    <bibliographic>\n");
	addstr("      <title>");
	addstr_escape(prg->meta_title, 0);
	addstr("</title>\n");
	addstr("      <author>");
	addstr_escape(prg->meta_author, 0);
	addstr("</author>\n");
	if(prg->meta_noun) {
		addstr("      <headline>");
		addstr_escape(prg->meta_noun, 0);
		addstr("</headline>\n");
	}
	addstr("      <group>Dialog</group>\n");
	if(prg->meta_blurb) {
		addstr("      <description>");
		addstr_escape(prg->meta_blurb, 1);
		addstr("</description>\n");
	}
	addstr("    </bibliographic>\n");
	if(imgdata) {
		if(imgwidth > 1200 || imgheight > 1200) {
			report(LVL_ERR, 0, "Maximum dimensions for cover image: 1200 x 1200 pixels.");
			exit(1);
		}
		addstr("    <cover>\n");
		addstr("      <format>png</format>\n");
		snprintf(buf, sizeof(buf), "%d", imgheight);
		addstr("      <height>");
		addstr(buf);
		addstr("</height>\n");
		snprintf(buf, sizeof(buf), "%d", imgwidth);
		addstr("      <width>");
		addstr(buf);
		addstr("</width>\n");
		if(coveralt) {
			addstr("      <description>");
			addstr_escape(coveralt, 0);
			addstr("</description>\n");
		}
		addstr("    </cover>\n");
	}
	addstr("    <releases>\n");
	addstr("      <attached>\n");
	addstr("        <release>\n");
	addstr("          <releasedate>");
	addstr(prg->meta_reldate);
	addstr("</releasedate>\n");
	addstr("          <version>");
	snprintf(buf, sizeof(buf), "%d", prg->meta_release);
	addstr(buf);
	addstr("</version>\n");
	addstr("          <compiler>Dialog compiler</compiler>\n");
	addstr("          <compilerversion>" VERSION "</compilerversion>\n");
	addstr("        </release>\n");
	addstr("      </attached>\n");
	addstr("    </releases>\n");
	addstr("    <colophon>\n");
	addstr("      <generator>Dialog compiler</generator>\n");
	addstr("      <generatorversion>" VERSION "</generatorversion>\n");
	addstr("      <originated>");
	addstr(prg->meta_reldate);
	addstr("</originated>\n");
	addstr("    </colophon>\n");
	if(zver) {
		addstr("    <zcode>\n");
		addstr("      <version>");
		snprintf(buf, sizeof(buf), "%d", zver);
		addstr(buf);
		addstr("</version>\n");
		addstr("      <serial>");
		addstr(prg->meta_serial);
		addstr("</serial>\n");
		addstr("      <release>");
		snprintf(buf, sizeof(buf), "%d", prg->meta_release);
		addstr(buf);
		addstr("</release>\n");
		addstr("      <compiler>Dialog compiler version " VERSION "</compiler>\n");
		if(imgdata) {
			addstr("      <coverpicture>1</coverpicture>\n");
		}
		addstr("    </zcode>\n");
	}
	addstr("  </story>\n");
	addstr("</ifindex>\n");
}

static void put32(uint32_t x, FILE *f) {
	fputc((x >> 24) & 0xff, f);
	fputc((x >> 16) & 0xff, f);
	fputc((x >> 8) & 0xff, f);
	fputc((x >> 0) & 0xff, f);
}

uint8_t *loadpng(char *fname, uint32_t *p_size, int *p_width, int *p_height) {
	FILE *f;
	struct stat st;
	uint32_t size;
	uint8_t *buf, magic[8] = {137, 'P', 'N', 'G', 13, 10, 26, 10};

	f = fopen(fname, "rb");
	if(!f) {
		report(LVL_ERR, 0, "Failed to open \"%s\": %s", fname, strerror(errno));
		exit(1);
	}

	if(fstat(fileno(f), &st)) {
		report(LVL_ERR, 0, "Failed to obtain file size for \"%s\": %s", fname, strerror(errno));
		exit(1);
	}
	if(st.st_size > 0x1000000UL) {
		report(LVL_ERR, 0, "PNG file too large: %s", fname);
		exit(1);
	}
	size = (uint32_t) st.st_size;
	buf = malloc(size);
	fread(buf, size, 1, f);
	fclose(f);

	if(size < 8 || memcmp(buf, magic, 8)) {
		report(LVL_ERR, 0, "Not a PNG file: %s", fname);
		exit(1);
	}

	*p_size = size;
	*p_width =
		(buf[0x10] << 24) |
		(buf[0x11] << 16) |
		(buf[0x12] << 8) |
		(buf[0x13] << 0);
	*p_height =
		(buf[0x14] << 24) |
		(buf[0x15] << 16) |
		(buf[0x16] << 8) |
		(buf[0x17] << 0);

	return buf;
}

void emit_blorb(
	char *fname,
	uint8_t *zimage,
	uint32_t zlength,
	struct program *prg,
	int zver,
	char *coverfname,
	char *coveralt)
{
	FILE *f;
	int nres;
	uint32_t imgsize;
	int imgwidth, imgheight;
	uint8_t *imgdata = 0;

	if(coverfname) {
		imgdata = loadpng(coverfname, &imgsize, &imgwidth, &imgheight);
	} else if(coveralt) {
		report(LVL_WARN, 0, "Ignoring alt-text since no cover image filename was given.");
	}

	build_ifiction(prg, imgdata, imgwidth, imgheight, coveralt, zver);

	f = fopen(fname, "wb");
	if(!f) {
		report(LVL_ERR, 0, "Error opening \"%s\" for output: %s", fname, strerror(errno));
		exit(1);
	}

	nres = 1 + !!imgdata;

	fwrite("FORM", 4, 1, f);
	put32(
		4 +
		8 + 4 + 12 * nres +
		8 + ((zlength + 1) & ~1) +
		(imgdata?
			8 + 4 +
			8 + ((imgsize + 1) & ~1) : 0) +
		8 + ((strpos + 1) & ~1), f);
	fwrite("IFRS", 4, 1, f);

	fwrite("RIdx", 4, 1, f);
	put32(4 + 12 * nres, f);
	put32(nres, f);
	fwrite("Exec", 4, 1, f);
	put32(0, f);
	put32(12 + 8 + 4 + 12 * nres, f);
	if(imgdata) {
		fwrite("Pict", 4, 1, f);
		put32(1, f);
		put32(12 + 8 + 4 + 12 * nres + 8 + ((zlength + 1) & ~1) + 8 + 4, f);
	}

	fwrite("ZCOD", 4, 1, f);
	put32(((zlength + 1) & ~1), f);
	fwrite(zimage, zlength, 1, f);
	if(zlength & 1) fputc(0, f);

	if(imgdata) {
		fwrite("Fspc", 4, 1, f);
		put32(4, f);
		put32(1, f);

		fwrite("PNG ", 4, 1, f);
		put32(imgsize, f);
		fwrite(imgdata, imgsize, 1, f);
		if(imgsize & 1) fputc(0, f);
	}

	fwrite("IFmd", 4, 1, f);
	put32(strpos, f);
	fwrite(strbuf, strpos, 1, f);
	if(strpos & 1) fputc(0, f);

	fclose(f);
}
