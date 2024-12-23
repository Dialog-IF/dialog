#include <stdint.h>
#include <string.h>

#include "common.h"
#include "report.h"
#include "unicode.h"

static void utf8_warning(uint8_t *src, int pos) {
	report(LVL_WARN, 0, "Invalid UTF-8 sequence in source code ('%s', at byte %d)", (char *) src, pos);
}

int utf8_to_unicode_n(uint16_t *dest, int ndest, uint8_t *src, int nsrc) {
	uint8_t ch;
	uint16_t uchar;
	int outpos = 0, inpos = 0;
	int nexpected = 0;

	/* Stops on end of input or full output. */
	/* The output is always null-terminated. 'ndest' includes null word. */
	/* Returns number of utf8 bytes consumed. */

	for(;;) {
		if(outpos >= ndest - 1) {
			dest[outpos] = 0;
			return inpos;
		}
		if(inpos >= nsrc) {
			dest[outpos] = 0;
			return inpos;
		}
		ch = src[inpos++];
		if((ch & 0xf0) == 0xe0) {
			if(nexpected) utf8_warning(src, inpos);
			uchar = ch & 0x0f;
			nexpected = 2;
		} else if((ch & 0xe0) == 0xc0) {
			if(nexpected) utf8_warning(src, inpos);
			uchar = ch & 0x1f;
			nexpected = 1;
		} else if(ch & 0x80) {
			if(nexpected) {
				uchar = (uchar << 6) | (ch & 0x3f);
				if(!--nexpected) {
					dest[outpos++] = uchar;
				}
			} else utf8_warning(src, inpos);
		} else {
			if(nexpected) utf8_warning(src, inpos);
			nexpected = 0;
			dest[outpos++] = ch;
		}
	}
}

int utf8_to_unicode(uint16_t *dest, int ndest, uint8_t *src) {
	return utf8_to_unicode_n(dest, ndest, src, strlen((char *) src));
}

int unicode_to_utf8(uint8_t *dest, int ndest, uint16_t *src) {
	int i;
	uint16_t ch;

	for(i = 0; src[i]; i++) {
		ch = src[i];
		if(ch < 0x80) {
			if(ndest - 1 < 1) break;
			*dest++ = ch;
			ndest -= 1;
		} else if(ch < 0x800) {
			if(ndest - 1 < 2) break;
			*dest++ = 0xc0 | (ch >> 6);
			*dest++ = 0x80 | ((ch >> 0) & 0x3f);
			ndest -= 2;
		} else {
			if(ndest - 1 < 3) break;
			*dest++ = 0xe0 | (ch >> 12);
			*dest++ = 0x80 | ((ch >> 6) & 0x3f);
			*dest++ = 0x80 | ((ch >> 0) & 0x3f);
			ndest -= 3;
		}
	}
	*dest = 0;

	return i;
}
