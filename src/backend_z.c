#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "common.h"
#include "arena.h"
#include "ast.h"
#include "frontend.h"
#include "compile.h"
#include "eval.h"
#include "report.h"
#include "zcode.h"
#include "blorb.h"

#define TWEAK_BINSEARCH 8
#define TWEAK_PROPDISPATCH 4

#define NZOBJFLAG 48

struct var {
	struct var	*next;
	struct word	*name;
	int16_t		slot;
	int8_t		used_in_subgoal;
	int8_t		persistent;
	int8_t		used;
	int8_t		occurrences;
	int8_t		remaining_occurrences;
	int8_t		still_in_reg;
};

struct valuelist {
	uint16_t		value;
	struct valuelist	*next;
};

struct backend_clause {
	struct var		*vars;
	int			first_free_temp;
	int			npersistent;
	int			envflags;
	struct valuelist	*removed;
};

struct dictword {
	uint16_t	encoded[3];
	struct word	*word;
	int		n_essential;
};

struct backend_pred {
	int		global_label;
	int		trace_output_label;
	uint16_t	user_global;
	uint16_t	user_flag_mask;
	int		object_flag;
	int		propbase_label;
	int		set_label;
	int		clear_label;
};

struct backend_wobj {
	uint16_t	*encoded_name;
	int		n_encoded;
	uint16_t	addr_objtable;
	uint16_t	addr_proptable;
	uint8_t		npropword[64];
	uint16_t	*propword[64];
	int		initialparent;
};

struct global_string {
	struct global_string	*next;
	uint8_t			*zscii;
	int			nchar;
	uint16_t		global_label;
};

struct indexvalue {
	uint16_t		offset;
	uint16_t		value;
	uint16_t		label;
	struct astnode		*an;
	uint8_t			drop_first;
};

struct memoized_index {
	struct memoized_index	*next;
	struct clause		**clauses;
	struct indexvalue	*values;
	int			nvalue;
	uint16_t		label;
	uint8_t			pending;
};

struct scantable {
	uint16_t		label;
	uint16_t		length;
	uint16_t		*value;
};

uint16_t extended_zscii[69] = {
	// These unicode chars map to zscii characters 155..223 in order.
	0x0e4, 0x0f6, 0x0fc, 0x0c4, 0x0d6, 0x0dc, 0x0df, 0x0bb, 0x0ab, 0x0eb,
	0x0ef, 0x0ff, 0x0cb, 0x0cf, 0x0e1, 0x0e9, 0x0ed, 0x0f3, 0x0fa, 0x0fd,
	0x0c1, 0x0c9, 0x0cd, 0x0d3, 0x0da, 0x0dd, 0x0e0, 0x0e8, 0x0ec, 0x0f2,
	0x0f9, 0x0c0, 0x0c8, 0x0cc, 0x0d2, 0x0d9, 0x0e2, 0x0ea, 0x0ee, 0x0f4,
	0x0fb, 0x0c2, 0x0ca, 0x0ce, 0x0d4, 0x0db, 0x0e5, 0x0c5, 0x0f8, 0x0d8,
	0x0e3, 0x0f1, 0x0f5, 0x0c3, 0x0d1, 0x0d5, 0x0e6, 0x0c6, 0x0e7, 0x0c7,
	0x0fe, 0x0f0, 0x0de, 0x0d0, 0x0a3, 0x153, 0x152, 0x0a1, 0x0bf
};

#define ENVF_ENV		0x010
#define ENVF_CUT_SAVED		0x020
#define ENVF_SIMPLE_SAVED	0x040
#define ENVF_SIMPLEREF_SAVED	0x080
#define ENVF_CAN_BE_MULTI	0x100
#define ENVF_CAN_BE_SIMPLE	0x200
#define ENVF_MAYBE_FOR_WORDS	0x400
#define ENVF_ARITY_MASK		0x00f

#define TAIL_CONT	0xffff

#define NSTOPCHAR 5
#define STOPCHARS ",.\";*"

#define BUCKETS 512

struct routine **routines;
int next_routine_num;

struct routine *routinehash[BUCKETS];

static uint8_t *zcore;

static int ndict;
static struct dictword *dictionary;
static uint16_t *dictmap;	// from intermediate code dict_id - 256 to backend dictionary[] index

static uint16_t *global_labels;
static int next_global_label = G_FIRST_FREE, nalloc_global_label;

static struct global_string *stringhash[BUCKETS];

static int next_temp, max_temp = 0;

static int next_user_global = 0, user_global_base, user_flags_global, next_user_flag = 0;
static int next_flag = 0;
static uint16_t *extflagreaders = 0;

static uint16_t undoflag_global, undoflag_mask;

int next_free_prop = 1;
uint16_t propdefault[64];

static int next_select;

static int tracing_enabled;

struct scantable *scantable;
int nscantable;

static struct backend_wobj *backendwobj;

void get_timestamp(char *dest, char *longdest) {
	time_t t = 0;
	struct tm *tm;

	time(&t);
	tm = localtime(&t);
	snprintf(dest, 8, "%02d%02d%02d", tm->tm_year % 100, tm->tm_mon + 1, tm->tm_mday);
	snprintf(longdest, 11, "%04d-%02d-%02d", 1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday);
}

void set_global_label(uint16_t lab, uint16_t val) {
	assert(lab < nalloc_global_label);
	global_labels[lab] = val;
}

uint16_t make_global_label() {
	uint16_t lab = next_global_label++;
	int nn;

	if(lab >= nalloc_global_label) {
		nn = (lab * 2) + 8;
		global_labels = realloc(global_labels, nn * sizeof(uint16_t));
		memset(global_labels + nalloc_global_label, 0, (nn - nalloc_global_label) * sizeof(uint16_t));
		nalloc_global_label = nn;
	}

	return lab;
}

uint16_t f_make_routine_labels(int n, int line) {
	int i;

	routines = realloc(routines, (next_routine_num + n) * sizeof(struct routine *));
	for(i = 0; i < n; i++) {
		routines[next_routine_num + i] = calloc(1, sizeof(struct routine));
		routines[next_routine_num + i]->aline = line;
	}

	i = next_routine_num;
	next_routine_num += n;

	return i;
}

#define make_routine_label() f_make_routine_labels(1, __LINE__)
#define make_routine_labels(n) f_make_routine_labels(n, __LINE__)

uint32_t hashbytes(uint8_t *data, int n) {
	int i;
	uint32_t h = 0;

	for(i = 0; i < n; i++) {
		if(h & 1) {
			h = (h >> 1) ^ 0x81324bba;
		} else {
			h >>= 1;
		}
		h += data[i];
	}

	return h;
}

uint16_t resolve_rnum(uint16_t num) {
	struct routine *r = routines[num], *r2;
	int i, j;
	uint16_t specified, actual;
	int h;

	if(r->actual_routine == 0xffff) {
		r->actual_routine = 0xfffe;
		for(i = 0; i < r->ninstr; i++) {
			for(j = 0; j < 4; j++) {
				if((r->instr[i].oper[j] & 0xf0000) == ROUTINE(0)) {
					specified = r->instr[i].oper[j] & 0xffff;
					actual = resolve_rnum(specified);
					if(actual == 0xfffe
					|| (r->instr[i].op == Z_CALLVS && j == 0)) {
						routines[specified]->actual_routine = specified;
					} else {
						if(actual == routines[R_FAIL_PRED]->actual_routine) {
							if(r->instr[i].op == Z_RET) {
								assert(j == 0);
								r->instr[i].op = Z_RFALSE;
								r->instr[i].oper[j] = 0;
							} else {
								r->instr[i].oper[j] = SMALL(0);
							}
						} else {
							r->instr[i].oper[j] = ROUTINE(actual);
						}
					}
				}
			}
		}
		if(r->actual_routine == 0xfffe
		&& r->instr[0].op == Z_RET
		&& (r->instr[0].oper[0] & 0xf0000) == ROUTINE(0)) {
			r->actual_routine = r->instr[0].oper[0] & 0xffff;
		} else {
			h = hashbytes((uint8_t *) r->instr, r->ninstr * sizeof(struct zinstr)) & (BUCKETS - 1);
			if(r->actual_routine == 0xfffe) {
				for(r2 = routinehash[h]; r2; r2 = r2->next_in_hash) {
					if(r->ninstr == r2->ninstr
					&& !memcmp(r->instr, r2->instr, r->ninstr * sizeof(struct zinstr))) {
						break;
					}
				}
				if(r2) {
					r->actual_routine = r2->actual_routine;
				} else {
					r->actual_routine = num;
					r->next_in_hash = routinehash[h];
					routinehash[h] = r;
				}
			} else {
				r->next_in_hash = routinehash[h];
				routinehash[h] = r;
			}
		}
	}

	return r->actual_routine;
}

static uint8_t unicode_to_zscii(uint16_t uchar) {
	int i;

	if(uchar < 128) {
		return uchar;
	} else {
		for(i = 0; i < sizeof(extended_zscii) / 2; i++) {
			if(extended_zscii[i] == uchar) break;
		}
		if(i >= sizeof(extended_zscii) / 2) {
			report(LVL_ERR, 0, "Unsupported unicode character U+%04x in dictionary word context.", uchar);
			exit(1);
		} else {
			return 155 + i;
		}
	}
}

static int utf8_to_zscii(uint8_t *dest, int ndest, char *src, uint32_t *special) {
	uint8_t ch;
	uint32_t uchar;
	int outpos = 0, inpos = 0, i;

	/* Stops on end of input, special unicode char, or full output. */
	/* The output is always null-terminated. */
	/* Returns number of utf8 bytes consumed. */

	for(;;) {
		if(outpos >= ndest - 1) {
			dest[outpos] = 0;
			if(special) *special = 0;
			return inpos;
		}
		ch = src[inpos];
		if(!ch) {
			dest[outpos] = 0;
			if(special) *special = 0;
			return inpos;
		}
		inpos++;
		if(ch & 0x80) {
			int nbyte = 0;
			int mask = 0x40;
			while(ch & mask) {
				nbyte++;
				mask >>= 1;
			}
			uchar = ch & (mask - 1);
			while(nbyte--) {
				ch = src[inpos++];
				if((ch & 0xc0) != 0x80) {
					report(LVL_ERR, 0, "Invalid UTF-8 sequence in source code file. ('%s')", src);
					exit(1);
				}
				uchar <<= 6;
				uchar |= ch & 0x3f;
			}
		} else {
			uchar = ch;
		}
		if(uchar < 128) {
			dest[outpos++] = uchar;
		} else {
			for(i = 0; i < sizeof(extended_zscii) / 2; i++) {
				if(extended_zscii[i] == uchar) break;
			}
			if(i >= sizeof(extended_zscii) / 2) {
				dest[outpos] = 0;
				if(special) *special = uchar;
				return inpos;
			} else {
				dest[outpos++] = 155 + i;
			}
			// In the future, we should convert extended characters to lowercase
			// for dictionary words. For now, story authors are expected to include
			// an explicit lowercase alias in the code where necessary.
		}
	}
}

static int encode_chars(uint8_t *dest, int ndest, int *for_dict, uint8_t *src) {
	int n = 0;
	char *str, *a2 = "\r0123456789.,!?_#'\"/\\-:()";
	uint8_t zscii;

	/* Converts from 8-bit zscii to pentets. */

	while((zscii = *src++)) {
		if(n >= ndest) return n;
		if(zscii == ' ') {
			dest[n++] = 0;
		} else if(zscii >= 'a' && zscii <= 'z') {
			dest[n++] = 6 + zscii - 'a';
		} else if(zscii >= 'A' && zscii <= 'Z') {
			if(for_dict) {
				dest[n++] = 6 + zscii - 'A';
			} else {
				dest[n++] = 4;
				if(n >= ndest) return n;
				dest[n++] = 6 + zscii - 'A';
			}
		} else if(zscii < 128 && (str = strchr(a2, (char) zscii))) {
			dest[n++] = 5;
			if(n >= ndest) return n;
			dest[n++] = 7 + (str - a2);
		} else {
			dest[n++] = 5;
			if(n >= ndest) return n;
			dest[n++] = 6;
			if(n >= ndest) return n;
			dest[n++] = (zscii >> 5) & 0x1f;
			if(n >= ndest) return n;
			dest[n++] = zscii & 0x1f;
		}
		if(for_dict) {
			(*for_dict)++;
		}
	}

	return n;
}

int pack_pentets(uint16_t *dest, uint8_t *pentets, int n) {
	int count = 0;

	while(n > 3) {
		*dest++ = (pentets[0] << 10) | (pentets[1] << 5) | pentets[2];
		count++;
		pentets += 3;
		n -= 3;
	}
	if(n == 1) {
		*dest = (pentets[0] << 10) | (5 << 5) | 5 | 0x8000;
		count++;
	} else if(n == 2) {
		*dest = (pentets[0] << 10) | (pentets[1] << 5) | 5 | 0x8000;
		count++;
	} else if(n == 3) {
		*dest = (pentets[0] << 10) | (pentets[1] << 5) | pentets[2] | 0x8000;
		count++;
	} else {
		*dest = 0x94a5; // 1 00101 00101 00101
		count++;
	}

	return count;
}

struct global_string *find_global_string(uint8_t *zscii) {
	int i;
	uint32_t h = 0;
	struct global_string *gs;

	for(i = 0; zscii[i]; i++) {
		if(h & 1) {
			h = (h >> 1) ^ 0x8abcd123;
		} else {
			h >>= 1;
		}
		h += zscii[i];
	}

	h &= BUCKETS - 1;

	for(gs = stringhash[h]; gs; gs = gs->next) {
		if(gs->nchar == i
		&& !memcmp(gs->zscii, zscii, i)) {
			return gs;
		}
	}

	gs = malloc(sizeof(*gs));
	gs->next = stringhash[h];
	stringhash[h] = gs;
	gs->nchar = i;
	gs->zscii = malloc(i + 1);
	memcpy(gs->zscii, zscii, i);
	gs->zscii[i] = 0;
	gs->global_label = make_global_label();

	return gs;
}

int oper_size(uint32_t oper) {
	switch(oper >> 16) {
	case 5:
	case 6:
	case 7:
	case 8:
		return 1;
	case 1:
	case 2:
	case 3:
	case 4:
		return 2;
	default:
		return 0;
	}
}

int typebits(uint32_t oper) {
	switch(oper >> 16) {
	case 0:
		return 3;
	case 5:
	case 8:
		return 1;
	case 6:
	case 7:
		return 2;
	default:
		return 0;
	}
}

int assemble_oper(uint32_t org, uint32_t oper, struct routine *r) {
	uint16_t value;
	int32_t diff;

	switch(oper >> 16) {
	case 5:
		zcore[org] = oper & 0xff;
		return 1;
	case 6:
		value = oper & 0xff;
		if(value >= 1 && value <= 0xf) {
			assert(value <= r->nlocal);
		}
		zcore[org] = value;
		return 1;
	case 7:
	case 8:
		zcore[org] = 0x10 + user_global_base + (oper & 0xff);
		return 1;
	case 1:
		zcore[org + 0] = oper >> 8;
		zcore[org + 1] = oper & 0xff;
		return 2;
	case 2:
		value = 0;
		if((oper & 0xffff) < next_global_label) value = global_labels[oper & 0xffff];
		if(!value) {
			report(LVL_WARN, 0, "Internal inconsistency: Undefined global label %d at %05x", oper & 0xffff, org);
			value = 0xdead;
		}
		zcore[org + 0] = value >> 8;
		zcore[org + 1] = value & 0xff;
		return 2;
	case 3:
		value = oper & 0xffff;
		assert(value < next_routine_num);
		assert(routines[value]->actual_routine == value);
		if(!routines[value]->address
		&& routines[value]->actual_routine != routines[R_FAIL_PRED]->actual_routine) {
			report(LVL_WARN, 0, "Internal inconsistency: Undefined routine number %d", oper & 0xffff);
			value = 0xdead;
		} else {
			value = routines[value]->address;
		}
		zcore[org + 0] = value >> 8;
		zcore[org + 1] = value & 0xff;
		return 2;
	case 4:
		if((oper & 0xffff) >= r->nalloc_lab
		|| r->local_labels[oper & 0xffff] == 0xffffffff) {
			report(LVL_ERR, 0, "Internal inconsistency: Local label %d not found", oper & 0xffff);
			exit(1);
		}
		diff = r->local_labels[oper & 0xffff] - (org + 2) + 2;
		if(diff < -0x8000 || diff > 0x7fff) {
			report(LVL_ERR, 0, "Relative jump offset is too large");
			exit(1);
		}
		value = diff;
		zcore[org + 0] = value >> 8;
		zcore[org + 1] = value & 0xff;
		return 2;
	}

	return 0;
}

void assemble(uint32_t org, struct routine *r) {
	int i, n, pc;
	uint8_t pentets[MAXSTRING];
	uint16_t words[(MAXSTRING + 2) / 3];
	uint16_t op, posflag;
	struct zinstr *zi;

	for(pc = 0; pc < r->ninstr; pc++) {
		zi = &r->instr[pc];
		posflag = (!(zi->op & OP_NOT)) << 7;
		op = zi->op & ~(OP_NOT | OP_FAR);
		if(op & OP_LABEL(0)) {
			// skip
		} else if(op & OP_EXT) {
			zcore[org++] = 0xbe;
			zcore[org++] = op & 0xff;
			zcore[org++] =
				(typebits(zi->oper[0]) << 6) |
				(typebits(zi->oper[1]) << 4) |
				(typebits(zi->oper[2]) << 2) |
				(typebits(zi->oper[3]) << 0);
			for(i = 0; i < 4; i++) {
				org += assemble_oper(org, zi->oper[i], r);
			}
		} else if(op & 0x80) {
			if((op & 0x30) == 0x30) {
				zcore[org++] = op;
				if(op == Z_PRINTLIT) {
					n = encode_chars(pentets, MAXSTRING, 0, (uint8_t *) zi->string);
					assert(n < MAXSTRING);
					n = pack_pentets(words, pentets, n);
					for(i = 0; i < n; i++) {
						zcore[org++] = words[i] >> 8;
						zcore[org++] = words[i] & 0xff;
					}
				}
			} else {
				zcore[org++] = op | (typebits(zi->oper[0]) << 4);
				org += assemble_oper(org, zi->oper[0], r);
			}
		} else {
			if(op < 0x20 && zi->oper[0] >= 0x50000 && zi->oper[1] >= 0x50000 && !zi->oper[2]) {
				zcore[org++] = (op & 0x1f)
					| ((zi->oper[0] >= 0x60000) << 6)
					| ((zi->oper[1] >= 0x60000) << 5);
				org += assemble_oper(org, zi->oper[0], r);
				org += assemble_oper(org, zi->oper[1], r);
			} else {
				zcore[org++] = 0xc0 | (op & 0x3f);
				zcore[org++] =
					(typebits(zi->oper[0]) << 6) |
					(typebits(zi->oper[1]) << 4) |
					(typebits(zi->oper[2]) << 2) |
					(typebits(zi->oper[3]) << 0);
				for(i = 0; i < 4; i++) {
					org += assemble_oper(org, zi->oper[i], r);
				}
			}
		}
		if(zi->store) {
			if(zi->store == REG_PUSH) {
				zcore[org++] = 0;
			} else if(zi->store & DEST_USERGLOBAL(0)) {
				zcore[org++] = 0x10 + user_global_base + (zi->store & 0xff);
			} else {
				zcore[org++] = zi->store;
			}
		}
		if(zi->branch) {
			if(zi->branch == RFALSE) {
				zcore[org++] = posflag | 0x40;
			} else if(zi->branch == RTRUE) {
				zcore[org++] = posflag | 0x41;
			} else {
				if(zi->branch >= r->nalloc_lab
				|| r->local_labels[zi->branch] == 0xffffffff) {
					report(LVL_ERR, 0, "Internal inconsistency: Unknown local label %d", zi->branch);
					exit(1);
				}
				if(zi->op & OP_FAR) {
					int32_t diff = r->local_labels[zi->branch] - (org + 2) + 2;
					if(diff < -0x2000 || diff >= 0x1fff) {
						report(LVL_ERR, 0, "Branch offset too large.");
						exit(1);
					}
					zcore[org++] = posflag | ((diff >> 8) & 0x3f);
					zcore[org++] = diff & 0xff;
				} else {
					uint16_t diff = r->local_labels[zi->branch] - (org + 1) + 2;
					assert(diff >= 2);
					assert(diff < 64);
					zcore[org++] = posflag | 0x40 | diff;
				}
			}
		}
	}
}

int pass1(struct routine *r, uint32_t org) {
	int size, lab;
	struct zinstr *zi;
	int i, n, need_recheck = 1, pc;
	uint8_t pentets[MAXSTRING];
	uint16_t op;

	assert(r->next_label <= 0x1000);

	do {
		size = 1;
		for(pc = 0; pc < r->ninstr; pc++) {
			zi = &r->instr[pc];
			op = zi->op & ~(OP_NOT | OP_FAR);
			if(op & OP_LABEL(0)) {
				lab = op & 0xfff;
				if(lab >= r->nalloc_lab) {
					r->local_labels = realloc(r->local_labels, (lab + 1) * sizeof(uint32_t));
					while(r->nalloc_lab < lab + 1) {
						r->local_labels[r->nalloc_lab++] = 0xffffffff;
					}
				}
				r->local_labels[lab] = org + size;
			} else if(op & OP_EXT) {
				size += 3;
				for(i = 0; i < 4; i++) {
					size += oper_size(zi->oper[i]);
				}
			} else if(op & 0x80) {
				if((op & 0x30) == 0x30) {
					size++;
					if(op == Z_PRINTLIT) {
						n = encode_chars(pentets, MAXSTRING, 0, (uint8_t *) zi->string);
						assert(n <= MAXSTRING);
						size += ((n + 2) / 3) * 2;
					}
				} else {
					size++;
					size += oper_size(zi->oper[0]);
				}
			} else {
				if(op < 0x20 && zi->oper[0] >= 0x50000 && zi->oper[1] >= 0x50000 && !zi->oper[2]) {
					size += 3;
				} else {
					size += 2;
					for(i = 0; i < 4; i++) {
						size += oper_size(zi->oper[i]);
					}
				}
			}
			if(zi->store) size++;
			if(zi->branch) {
				if(zi->branch == RFALSE || zi->branch == RTRUE) {
					size += 1;
				} else if(zi->branch >= r->nalloc_lab || r->local_labels[zi->branch] == 0xffffffff) {
					if(zi->op & OP_FAR) {
						size += 2;
					} else {
						size += 1;
					}
					assert(need_recheck);
				} else {
					if(zi->op & OP_FAR) {
						int32_t diff = r->local_labels[zi->branch] - (org + size + 2) + 2;
						if(diff < -0x2000 || diff > 0x1fff) {
							if(r->ninstr + 2 < r->nalloc_instr) {
								r->nalloc_instr = r->ninstr + 16;
								r->instr = realloc(r->instr, r->nalloc_instr * sizeof(struct zinstr));
							}
							r->ninstr += 2;
							memmove(r->instr + pc + 3, r->instr + pc + 1, (r->ninstr - pc - 3) * sizeof(struct zinstr));
							memset(&r->instr[pc + 1], 0, 2 * sizeof(*zi));
							r->instr[pc + 1].op = Z_JUMP;
							r->instr[pc + 1].oper[0] = REL_LABEL(r->instr[pc + 0].branch);
							r->instr[pc + 2].op = OP_LABEL(r->next_label);
							r->instr[pc + 0].branch = r->next_label++;
							r->instr[pc + 0].op ^= OP_NOT;
							r->instr[pc + 0].op &= ~OP_FAR;
							need_recheck = 2;
							r->nalloc_lab = 0;
							break;
						} else {
							size += 2;
						}
					} else {
						int32_t diff = r->local_labels[zi->branch] - (org + size + 1) + 2;
						if(diff < 2 || diff > 63) {
							zi->op |= OP_FAR;
							size += 2;
							for(i = 0; i < r->nalloc_lab; i++) {
								if(r->local_labels[i] != 0xffffffff
								&& r->local_labels[i] >= org + size) {
									r->local_labels[i]++;
								}
							}
							need_recheck = 2;
						} else {
							size += 1;
						}
					}
				}
			}
		}
	} while(need_recheck--);

	return size;
}

int cmp_dictword(const void *a, const void *b) {
	const struct dictword *aa = (const struct dictword *) a;
	const struct dictword *bb = (const struct dictword *) b;
	int i;

	for(i = 0; i < 3; i++) {
		if(aa->encoded[i] < bb->encoded[i]) return -1;
		if(aa->encoded[i] > bb->encoded[i]) return 1;
	}

	return 0;
}

static void prepare_dictionary(struct program *prg) {
	int i, n;
	uint8_t pentets[9];
	uint8_t zbuf[MAXSTRING];
	uint32_t uchar;

	dictmap = malloc(prg->ndictword * sizeof(*dictmap));
	memset(dictmap, 0xff, prg->ndictword * sizeof(*dictmap));

	ndict = prg->ndictword;
	dictionary = calloc(ndict, sizeof(*dictionary));

	for(i = 0; i < ndict; i++) {
		dictionary[i].word = prg->dictwordnames[i];
		n = utf8_to_zscii(zbuf, sizeof(zbuf), prg->dictwordnames[i]->name, &uchar);
		if(uchar) {
			report(
				LVL_ERR,
				0,
				"Unsupported character U+%04x in explicit dictionary word '@%s'.",
				uchar,
				prg->dictwordnames[i]->name);
			exit(1);
		}
		n = encode_chars(pentets, 9, &dictionary[i].n_essential, zbuf);
		memset(pentets + n, 5, 9 - n);
		pack_pentets(dictionary[i].encoded, pentets, 9);
	}

	qsort(dictionary, ndict, sizeof(struct dictword), cmp_dictword);

	for(i = 0; i < ndict; ) {
		assert(dictionary[i].word->name[0]);
		assert(dictionary[i].word->name[1]);
		assert(dictionary[i].word->dict_id >= 256);
		dictmap[dictionary[i].word->dict_id - 256] = i;
		if(i < ndict - 1 && !memcmp(dictionary[i].encoded, dictionary[i + 1].encoded, 6)) {
			report(LVL_INFO, 0, "Consolidating dictionary words \"%s\" and \"%s\".",
				dictionary[i].word->name,
				dictionary[i + 1].word->name);
			assert(dictionary[i + 1].word->dict_id >= 256);
			dictmap[dictionary[i + 1].word->dict_id - 256] = i;
			memmove(dictionary + i, dictionary + i + 1, (ndict - i - 1) * sizeof(struct dictword));
			ndict--;
		} else {
			i++;
		}
	}
}

void init_backend_wobj(struct program *prg, int id, struct backend_wobj *wobj, int strip) {
	uint8_t pentets[256];
	int n;
	uint8_t zbuf[MAXSTRING];
	uint32_t uchar;

	if(strip) {
		pentets[0] = 5;
		n = 1;
	} else {
		n = utf8_to_zscii(zbuf, sizeof(zbuf), prg->worldobjnames[id]->name, &uchar);
		if(uchar) {
			report(
				LVL_ERR,
				0,
				"Unsupported character U+%04x in object name '#%s'\n",
				uchar,
				prg->worldobjnames[id]->name);
			exit(1);
		}
		n = encode_chars(pentets, sizeof(pentets), 0, zbuf);
		if(n == sizeof(pentets)) {
			report(
				LVL_ERR,
				0,
				"Object name too long: '#%s'",
				prg->worldobjnames[id]->name);
			exit(1);
		}
	}
	wobj->n_encoded = (n + 2) / 3;
	wobj->encoded_name = malloc(wobj->n_encoded * 2);
	pack_pentets(wobj->encoded_name, pentets, n);
}

struct backend_pred *init_backend_pred(struct predicate *pred) {
	int i;
	struct clause *cl;

	if(!pred->backend) {
		pred->backend = calloc(1, sizeof(struct backend_pred));
		for(i = 0; i < pred->nclause; i++) {
			cl = pred->clauses[i];
			cl->backend = calloc(1, sizeof(struct backend_clause));
		}
	}
	return pred->backend;
}

void init_abbrev(uint16_t addr_abbrevstr, uint16_t addr_abbrevtable) {
	uint8_t pentets[3];
	uint16_t words[1];
	int i;

	encode_chars(pentets, sizeof(pentets), 0, (uint8_t *) "foo");
	pack_pentets(words, pentets, 3);
	zcore[addr_abbrevstr + 0] = words[0] >> 8;
	zcore[addr_abbrevstr + 1] = words[0] & 0xff;

	for(i = 0; i < 96; i++) {
		zcore[addr_abbrevtable + i * 2 + 0] = addr_abbrevstr >> 9;
		zcore[addr_abbrevtable + i * 2 + 1] = (addr_abbrevstr >> 1) & 0xff;
	}
}

struct routine *make_routine(uint16_t lab, int nlocal) {
	struct routine *r;

	assert(lab < next_routine_num);
	r = routines[lab];
	assert(!r->ninstr);
	r->nlocal = nlocal;
	r->next_label = 2;
	r->actual_routine = 0xffff;
	return r;
}

struct zinstr *append_instr(struct routine *r, uint16_t op) {
	struct zinstr *zi;

	if(r->ninstr >= r->nalloc_instr) {
		r->nalloc_instr = (r->ninstr * 2) + 8;
		r->instr = realloc(r->instr, r->nalloc_instr * sizeof(struct zinstr));
	}
	zi = &r->instr[r->ninstr++];
	memset(zi, 0, sizeof(*zi));
	zi->op = op;

	return zi;
}

// A simple builtin can be inlined, and preserves args, temps and cut. It may fail.

struct astnode *strip_trivial_conditions(struct astnode *an);
int only_simple_builtins(struct astnode *);

int is_simple_builtin(struct astnode *an, int negated) {
	// negated is only set when called from compile_neg

	if(an->kind == AN_RULE
	|| an->kind == AN_NEG_RULE) {
		if(an->kind == AN_NEG_RULE) negated ^= 1;

		if(!negated
		&& an->predicate->builtin
		&& !((struct backend_pred *) an->predicate->pred->backend)->global_label) {
			return 1;
		}

		if(an->predicate->builtin == BI_OBJECT
		&& !an->children[0]->unbound) {
			return 1;
		}

		if(an->predicate->pred->dynamic) {
			if(an->predicate->pred->flags & PREDF_GLOBAL_VAR) {
				if(an->children[0]->kind == AN_VARIABLE
				&& !an->children[0]->word->name[0]) {
					return 1;
				}
				return !negated;
			} else if(an->predicate->builtin == BI_HASPARENT) {
				if(!an->children[0]->unbound) {
					return !negated;
				}
			} else if(an->predicate->arity == 2) {
				if(!an->children[0]->unbound) {
					if(an->children[1]->kind == AN_VARIABLE
					&& !an->children[1]->word->name[0]) {
						return 1;
					}
					return !negated;
				}
			} else if(an->predicate->arity == 1) {
				if(!an->children[0]->unbound) {
					return 1;
				}
			} else {
				return 1;
			}
		}

		if(an->predicate->pred->flags & PREDF_FAIL) {
			return 1;
		}

		return 0;
	}
	
	if(an->kind == AN_BLOCK
	|| an->kind == AN_FIRSTRESULT) {
		return only_simple_builtins(an->children[0]);
	}

	if(an->kind == AN_IF) {
		return
			!strip_trivial_conditions(an->children[0]) &&
			only_simple_builtins(an->children[1]) &&
			only_simple_builtins(an->children[2]);
	}

	if(an->kind == AN_NEG_BLOCK
	|| an->kind == AN_NEG_RULE
	|| an->kind == AN_OR
	|| an->kind == AN_EXHAUST
	|| an->kind == AN_COLLECT
	|| an->kind == AN_COLLECT_WORDS
	|| an->kind == AN_DETERMINE_OBJECT
	|| an->kind == AN_SELECT
	|| an->kind == AN_STOPPABLE
	|| an->kind == AN_STATUSBAR) {
		return 0;
	}

	return 1;
}

uint16_t dict_id_tag(uint16_t dict_id) {
	if(dict_id < 256) {
		return 0x3e00 | dict_id;
	} else {
		assert(dictmap[dict_id - 256] < ndict);
		return 0x2000 | dictmap[dict_id - 256];
	}
}

uint16_t dictword_tag(struct word *w) {
	assert(w->flags & WORDF_DICT);
	return dict_id_tag(w->dict_id);
}

void compile_put_ast_in_reg(struct routine *r, struct astnode *an, int target_reg, struct var *vars);

uint8_t *stash_lingering(struct var *vars) {
	int i = 0, n = 0;
	struct var *v;
	uint8_t *buf;

	for(v = vars; v; v = v->next) {
		n++;
	}
	buf = malloc(n);
	for(v = vars; v; v = v->next) {
		buf[i++] = v->still_in_reg;
	}

	return buf;
}

void reapply_lingering(struct var *vars, uint8_t *stash) {
	int i = 0;
	struct var *v;

	for(v = vars; v; v = v->next) {
		v->still_in_reg = stash[i++];
	}
}

void clear_lingering(struct var *vars) {
	while(vars) {
		vars->still_in_reg = 0;
		vars = vars->next;
	}
}

static uint32_t compile_dictword(struct routine *r, struct word *w) {
	int n, i, t, first;
	uint8_t zbuf[MAXSTRING];
	uint32_t uchar;
	struct zinstr *zi;

	n = utf8_to_zscii(zbuf, sizeof(zbuf), w->name, &uchar);
	assert(!w->name[n]);
	assert(!uchar);
	int zdict = dictmap[w->dict_id - 256];
	if(strlen((char *) zbuf) != dictionary[zdict].n_essential) {
		t = next_temp++;
		first = 1;
		for(i = strlen((char *) zbuf) - 1; i >= dictionary[zdict].n_essential; i--) {
			zi = append_instr(r, Z_CALLVS);
			zi->oper[0] = ROUTINE(R_PUSH_PAIR_VV);
			zi->oper[1] = LARGE(0x3e00 | zbuf[i]);
			zi->oper[2] = first? VALUE(REG_NIL) : VALUE(REG_X + t);
			zi->store = REG_X + t;
			first = 0;
		}
		zi = append_instr(r, Z_CALLVS);
		zi->oper[0] = ROUTINE(R_PUSH_PAIR_VV);
		zi->oper[1] = LARGE(dictword_tag(w));
		zi->oper[2] = VALUE(REG_X + t);
		zi->store = REG_X + t;
		zi = append_instr(r, Z_OR);
		zi->oper[0] = VALUE(REG_X + t);
		zi->oper[1] = VALUE(REG_E000);
		zi->store = REG_X + t;
#if 0
		printf("creating extended: ");
		for(i = 0; zbuf[i]; i++) {
			if(i == dictionary[zdict].n_essential) {
				printf("+");
			}
			if(zbuf[i] < 128) {
				fputc(zbuf[i], stdout);
			} else {
				printf("\\%02x", zbuf[i]);
			}
		}
		printf("\n");
#endif
		return VALUE(REG_X + t);
	} else {
		return LARGE(dictword_tag(w));
	}
}

uint32_t compile_ast_to_oper(struct routine *r, struct astnode *an, struct var *vars, int extdict) {
	struct zinstr *zi;
	struct var *v;
	int t, t1;

	switch(an->kind) {
	case AN_VARIABLE:
		assert(r);
		for(v = vars; v; v = v->next) {
			if(v->name == an->word) break;
		}
		if(!v || !v->used) {
			if(v) {
				v->used = 1;
				if(v->persistent) {
					t1 = next_temp++;
					zi = append_instr(r, Z_CALL2S);
					zi->oper[0] = ROUTINE(R_PUSH_VAR_SETENV);
					zi->oper[1] = SMALL(2 + v->slot);
					zi->store = REG_X + t1;
					v->still_in_reg = REG_X + t1;
					return VALUE(REG_X + t1);
				} else {
					zi = append_instr(r, Z_CALL1S);
					zi->oper[0] = ROUTINE(R_PUSH_VAR);
					zi->store = REG_X + v->slot;
					return VALUE(REG_X + v->slot);
				}
			} else {
				t1 = next_temp++;
				zi = append_instr(r, Z_CALL1S);
				zi->oper[0] = ROUTINE(R_PUSH_VAR);
				zi->store = REG_X + t1;
				return VALUE(REG_X + t1);
			}
		} else {
			if(v->persistent) {
				if(v->still_in_reg) {
					return VALUE(v->still_in_reg);
				} else {
					t1 = next_temp++;
					zi = append_instr(r, Z_LOADW);
					zi->oper[0] = VALUE(REG_ENV);
					zi->oper[1] = SMALL(2 + v->slot);
					zi->store = REG_X + t1;
					v->still_in_reg = REG_X + t1;
					return VALUE(REG_X + t1);
				}
			} else {
				return VALUE(REG_X + v->slot);
			}
		}
		break;
	case AN_PAIR:
		assert(r);
		t = next_temp++;
		compile_put_ast_in_reg(r, an, REG_X + t, vars);
		return VALUE(REG_X + t);
	case AN_TAG:
		return SMALL_OR_LARGE(1 + an->word->obj_id);
	case AN_DICTWORD:
	case AN_BAREWORD:
		assert(an->word->flags & WORDF_DICT);
		if(extdict && an->word->dict_id >= 256) {
			return compile_dictword(r, an->word);
		} else {
			return LARGE(dictword_tag(an->word));
		}
	case AN_INTEGER:
		return LARGE(0x4000 + an->value);
	case AN_EMPTY_LIST:
		return VALUE(REG_NIL);
	default:
		assert(0);
		exit(1);
	}
}

int is_list_without_vars_or_pairs(struct astnode *an) {
	int n = 0;

	for(;;) {
		if(!an) return 0;
		if(an->kind == AN_EMPTY_LIST) return n;
		if(an->kind != AN_PAIR) return 0;
		if(an->children[0]->kind == AN_VARIABLE) return 0;
		if(an->children[0]->kind == AN_PAIR) return 0;
		an = an->children[1];
		n++;
	}
}

int is_list_without_unbound(struct astnode *an) {
	int n = 0;

	for(;;) {
		if(!an) return 0;
		if(an->kind == AN_EMPTY_LIST) return n;
		if(an->kind != AN_PAIR) return 0;
		if(an->children[0]->unbound) return 0;
		an = an->children[1];
		n++;
	}
}

void compile_put_ast_in_reg(struct routine *r, struct astnode *an, int target_reg, struct var *vars) {
	struct zinstr *zi;
	struct var *v;
	int t, i, is_ref[2], temp[2], n;
	uint32_t oper[3];
	struct astnode *sub;

	switch(an->kind) {
	case AN_VARIABLE:
		for(v = vars; v; v = v->next) {
			if(v->name == an->word) break;
		}
		if(!v) {
			zi = append_instr(r, Z_CALL1S);
			zi->oper[0] = ROUTINE(R_PUSH_VAR);
			zi->store = target_reg;
		} else if(!v->used) {
			if(v->persistent) {
				zi = append_instr(r, Z_CALL2S);
				zi->oper[0] = ROUTINE(R_PUSH_VAR_SETENV);
				zi->oper[1] = SMALL(2 + v->slot);
				zi->store = target_reg;
				v->still_in_reg = target_reg;
			} else {
				zi = append_instr(r, Z_CALL1S);
				zi->oper[0] = ROUTINE(R_PUSH_VAR);
				zi->store = target_reg;
				zi = append_instr(r, Z_STORE);
				zi->oper[0] = SMALL(REG_X + v->slot);
				zi->oper[1] = VALUE(target_reg);
			}
			v->used = 1;
		} else {
			if(v->persistent) {
				if(v->still_in_reg) {
					zi = append_instr(r, Z_STORE);
					zi->oper[0] = SMALL(target_reg);
					zi->oper[1] = VALUE(v->still_in_reg);
				} else {
					zi = append_instr(r, Z_LOADW);
					zi->oper[0] = VALUE(REG_ENV);
					zi->oper[1] = SMALL(2 + v->slot);
					zi->store = target_reg;
				}
			} else {
				zi = append_instr(r, Z_STORE);
				zi->oper[0] = SMALL(target_reg);
				zi->oper[1] = VALUE(REG_X + v->slot);
			}
		}
		break;
	case AN_PAIR:
		if((n = is_list_without_unbound(an)) && n >= 2 && n <= 3) {
			for(i = 0, sub = an; i < n; i++) {
				oper[i] = compile_ast_to_oper(r, sub->children[0], vars, 1);
				sub = sub->children[1];
			}
			if(n == 2) {
				zi = append_instr(r, Z_CALLVS);
				zi->oper[0] = ROUTINE(R_PUSH_LIST_VV);
				zi->oper[1] = oper[0];
				zi->oper[2] = oper[1];
				zi->store = target_reg;
			} else if(n == 3) {
				zi = append_instr(r, Z_CALLVS);
				zi->oper[0] = ROUTINE(R_PUSH_LIST_VVV);
				zi->oper[1] = oper[0];
				zi->oper[2] = oper[1];
				zi->oper[3] = oper[2];
				zi->store = target_reg;
			} else {
				assert(0);
				exit(1);
			}
		} else {
			for(i = 0; i < 2; i++) {
				if(an->children[i]->kind != AN_VARIABLE) {
					oper[i] = compile_ast_to_oper(r, an->children[i], vars, 1);
					is_ref[i] = 0;
				}
			}
			for(i = 0; i < 2; i++) {
				if(an->children[i]->kind == AN_VARIABLE) {
					for(v = vars; v; v = v->next) {
						if(v->name == an->children[i]->word) break;
					}
					if(v) {
						if(v->used) {
							if(v->persistent) {
								if(v->still_in_reg) {
									oper[i] = VALUE(v->still_in_reg);
								} else {
									t = next_temp++;
									oper[i] = VALUE(REG_X + t);
									zi = append_instr(r, Z_LOADW);
									zi->oper[0] = VALUE(REG_ENV);
									zi->oper[1] = SMALL(2 + v->slot);
									zi->store = REG_X + t;
									v->still_in_reg = REG_X + t;
								}
								is_ref[i] = 0;
							} else {
								oper[i] = VALUE(REG_X + v->slot);
								is_ref[i] = 0;
							}
						} else {
							if(v->persistent) {
								temp[i] = next_temp++;
								oper[i] = SMALL(REG_X + temp[i]);
								is_ref[i] = 1;
							} else {
								oper[i] = SMALL(REG_X + v->slot);
								is_ref[i] = 1;
							}
						}
					} else {
						oper[i] = SMALL(REG_TEMP);
						is_ref[i] = 1;
					}
				}
			}
			zi = append_instr(r, Z_CALLVS);
			zi->oper[0] = ROUTINE(R_PUSH_PAIR_VV + is_ref[0] * 2 + is_ref[1]);
			zi->oper[1] = oper[0];
			zi->oper[2] = oper[1];
			zi->store = target_reg;
			for(i = 0; i < 2; i++) {
				if(an->children[i]->kind == AN_VARIABLE) {
					for(v = vars; v; v = v->next) {
						if(v->name == an->children[i]->word) break;
					}
					if(v && !v->used) {
						if(v->persistent) {
							assert(is_ref[i]);
							zi = append_instr(r, Z_STOREW);
							zi->oper[0] = VALUE(REG_ENV);
							zi->oper[1] = SMALL(2 + v->slot);
							zi->oper[2] = VALUE(REG_X + temp[i]);
							v->still_in_reg = REG_X + temp[i];
						}
						v->used = 1;
					}
				}
			}
		}
		break;
	default:
		oper[0] = compile_ast_to_oper(r, an, vars, 1);
		zi = append_instr(r, Z_STORE);
		zi->oper[0] = SMALL(target_reg);
		zi->oper[1] = oper[0];
	}
}

void compile_param(struct routine *r, struct astnode *an, int arg_reg, struct var *vars, int unbound) {
	struct var *v;
	struct zinstr *zi;
	int i;
	int temp[2], is_ref[2];
	uint32_t oper[2], o1;

	switch(an->kind) {
	case AN_VARIABLE:
		for(v = vars; v; v = v->next) {
			if(v->name == an->word) break;
		}
		if(v) {
			if(v->remaining_occurrences >= 2) {
				if(!v->used) {
					if(v->persistent) {
						zi = append_instr(r, Z_STOREW);
						zi->oper[0] = VALUE(REG_ENV);
						zi->oper[1] = SMALL(2 + v->slot);
						zi->oper[2] = VALUE(arg_reg);
					} else {
						zi = append_instr(r, Z_STORE);
						zi->oper[0] = SMALL(REG_X + v->slot);
						zi->oper[1] = VALUE(arg_reg);
					}
					v->used = 1;
				} else {
					o1 = compile_ast_to_oper(r, an, vars, 1);
					zi = append_instr(r, Z_CALLVN);
					zi->oper[0] = ROUTINE(R_UNIFY);
					zi->oper[1] = o1;
					zi->oper[2] = VALUE(arg_reg);
				}
			}
		}
		break;
	case AN_PAIR:
		for(i = 0; i < 2; i++) {
			switch(an->children[i]->kind) {
			case AN_PAIR:
				temp[i] = next_temp++;
				oper[i] = SMALL(REG_X + temp[i]);
				is_ref[i] = 1;
				break;
			case AN_VARIABLE:
				for(v = vars; v; v = v->next) {
					if(v->name == an->children[i]->word) break;
				}
				if(v) {
					if(v->used) {
						oper[i] = compile_ast_to_oper(r, an->children[i], vars, 1);
						is_ref[i] = 0;
					} else {
						if(v->persistent) {
							temp[i] = next_temp++;
							oper[i] = SMALL(REG_X + temp[i]);
							is_ref[i] = 1;
						} else {
							oper[i] = SMALL(REG_X + v->slot);
							is_ref[i] = 1;
						}
					}
				} else {
					oper[i] = SMALL(REG_TEMP);
					is_ref[i] = 1;
				}
				break;
			default:
				oper[i] = compile_ast_to_oper(r, an->children[i], 0, 1);
				is_ref[i] = 0;
			}
		}
		if(!is_ref[1] && oper[1] == VALUE(REG_NIL)) {
			zi = append_instr(r, Z_CALLVN);
			zi->oper[0] = ROUTINE(R_GET_LIST_V + is_ref[0]);
			zi->oper[1] = VALUE(arg_reg);
			zi->oper[2] = oper[0];
		} else {
			zi = append_instr(r, Z_CALLVN);
			zi->oper[0] = ROUTINE(R_GET_PAIR_VV + is_ref[0] * 2 + is_ref[1]);
			zi->oper[1] = VALUE(arg_reg);
			zi->oper[2] = oper[0];
			zi->oper[3] = oper[1];
		}
		for(i = 0; i < 2; i++) {
			if(an->children[i]->kind == AN_VARIABLE) {
				for(v = vars; v; v = v->next) {
					if(v->name == an->children[i]->word) break;
				}
				if(v && !v->used) {
					if(v->persistent) {
						assert(is_ref[i]);
						zi = append_instr(r, Z_STOREW);
						zi->oper[0] = VALUE(REG_ENV);
						zi->oper[1] = SMALL(2 + v->slot);
						zi->oper[2] = VALUE(REG_X + temp[i]);
						v->still_in_reg = REG_X + temp[i];
					}
					v->used = 1;
				}
			}
		}
		for(i = 0; i < 2; i++) {
			if(an->children[i]->kind == AN_PAIR) {
				compile_param(r, an->children[i], REG_X + temp[i], vars, unbound);
			}
		}
		break;
	default:
		o1 = compile_ast_to_oper(r, an, vars, 1);
		zi = append_instr(r, Z_CALLVN);
		zi->oper[0] = VALUE(REG_R_USIMPLE);
		zi->oper[1] = o1;
		zi->oper[2] = VALUE(arg_reg);
	}
}

void call_output_routine(struct routine *r, struct astnode *an, struct var *vars) {
	struct zinstr *zi;
	uint32_t o1;

	switch(an->predicate->builtin) {
	case BI_NOSPACE:
		zi = append_instr(r, Z_CALL1N);
		zi->oper[0] = ROUTINE(R_NOSPACE);
		break;
	case BI_SPACE:
		zi = append_instr(r, Z_CALL1N);
		zi->oper[0] = ROUTINE(R_SPACE);
		break;
	case BI_SPACE_N:
		o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
		zi = append_instr(r, Z_CALL2N);
		zi->oper[0] = ROUTINE(R_SPACE_N);
		zi->oper[1] = o1;
		break;
	case BI_LINE:
		zi = append_instr(r, Z_CALL1N);
		zi->oper[0] = ROUTINE(R_LINE);
		break;
	case BI_PAR:
		zi = append_instr(r, Z_CALL1N);
		zi->oper[0] = ROUTINE(R_PAR);
		break;
	case BI_PAR_N:
		o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
		zi = append_instr(r, Z_CALL2N);
		zi->oper[0] = ROUTINE(R_PAR_N);
		zi->oper[1] = o1;
		break;
	default:
		assert(0);
		exit(1);
	}
}

static void add_to_buf(char **buf, int *nalloc, int *pos, char ch) {
	if(*pos >= *nalloc) {
		*nalloc = (*pos) * 2 + 32;
		*buf = realloc(*buf, *nalloc);
	}
	(*buf)[(*pos)++] = ch;
}

struct astnode *decode_output(char **bufptr, struct astnode *an, int *p_space, int *p_nl) {
	int post_space = 0, nl_printed = 0;
	int i;
	char last = 0;
	char *buf = 0;
	int nalloc = 0, pos = 0;

	while(an) {
		if(an->kind == AN_BAREWORD) {
			if(post_space && !strchr(".,:;!?)]}>-%/", an->word->name[0])) {
				add_to_buf(&buf, &nalloc, &pos, ' ');
				nl_printed = 0;
			}
			for(i = 0; an->word->name[i]; i++) {
				last = an->word->name[i];
				add_to_buf(&buf, &nalloc, &pos, last);
				nl_printed = 0;
			}
			post_space = !strchr("([{<-/", last);
		} else if(an->kind == AN_RULE && an->predicate->builtin == BI_NOSPACE) {
			post_space = 0;
		} else if(an->kind == AN_RULE && an->predicate->builtin == BI_SPACE) {
			add_to_buf(&buf, &nalloc, &pos, ' ');
			post_space = 0;
		} else if(an->kind == AN_RULE
		&& an->predicate->builtin == BI_SPACE_N
		&& an->children[0]->kind == AN_INTEGER
		&& an->children[0]->value < 22) {
			for(i = 0; i < an->children[0]->value; i++) {
				add_to_buf(&buf, &nalloc, &pos, ' ');
				nl_printed = 0;
			}
			post_space = 0;
		} else if(an->kind == AN_RULE && an->predicate->builtin == BI_LINE) {
			while(nl_printed < 1) {
				add_to_buf(&buf, &nalloc, &pos, '\r');
				nl_printed++;
				post_space = 0;
			}
		} else if(an->kind == AN_RULE && an->predicate->builtin == BI_PAR) {
			while(nl_printed < 2) {
				add_to_buf(&buf, &nalloc, &pos, '\r');
				nl_printed++;
				post_space = 0;
			}
		} else {
			break;
		}
		an = an->next_in_body;
	}

	add_to_buf(&buf, &nalloc, &pos, 0);
	*bufptr = buf;
	if(p_space) *p_space = post_space;
	if(p_nl) *p_nl = nl_printed;
	return an;
}
 
struct astnode *compile_output(struct routine *r, struct astnode *an) {
	char *utf8;
	uint8_t zbuf[MAXSTRING];
	int pos;
	int pre_space;
	int post_space;
	int n;
	uint16_t stringlabel;
	uint32_t variant;
	struct zinstr *zi;
	int nl_printed = 0;
	uint32_t uchar;

	pre_space = !strchr(".,:;!?)]}>-%/", an->word->name[0]);

	an = decode_output(&utf8, an, &post_space, &nl_printed);

	for(pos = 0; utf8[pos]; pos += n) {
		uchar = 0;
		n = utf8_to_zscii(zbuf, sizeof(zbuf), utf8 + pos, &uchar);
		stringlabel = find_global_string(zbuf)->global_label;

		if(uchar) {
			if(n) {
				zi = append_instr(r, Z_CALL2N);
				zi->oper[0] = ROUTINE(pre_space? R_SPACE_PRINT_NOSPACE : R_NOSPACE_PRINT_NOSPACE);
				zi->oper[1] = REF(stringlabel);
			} else if(pre_space) {
				zi = append_instr(r, Z_CALL1N);
				zi->oper[0] = ROUTINE(R_SYNC_SPACE);
			}

			zi = append_instr(r, Z_CALL2N);
			zi->oper[0] = ROUTINE(R_UNICODE);
			zi->oper[1] = SMALL_OR_LARGE(uchar & 0xffff);
			pre_space = 0;
		} else if(utf8[pos + n]) {
			/* String too long (for runtime uppercase buffer) */
			assert(n);
			zi = append_instr(r, Z_CALL2N);
			zi->oper[0] = ROUTINE(pre_space? R_SPACE_PRINT_NOSPACE : R_NOSPACE_PRINT_NOSPACE);
			zi->oper[1] = REF(stringlabel);
			pre_space = 0;
		} else {
			if(n) {
				if(!pre_space && !post_space) {
					variant = ROUTINE(R_NOSPACE_PRINT_NOSPACE);
				} else if(!pre_space && post_space) {
					variant = ROUTINE(R_NOSPACE_PRINT_AUTO);
				} else if(pre_space && !post_space) {
					variant = ROUTINE(R_SPACE_PRINT_NOSPACE);
				} else {
					variant = VALUE(REG_R_SPA);
				}

				zi = append_instr(r, Z_CALL2N);
				zi->oper[0] = variant;
				zi->oper[1] = REF(stringlabel);
			} else {
				assert(!pre_space);
				assert(!nl_printed);
				zi = append_instr(r, Z_STORE);
				zi->oper[0] = SMALL(REG_SPACE);
				zi->oper[1] = SMALL(1);
			}
		}
	}

	if(nl_printed) {
		zi = append_instr(r, Z_STORE);
		zi->oper[0] = SMALL(REG_SPACE);
		zi->oper[1] = SMALL(3 + nl_printed);
	}

	free(utf8);
	return an;
}

struct astnode *compile_add_words(struct routine *r, struct astnode *an) {
	struct zinstr *zi;
	struct astnode *sub;
	int n, i, nchunk, j;
	struct astnode **table;
	uint32_t oper[3];

	n = 0;
	sub = an;
	while(sub && (
		sub->kind == AN_BAREWORD ||
		(sub->kind == AN_RULE && (
			sub->predicate->builtin == BI_SPACE ||
			sub->predicate->builtin == BI_NOSPACE ||
			sub->predicate->builtin == BI_LINE ||
			sub->predicate->builtin == BI_PAR))))
	{
		if(sub->kind == AN_BAREWORD) {
			n++;
		}
		sub = sub->next_in_body;
	}

	table = malloc(n * sizeof(struct astnode *));
	n = 0;
	while(an && (
		an->kind == AN_BAREWORD ||
		(an->kind == AN_RULE && (
			an->predicate->builtin == BI_SPACE ||
			an->predicate->builtin == BI_NOSPACE ||
			an->predicate->builtin == BI_LINE ||
			an->predicate->builtin == BI_PAR))))
	{
		if(an->kind == AN_BAREWORD) {
			table[n++] = an;
		}
		an = an->next_in_body;
	}
	assert(n > 0);

	for(i = 0; i < n; i += nchunk) {
		nchunk = 3;
		if(nchunk > n - i) nchunk = n - i;
		for(j = 0; j < nchunk; j++) {
			oper[j] = compile_ast_to_oper(0, table[i + j], 0, 0);
		}
		if(nchunk == 1) {
			zi = append_instr(r, Z_CALL2N);
			zi->oper[0] = ROUTINE(R_AUX_PUSH1);
		} else if(nchunk == 2) {
			zi = append_instr(r, Z_CALLVN);
			zi->oper[0] = ROUTINE(R_AUX_PUSH2);
		} else {
			assert(nchunk == 3);
			zi = append_instr(r, Z_CALLVN);
			zi->oper[0] = ROUTINE(R_AUX_PUSH3);
		}
		for(j = 0; j < nchunk; j++) {
			zi->oper[1 + j] = oper[j];
		}
	}

	free(table);
	return an;
}

void compile_now(struct routine *r, struct astnode *an, struct var *vars, struct program *prg) {
	struct zinstr *zi;
	struct predname *predname;
	struct predicate *pred;
	struct backend_pred *bp;
	int t1, t2;
	uint32_t o1, o2;
	uint16_t ll;

	while(an) {
		if(an->kind == AN_RULE) {
			predname = an->predicate;
			pred = predname->pred;
			bp = pred->backend;
			if(predname->builtin == BI_HASPARENT) {
				o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
				if(an->children[0]->kind != AN_TAG) {
					t1 = next_temp++;
					zi = append_instr(r, Z_CALL2S);
					zi->oper[0] = ROUTINE(R_DEREF_OBJ_FORCE);
					zi->oper[1] = o1;
					zi->store = REG_X + t1;
					o1 = VALUE(REG_X + t1);
				}
				o2 = compile_ast_to_oper(r, an->children[1], vars, 0);
				if(an->children[1]->kind != AN_TAG) {
					t2 = next_temp++;
					zi = append_instr(r, Z_CALL2S);
					zi->oper[0] = ROUTINE(R_DEREF_OBJ_FORCE);
					zi->oper[1] = o2;
					zi->store = REG_X + t2;
					o2 = VALUE(REG_X + t2);
				}
				zi = append_instr(r, Z_INSERT_OBJ);
				zi->oper[0] = o1;
				zi->oper[1] = o2;
			} else if(predname->arity == 0) {
				zi = append_instr(r, Z_OR);
				zi->oper[0] = USERGLOBAL(bp->user_global);
				zi->oper[1] = SMALL_OR_LARGE(bp->user_flag_mask);
				zi->store = DEST_USERGLOBAL(bp->user_global);
			} else if(predname->arity == 1) {
				assert(!(pred->flags & PREDF_FIXED_FLAG));
				if(pred->flags & PREDF_GLOBAL_VAR) {
					o1 = compile_ast_to_oper(r, an->children[0], vars, 1);
					zi = append_instr(r, Z_CALLVN);
					zi->oper[0] = ROUTINE(R_SET_LONGTERM_VAR);
					zi->oper[1] = REF(G_USER_GLOBALS);
					zi->oper[2] = SMALL(bp->user_global);
					zi->oper[3] = o1;
				} else if(bp->set_label) {
					if(an->children[0]->kind == AN_TAG) {
						o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
						zi = append_instr(r, Z_CALL2N);
						zi->oper[0] = ROUTINE(bp->set_label);
						zi->oper[1] = o1;
					} else {
						o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
						zi = append_instr(r, Z_CALL2S);
						zi->oper[0] = ROUTINE(R_DEREF_OBJ_FORCE);
						zi->oper[1] = o1;
						zi->store = REG_TEMP;
						zi = append_instr(r, Z_CALL2N);
						zi->oper[0] = ROUTINE(bp->set_label);
						zi->oper[1] = VALUE(REG_TEMP);
					}
				} else {
					assert(bp->object_flag < NZOBJFLAG);
					if(an->children[0]->kind == AN_TAG) {
						zi = append_instr(r, Z_SET_ATTR);
						zi->oper[0] = SMALL_OR_LARGE(1 + an->children[0]->word->obj_id);
						zi->oper[1] = SMALL(bp->object_flag);
					} else {
						o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
						zi = append_instr(r, Z_CALL2S);
						zi->oper[0] = ROUTINE(R_DEREF_OBJ_FORCE);
						zi->oper[1] = o1;
						zi->store = REG_TEMP;
						zi = append_instr(r, Z_SET_ATTR);
						zi->oper[0] = VALUE(REG_TEMP);
						zi->oper[1] = SMALL(bp->object_flag);
					}
				}
			} else if(predname->arity == 2) {
				o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
				if(an->children[0]->kind != AN_TAG) {
					t1 = next_temp++;
					zi = append_instr(r, Z_CALL2S);
					zi->oper[0] = ROUTINE(R_DEREF_OBJ_FORCE);
					zi->oper[1] = o1;
					zi->store = REG_X + t1;
					o1 = VALUE(REG_X + t1);
				}
				o2 = compile_ast_to_oper(r, an->children[1], vars, 1);
				zi = append_instr(r, Z_CALLVN);
				zi->oper[0] = ROUTINE(R_SET_LONGTERM_VAR);
				zi->oper[1] = REF(bp->propbase_label);
				zi->oper[2] = o1;
				zi->oper[3] = o2;
			} else {
				assert(0); // unimplemented (now) case
				exit(1);
			}
		} else if(an->kind == AN_NEG_RULE) {
			predname = an->predicate;
			pred = predname->pred;
			bp = pred->backend;
			if(predname->builtin == BI_HASPARENT) {
				if(an->children[1]->kind == AN_VARIABLE
				&& !an->children[1]->word->name[0]) {
					if(an->children[0]->kind == AN_TAG) {
						zi = append_instr(r, Z_REMOVE_OBJ);
						zi->oper[0] = SMALL_OR_LARGE(1 + an->children[0]->word->obj_id);
					} else {
						o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
						zi = append_instr(r, Z_CALL2S);
						zi->oper[0] = ROUTINE(R_DEREF_OBJ_FORCE);
						zi->oper[1] = o1;
						zi->store = REG_TEMP;
						zi = append_instr(r, Z_REMOVE_OBJ);
						zi->oper[0] = VALUE(REG_TEMP);
					}
				} else {
					report(LVL_ERR, an->line, "When resetting a per-object variable, the second argument must be anonymous ($).");
					exit(1);
				}
			} else if(predname->arity == 0) {
				zi = append_instr(r, Z_AND);
				zi->oper[0] = USERGLOBAL(bp->user_global);
				zi->oper[1] = LARGE(bp->user_flag_mask ^ 0xffff);
				zi->store = DEST_USERGLOBAL(bp->user_global);
			} else if(predname->arity == 1) {
				assert(!(pred->flags & PREDF_FIXED_FLAG));
				if(pred->flags & PREDF_GLOBAL_VAR) {
					zi = append_instr(r, Z_CALLVN);
					zi->oper[0] = ROUTINE(R_SET_LONGTERM_VAR);
					zi->oper[1] = REF(G_USER_GLOBALS);
					zi->oper[2] = SMALL(bp->user_global);
					zi->oper[3] = SMALL(0);
				} else if(bp->clear_label) {
					o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
					zi = append_instr(r, Z_CALL2N);
					zi->oper[0] = ROUTINE(bp->clear_label);
					zi->oper[1] = o1;
				} else {
					assert(bp->object_flag < NZOBJFLAG);
					if(an->children[0]->kind == AN_TAG) {
						zi = append_instr(r, Z_CLEAR_ATTR);
						zi->oper[0] = SMALL_OR_LARGE(1 + an->children[0]->word->obj_id);
						zi->oper[1] = SMALL(bp->object_flag);
					} else {
						o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
						zi = append_instr(r, Z_CALL2S);
						zi->oper[0] = ROUTINE(R_DEREF_OBJ_FORCE);
						zi->oper[1] = o1;
						zi->store = REG_TEMP;
						zi = append_instr(r, Z_CLEAR_ATTR);
						zi->oper[0] = VALUE(REG_TEMP);
						zi->oper[1] = SMALL(bp->object_flag);
					}
				}
			} else if(predname->arity == 2) {
				if(an->children[1]->kind == AN_VARIABLE
				&& !an->children[1]->word->name[0]) {
					if(an->children[0]->kind == AN_VARIABLE
					&& !an->children[0]->word->name[0]) {
						// (now) ~($ relation $) clears the array
						t1 = next_temp++;
						ll = r->next_label++;
						zi = append_instr(r, Z_STORE);
						zi->oper[0] = SMALL(REG_X + t1);
						zi->oper[1] = SMALL(1);
						zi = append_instr(r, OP_LABEL(ll));
						zi = append_instr(r, Z_CALLVN);
						zi->oper[0] = ROUTINE(R_SET_LONGTERM_VAR);
						zi->oper[1] = REF(bp->propbase_label);
						zi->oper[2] = VALUE(REG_X + t1);
						zi->oper[3] = SMALL(0);
						zi = append_instr(r, Z_INC_JLE);
						zi->oper[0] = SMALL(REG_X + t1);
						zi->oper[1] = SMALL_OR_LARGE(prg->nworldobj);
						zi->branch = ll;
					} else {
						o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
						if(an->children[0]->kind != AN_TAG) {
							zi = append_instr(r, Z_CALL2S);
							zi->oper[0] = ROUTINE(R_DEREF_OBJ_FORCE);
							zi->oper[1] = o1;
							zi->store = REG_TEMP;
							o1 = VALUE(REG_TEMP);
						}
						zi = append_instr(r, Z_CALLVN);
						zi->oper[0] = ROUTINE(R_SET_LONGTERM_VAR);
						zi->oper[1] = REF(bp->propbase_label);
						zi->oper[2] = o1;
						zi->oper[3] = SMALL(0);
					}
				} else {
					report(LVL_ERR, an->line, "When resetting a per-object variable, the second argument must be anonymous ($).");
					exit(1);
				}
			} else {
				assert(0); // unimplemented (now) case
				exit(1);
			}
		} else if(an->kind == AN_BLOCK || an->kind == AN_FIRSTRESULT) {
			compile_now(r, an->children[0], vars, prg);
		} else {
			assert(0); // invalid astnode after (now)
			exit(1);
		}
		an = an->next_in_body;
	}
}

void compile_dynamic(struct routine *r, struct astnode *an, struct var *vars, struct program *prg) {
	struct zinstr *zi;
	struct predname *predname;
	struct predicate *pred;
	struct backend_pred *bp;
	int t1;
	uint32_t o1, o2;
	struct var *v;

	predname = an->predicate;
	pred = predname->pred;
	bp = pred->backend;

	if(predname->arity == 0) {
		zi = append_instr(r, (an->kind == AN_RULE)? Z_TESTN : Z_TEST);
		zi->oper[0] = USERGLOBAL(bp->user_global);
		zi->oper[1] = SMALL_OR_LARGE(bp->user_flag_mask);
		zi->branch = RFALSE;
	} else if(predname->arity == 1) {
		if(pred->flags & PREDF_GLOBAL_VAR) {
			if(an->children[0]->kind == AN_VARIABLE
			&& !an->children[0]->word->name[0]) {
				if(an->kind == AN_NEG_RULE) {
					zi = append_instr(r, Z_JNZ);
					zi->oper[0] = USERGLOBAL(bp->user_global);
					zi->branch = RFALSE;
				} else {
					zi = append_instr(r, Z_JZ);
					zi->oper[0] = USERGLOBAL(bp->user_global);
					zi->branch = RFALSE;
				}
			} else {
				if(an->children[0]->kind == AN_VARIABLE) {
					for(v = vars; v; v = v->next) {
						if(v->name == an->children[0]->word) break;
					}
				} else {
					v = 0;
				}
				if(v && !v->used) {
					zi = append_instr(r, Z_JZ);
					zi->oper[0] = USERGLOBAL(bp->user_global);
					zi->branch = RFALSE;
					zi = append_instr(r, Z_CALL2S);
					zi->oper[0] = ROUTINE(R_GET_LONGTERM_VAR);
					zi->oper[1] = USERGLOBAL(bp->user_global);
					if(v->persistent) {
						zi->store = REG_TEMP;
						zi = append_instr(r, Z_STOREW);
						zi->oper[0] = VALUE(REG_ENV);
						zi->oper[1] = SMALL(2 + v->slot);
						zi->oper[2] = VALUE(REG_TEMP);
					} else {
						zi->store = REG_X + v->slot;
					}
					v->used = 1;
				} else {
					if(an->children[0]->unbound) {
						zi = append_instr(r, Z_JZ);
						zi->oper[0] = USERGLOBAL(bp->user_global);
						zi->branch = RFALSE;
					}
					o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
					zi = append_instr(r, Z_CALL2S);
					zi->oper[0] = ROUTINE(R_GET_LONGTERM_VAR);
					zi->oper[1] = USERGLOBAL(bp->user_global);
					zi->store = REG_TEMP;
					if(an->children[0]->kind == AN_INTEGER
					|| an->children[0]->kind == AN_TAG
					|| an->children[0]->kind == AN_DICTWORD
					|| an->children[0]->kind == AN_EMPTY_LIST) {
						zi = append_instr(r, Z_JNE);
						zi->oper[0] = USERGLOBAL(bp->user_global);
						zi->oper[1] = o1;
						zi->branch = RFALSE;
					} else {
						zi = append_instr(r, Z_CALLVN);
						zi->oper[0] = ROUTINE(R_UNIFY);
						zi->oper[1] = VALUE(REG_TEMP);
						zi->oper[2] = o1;
					}
				}
			}
		} else {
			assert(!an->children[0]->unbound);
			if(an->children[0]->kind == AN_TAG && bp->object_flag < NZOBJFLAG) {
				o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
				zi = append_instr(r, (an->kind == AN_RULE)? Z_JNA : Z_JA);
				zi->oper[0] = o1;
				zi->oper[1] = SMALL(bp->object_flag);
				zi->branch = RFALSE;
			} else if(bp->object_flag >= NZOBJFLAG) {
				o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
				zi = append_instr(r, Z_CALL2S);
				zi->oper[0] = ROUTINE(extflagreaders[(bp->object_flag - NZOBJFLAG) / 8]);
				zi->oper[1] = o1;
				zi->store = REG_TEMP;
				zi = append_instr(r, (an->kind == AN_RULE)? Z_TESTN : Z_TEST);
				zi->oper[0] = VALUE(REG_TEMP);
				zi->oper[1] = SMALL(0x80 >> ((bp->object_flag - NZOBJFLAG) & 7));
				zi->branch = RFALSE;
			} else {
				o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
				zi = append_instr(r, Z_CALLVN);
				zi->oper[0] = an->kind == AN_RULE? ROUTINE(R_CHECK_FLAG) : ROUTINE(R_CHECK_FLAG_N);
				zi->oper[1] = o1;
				zi->oper[2] = SMALL(bp->object_flag);
			}
		}
	} else if(predname->arity == 2) {
		assert(!an->children[0]->unbound);
		o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
		if(an->children[0]->kind != AN_TAG) {
			zi = append_instr(r, Z_CALL2S);
			zi->oper[0] = ROUTINE(R_DEREF_OBJ_FAIL);
			zi->oper[1] = o1;
			zi->store = REG_TEMP;
			o1 = VALUE(REG_TEMP);
		}
		zi = append_instr(r, Z_LOADW);
		zi->oper[0] = REF(bp->propbase_label);
		zi->oper[1] = o1;
		zi->store = REG_TEMP;
		if(an->children[1]->kind == AN_VARIABLE
		&& !an->children[1]->word->name[0]) {
			if(an->kind == AN_NEG_RULE) {
				zi = append_instr(r, Z_JNZ);
				zi->oper[0] = VALUE(REG_TEMP);
				zi->branch = RFALSE;
			} else {
				zi = append_instr(r, Z_JZ);
				zi->oper[0] = VALUE(REG_TEMP);
				zi->branch = RFALSE;
			}
		} else {
			if(an->children[1]->kind == AN_VARIABLE) {
				for(v = vars; v; v = v->next) {
					if(v->name == an->children[1]->word) break;
				}
			} else {
				v = 0;
			}
			zi = append_instr(r, Z_CALL2S);
			zi->oper[0] = ROUTINE(R_GET_LONGTERM_VAR);
			zi->oper[1] = VALUE(REG_TEMP);
			if(v && !v->used) {
				if(v->persistent) {
					zi->store = REG_TEMP;
					zi = append_instr(r, Z_STOREW);
					zi->oper[0] = VALUE(REG_ENV);
					zi->oper[1] = SMALL(2 + v->slot);
					zi->oper[2] = VALUE(REG_TEMP);
				} else {
					zi->store = REG_X + v->slot;
				}
				v->used = 1;
			} else {
				t1 = next_temp++;
				zi->store = REG_X + t1;
				o2 = compile_ast_to_oper(r, an->children[1], vars, 0);
				zi = append_instr(r, Z_CALLVN);
				zi->oper[0] = ROUTINE(R_UNIFY);
				zi->oper[1] = VALUE(REG_X + t1);
				zi->oper[2] = o2;
			}
		}
	} else {
		assert(0); // unimplemented dynamic case
		exit(1);
	}
}

struct var *findvar(struct astnode *an, struct var *vars) {
	if(an->kind != AN_VARIABLE) return 0;

	while(vars) {
		if(vars->name == an->word) {
			return vars;
		}
		vars = vars->next;
	}

	return 0;
}

void compile_simple_builtin(struct routine *r, struct astnode *an, struct predicate *me_pred, int envflags, struct var *vars, struct program *prg) {
	struct predname *predname = an->predicate;
	struct predicate *pred = predname->pred;
	struct zinstr *zi;
	int i, reg;
	uint32_t oper[2], o1, o2;
	uint16_t ll;
	struct var *v1, *v2;

	if(pred->flags & PREDF_FAIL) {
		assert(0);
		exit(1);
		zi = append_instr(r, Z_JZ);	// don't confuse txd by dead code
		zi->oper[0] = SMALL(0);
		zi->branch = RFALSE;
		return;
	}

	if(predname->nameflags & PREDNF_OUTPUT) {
		call_output_routine(r, an, vars);
	} else switch(predname->builtin) {
	case BI_RESTART:
		zi = append_instr(r, Z_CALL1N);
		zi->oper[0] = ROUTINE(R_LINE);
		zi = append_instr(r, Z_RESTART);
		break;
	case BI_UNDO:
		// This flag prevents any attempts to undo back to before the
		// first successfully saved undo point, since doing that
		// crashes frotz.
		zi = append_instr(r, Z_TESTN);
		zi->oper[0] = USERGLOBAL(undoflag_global);
		zi->oper[1] = SMALL_OR_LARGE(undoflag_mask);
		zi->branch = RFALSE;
		zi = append_instr(r, Z_RESTORE_UNDO);
		zi->store = REG_TEMP;
		break;
	case BI_RESTORE:
		zi = append_instr(r, Z_CALL1N);
		zi->oper[0] = ROUTINE(R_LINE);
		zi = append_instr(r, Z_RESTORE);
		zi->store = REG_TEMP;
		zi = append_instr(r, Z_STORE);
		zi->oper[0] = SMALL(REG_SPACE);
		zi->oper[1] = SMALL(4);
		break;
	case BI_SCRIPT_OFF:
		zi = append_instr(r, Z_OUTPUT_STREAM);
		zi->oper[0] = LARGE(0xfffe);
		break;
	case BI_PLUS:
	case BI_MINUS:
		if(an->children[1]->kind == AN_INTEGER
		&& an->children[1]->value == 1
		&& (v1 = findvar(an->children[2], vars))
		&& !v1->used) {
			reg = v1->persistent? REG_TEMP : (REG_X + v1->slot);
			o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
			zi = append_instr(r, Z_CALL2S);
			zi->oper[0] = ROUTINE(R_DEREF);
			zi->oper[1] = o1;
			zi->store = reg;
			zi = append_instr(r, (predname->builtin == BI_PLUS)? Z_INC : Z_DEC);
			zi->oper[0] = SMALL(reg);
			zi = append_instr(r, Z_JL);
			zi->oper[0] = VALUE(reg);
			zi->oper[1] = VALUE(REG_4000);
			zi->branch = RFALSE;
			if(v1->persistent) {
				zi = append_instr(r, Z_STOREW);
				zi->oper[0] = VALUE(REG_ENV);
				zi->oper[1] = SMALL(2 + v1->slot);
				zi->oper[2] = VALUE(REG_TEMP);
			}
			v1->used = 1;
			break;
		}
		// else drop through
	case BI_TIMES:
	case BI_DIVIDED:
	case BI_MODULO:
	case BI_RANDOM:
		for(i = 0; i < 2; i++) {
			oper[i] = compile_ast_to_oper(r, an->children[i], vars, 0);
		}
		o1 = compile_ast_to_oper(r, an->children[2], vars, 0);
		zi = append_instr(r, Z_CALLVN);
		if(predname->builtin == BI_PLUS) {
			zi->oper[0] = ROUTINE(R_PLUS);
		} else if(predname->builtin == BI_MINUS) {
			zi->oper[0] = ROUTINE(R_MINUS);
		} else if(predname->builtin == BI_TIMES) {
			zi->oper[0] = ROUTINE(R_TIMES);
		} else if(predname->builtin == BI_DIVIDED) {
			zi->oper[0] = ROUTINE(R_DIVIDED);
		} else if(predname->builtin == BI_MODULO) {
			zi->oper[0] = ROUTINE(R_MODULO);
		} else if(predname->builtin == BI_RANDOM) {
			zi->oper[0] = ROUTINE(R_RANDOM);
		}
		zi->oper[1] = oper[0];
		zi->oper[2] = oper[1];
		zi->oper[3] = o1;
		break;
	case BI_LESSTHAN:
		o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
		o2 = compile_ast_to_oper(r, an->children[1], vars, 0);
		zi = append_instr(r, Z_CALLVS);
		zi->oper[0] = ROUTINE(R_LESS_OR_GREATER);
		zi->oper[1] = o1;
		zi->oper[2] = o2;
		zi->store = REG_TEMP;
		zi = append_instr(r, Z_JGE);
		zi->oper[0] = VALUE(REG_TEMP);
		zi->oper[1] = SMALL(0);
		zi->branch = RFALSE;
		break;
	case BI_GREATERTHAN:
		o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
		o2 = compile_ast_to_oper(r, an->children[1], vars, 0);
		zi = append_instr(r, Z_CALLVS);
		zi->oper[0] = ROUTINE(R_LESS_OR_GREATER);
		zi->oper[1] = o1;
		zi->oper[2] = o2;
		zi->store = REG_TEMP;
		zi = append_instr(r, Z_JLE);
		zi->oper[0] = VALUE(REG_TEMP);
		zi->oper[1] = SMALL(0);
		zi->branch = RFALSE;
		break;
	case BI_TRACE_ON:
		zi = append_instr(r, Z_STORE);
		zi->oper[0] = SMALL(REG_TRACING);
		zi->oper[1] = SMALL(1);
		break;
	case BI_TRACE_OFF:
		zi = append_instr(r, Z_STORE);
		zi->oper[0] = SMALL(REG_TRACING);
		zi->oper[1] = SMALL(0);
		break;
	case BI_BREAKPOINT:
		break;
	case BI_LIST:
		ll = r->next_label++;
		o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
		zi = append_instr(r, Z_CALL2S);
		zi->oper[0] = ROUTINE(R_DEREF);
		zi->oper[1] = o1;
		zi->store = REG_TEMP;
		zi = append_instr(r, Z_JE);
		zi->oper[0] = VALUE(REG_TEMP);
		zi->oper[1] = VALUE(REG_NIL);
		zi->branch = ll;
		zi = append_instr(r, Z_JGE);
		zi->oper[0] = VALUE(REG_TEMP);
		zi->oper[1] = VALUE(REG_E000);
		zi->branch = RFALSE;
		zi = append_instr(r, Z_JL);
		zi->oper[0] = VALUE(REG_TEMP);
		zi->oper[1] = VALUE(REG_C000);
		zi->branch = RFALSE;
		zi = append_instr(r, OP_LABEL(ll));
		break;
	case BI_EMPTY:
		o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
		zi = append_instr(r, Z_CALL2S);
		zi->oper[0] = ROUTINE(R_DEREF);
		zi->oper[1] = o1;
		zi->store = REG_TEMP;
		zi = append_instr(r, Z_JNE);
		zi->oper[0] = VALUE(REG_TEMP);
		zi->oper[1] = VALUE(REG_NIL);
		zi->branch = RFALSE;
		break;
	case BI_NONEMPTY:
		o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
		zi = append_instr(r, Z_CALL2S);
		zi->oper[0] = ROUTINE(R_DEREF);
		zi->oper[1] = o1;
		zi->store = REG_TEMP;
		zi = append_instr(r, Z_JGE);
		zi->oper[0] = VALUE(REG_TEMP);
		zi->oper[1] = VALUE(REG_E000);
		zi->branch = RFALSE;
		zi = append_instr(r, Z_JL);
		zi->oper[0] = VALUE(REG_TEMP);
		zi->oper[1] = VALUE(REG_C000);
		zi->branch = RFALSE;
		break;
	case BI_WORD:
		o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
		zi = append_instr(r, Z_CALL2S);
		zi->oper[0] = ROUTINE(R_DEREF);
		zi->oper[1] = o1;
		zi->store = REG_TEMP;
		zi = append_instr(r, Z_AND);
		zi->oper[0] = VALUE(REG_TEMP);
		zi->oper[1] = VALUE(REG_E000);
		zi->store = REG_TEMP;
		zi = append_instr(r, Z_JNE);
		zi->oper[0] = VALUE(REG_TEMP);
		zi->oper[1] = VALUE(REG_2000);
		zi->oper[2] = VALUE(REG_E000);
		zi->branch = RFALSE;
		break;
	case BI_NUMBER:
		o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
		zi = append_instr(r, Z_CALL2S);
		zi->oper[0] = ROUTINE(R_DEREF);
		zi->oper[1] = o1;
		zi->store = REG_TEMP;
		zi = append_instr(r, Z_JL);
		zi->oper[0] = VALUE(REG_TEMP);
		zi->oper[1] = VALUE(REG_4000);
		zi->branch = RFALSE;
		break;
	case BI_OBJECT:
		assert(!an->children[0]->unbound);
		o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
		if(an->kind == AN_RULE) {
			zi = append_instr(r, Z_CALL2N);
			zi->oper[0] = ROUTINE(R_DEREF_OBJ_FAIL);
			zi->oper[1] = o1;
		} else {
			zi = append_instr(r, Z_CALL2S);
			zi->oper[0] = ROUTINE(R_DEREF_OBJ);
			zi->oper[1] = o1;
			zi->store = REG_TEMP;
			zi = append_instr(r, Z_JNZ);
			zi->oper[0] = VALUE(REG_TEMP);
			zi->branch = RFALSE;
		}
		break;
	case BI_UNBOUND:
		o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
		zi = append_instr(r, Z_CALL2S);
		zi->oper[0] = ROUTINE(R_DEREF);
		zi->oper[1] = o1;
		zi->store = REG_TEMP;
		zi = append_instr(r, Z_JGE);
		zi->oper[0] = VALUE(REG_TEMP);
		zi->oper[1] = VALUE(REG_C000);
		zi->branch = RFALSE;
		break;
	case BI_ROMAN:
		ll = r->next_label++;
		if(envflags & ENVF_MAYBE_FOR_WORDS) {
			zi = append_instr(r, Z_JNZ);
			zi->oper[0] = VALUE(REG_FORWORDS);
			zi->branch = ll;
		}
		zi = append_instr(r, Z_TEXTSTYLE);
		zi->oper[0] = SMALL(0);
		zi = append_instr(r, OP_LABEL(ll));
		break;
	case BI_BOLD:
		zi = append_instr(r, Z_CALL2N);
		zi->oper[0] = ROUTINE(R_ENABLE_STYLE);
		zi->oper[1] = SMALL(2);
		break;
	case BI_ITALIC:
		zi = append_instr(r, Z_CALL2N);
		zi->oper[0] = ROUTINE(R_ENABLE_STYLE);
		zi->oper[1] = SMALL(4);
		break;
	case BI_REVERSE:
		zi = append_instr(r, Z_CALL2N);
		zi->oper[0] = ROUTINE(R_ENABLE_STYLE);
		zi->oper[1] = SMALL(1);
		break;
	case BI_FIXED:
		zi = append_instr(r, Z_CALL2N);
		zi->oper[0] = ROUTINE(R_ENABLE_STYLE);
		zi->oper[1] = SMALL(8);
		break;
	case BI_UPPER:
		ll = r->next_label++;
		if(envflags & ENVF_MAYBE_FOR_WORDS) {
			zi = append_instr(r, Z_JNZ);
			zi->oper[0] = VALUE(REG_FORWORDS);
			zi->branch = ll;
		}
		zi = append_instr(r, Z_STORE);
		zi->oper[0] = SMALL(REG_UPPER);
		zi->oper[1] = SMALL(1);
		zi = append_instr(r, OP_LABEL(ll));
		break;
	case BI_UNIFY:
		if(an->children[0]->kind == AN_VARIABLE) {
			for(v1 = vars; v1; v1 = v1->next) {
				if(v1->name == an->children[0]->word) break;
			}
			if(v1 && v1->used) v1 = 0;
		} else {
			v1 = 0;
		}
		if(an->children[1]->kind == AN_VARIABLE) {
			for(v2 = vars; v2; v2 = v2->next) {
				if(v2->name == an->children[1]->word) break;
			}
			if(v2 && v2->used) v2 = 0;
			if(v1) v2 = 0;
		} else {
			v2 = 0;
		}
#if 0
		if(v1) {
			report(LVL_DEBUG, an->line, "Unify sets lhs");
		} else if(v2) {
			report(LVL_DEBUG, an->line, "Unify sets rhs");
		}
#endif
		o1 = compile_ast_to_oper(r, an->children[0], vars, an->children[1]->unbound);
		o2 = compile_ast_to_oper(r, an->children[1], vars, an->children[0]->unbound);
		if(an->children[0]->kind != AN_PAIR && an->children[0]->kind != AN_VARIABLE) {
			zi = append_instr(r, Z_CALLVN);
			zi->oper[0] = VALUE(REG_R_USIMPLE);
			zi->oper[1] = o1;
			zi->oper[2] = o2;
		} else if(an->children[1]->kind != AN_PAIR && an->children[1]->kind != AN_VARIABLE) {
			zi = append_instr(r, Z_CALLVN);
			zi->oper[0] = VALUE(REG_R_USIMPLE);
			zi->oper[1] = o2;
			zi->oper[2] = o1;
		} else {
			zi = append_instr(r, Z_CALLVN);
			zi->oper[0] = ROUTINE(R_UNIFY);
			zi->oper[1] = o1;
			zi->oper[2] = o2;
		}
		break;
	case BI_GETKEY:
		if(an->children[0]->kind == AN_VARIABLE
		&& !*an->children[0]->word->name) {
			zi = append_instr(r, Z_CALL1N);
			zi->oper[0] = ROUTINE(R_GET_KEY);
		} else {
			o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
			zi = append_instr(r, Z_CALL1S);
			zi->oper[0] = ROUTINE(R_GET_KEY);
			zi->store = REG_TEMP;
			zi = append_instr(r, Z_CALLVN);
			zi->oper[0] = VALUE(REG_R_USIMPLE);
			zi->oper[1] = VALUE(REG_TEMP);
			zi->oper[2] = o1;
		}
		break;
	case BI_CLEAR:
		zi = append_instr(r, Z_ERASE_WINDOW);
		zi->oper[0] = SMALL(0);
		zi = append_instr(r, Z_STORE);
		zi->oper[0] = SMALL(REG_SPACE);
		zi->oper[1] = SMALL(4);
		break;
	case BI_CLEAR_ALL:
		zi = append_instr(r, Z_ERASE_WINDOW);
		zi->oper[0] = VALUE(REG_FFFF);
		zi = append_instr(r, Z_STORE);
		zi->oper[0] = SMALL(REG_SPACE);
		zi->oper[1] = SMALL(4);
		break;
	case BI_WINDOWWIDTH:
		o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
		zi = append_instr(r, Z_LOADB);
		zi->oper[0] = SMALL(0);
		zi->oper[1] = SMALL(0x21);
		zi->store = REG_TEMP;
		zi = append_instr(r, Z_OR);
		zi->oper[0] = VALUE(REG_TEMP);
		zi->oper[1] = VALUE(REG_4000);
		zi->store = REG_TEMP;
		zi = append_instr(r, Z_CALLVN);
		zi->oper[0] = VALUE(REG_R_USIMPLE);
		zi->oper[1] = VALUE(REG_TEMP);
		zi->oper[2] = o1;
		break;
	case BI_CURSORTO:
		o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
		o2 = compile_ast_to_oper(r, an->children[1], vars, 0);
		zi = append_instr(r, Z_CALLVN);
		zi->oper[0] = ROUTINE(R_CURSORTO);
		zi->oper[1] = o1;
		zi->oper[2] = o2;
		break;
	case BI_WORDREP_RETURN:
	case BI_WORDREP_SPACE:
	case BI_WORDREP_BACKSPACE:
	case BI_WORDREP_UP:
	case BI_WORDREP_DOWN:
	case BI_WORDREP_LEFT:
	case BI_WORDREP_RIGHT:
		o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
		zi = append_instr(r, Z_CALLVN);
		zi->oper[0] = VALUE(REG_R_USIMPLE);
		zi->oper[1] = LARGE(
			(predname->builtin == BI_WORDREP_RETURN)? 0x3e0d :
			(predname->builtin == BI_WORDREP_SPACE)? 0x3e20 :
			(predname->builtin == BI_WORDREP_BACKSPACE)? 0x3e08 :
			(predname->builtin == BI_WORDREP_UP)? 0x3e81 :
			(predname->builtin == BI_WORDREP_DOWN)? 0x3e82 :
			(predname->builtin == BI_WORDREP_LEFT)? 0x3e83 :
			(predname->builtin == BI_WORDREP_RIGHT)? 0x3e84 :
			0);
		zi->oper[2] = o1;
		break;
	case BI_HAVE_UNDO:
		zi = append_instr(r, Z_LOADB);
		zi->oper[0] = SMALL(0);
		zi->oper[1] = SMALL(0x11);
		zi->store = REG_TEMP;
		zi = append_instr(r, Z_TESTN);
		zi->oper[0] = VALUE(REG_TEMP);
		zi->oper[1] = SMALL(0x10);
		zi->branch = RFALSE;
		break;
	case BI_HASPARENT:
		assert(!an->children[0]->unbound); // otherwise it's not considered simple
		if(!an->children[1]->unbound) {
			o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
			if(an->children[0]->kind != AN_TAG) {
				zi = append_instr(r, Z_CALL2S);
				zi->oper[0] = ROUTINE(R_DEREF_OBJ_FAIL);
				zi->oper[1] = o1;
				zi->store = REG_TEMP;
				o1 = VALUE(REG_TEMP);
			}
			o2 = compile_ast_to_oper(r, an->children[1], vars, 0);
			if(an->children[1]->kind != AN_TAG) {
				zi = append_instr(r, Z_CALL2S);
				zi->oper[0] = ROUTINE(R_DEREF_OBJ_FAIL);
				zi->oper[1] = o2;
				if(o1 == VALUE(REG_TEMP)) {
					zi->store = REG_X + next_temp++;
				} else {
					zi->store = REG_TEMP;
				}
				o2 = VALUE(zi->store);
			}
			zi = append_instr(r, Z_JIN_N);
			zi->oper[0] = o1;
			zi->oper[1] = o2;
			zi->branch = RFALSE;
		} else {
			o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
			o2 = compile_ast_to_oper(r, an->children[1], vars, 0);
			if(an->children[0]->kind != AN_TAG) {
				zi = append_instr(r, Z_CALL2S);
				zi->oper[0] = ROUTINE(R_DEREF_OBJ_FAIL);
				zi->oper[1] = o1;
				zi->store = REG_TEMP;
				o1 = VALUE(REG_TEMP);
			}
			zi = append_instr(r, Z_GET_PARENT);
			zi->oper[0] = o1;
			zi->store = REG_TEMP;
			if(an->children[1]->unbound) {
				zi = append_instr(r, Z_JZ);
				zi->oper[0] = VALUE(REG_TEMP);
				zi->branch = RFALSE;
			}
			zi = append_instr(r, Z_CALLVN);
			zi->oper[0] = VALUE(REG_R_USIMPLE);	// also fails if no parent, since o2 is bound
			zi->oper[1] = VALUE(REG_TEMP);
			zi->oper[2] = o2;
		}
		break;
	case 0:
		compile_dynamic(r, an, vars, prg);
		break;
	default:
		printf("%d\n", predname->builtin);
		assert(0); // unimplemented simple builtin
		exit(1);
	}
}

void compile_dynlinkage_set(struct predicate *pred, uint16_t label) {
	// Set flag for one object, already resolved into id and passed as parameter.
	// This is a simple subroutine with one parameter.

	struct routine *r;
	struct zinstr *zi;
	struct backend_pred *bp = pred->backend;

	assert(bp->object_flag < NZOBJFLAG);

	r = make_routine(label, 1);

	zi = append_instr(r, Z_JA);
	zi->oper[0] = VALUE(REG_LOCAL+0);
	zi->oper[1] = SMALL(bp->object_flag);
	zi->branch = RFALSE;

	zi = append_instr(r, Z_SET_ATTR);
	zi->oper[0] = VALUE(REG_LOCAL+0);
	zi->oper[1] = SMALL(bp->object_flag);
	zi = append_instr(r, Z_STOREW);
	zi->oper[0] = REF(bp->propbase_label);
	zi->oper[1] = VALUE(REG_LOCAL+0);
	zi->oper[2] = USERGLOBAL(bp->user_global);
	zi = append_instr(r, Z_LOAD);
	zi->oper[0] = SMALL(REG_LOCAL+0);
	zi->store = DEST_USERGLOBAL(bp->user_global);

	zi = append_instr(r, Z_RFALSE);
}

void compile_dynlinkage_clear(struct predicate *pred, uint16_t label) {
	// Clear flag for one object or (if the parameter is unbound) all objects.
	// This is a simple subroutine with one parameter.

	struct routine *r;
	struct zinstr *zi;
	struct backend_pred *bp = pred->backend;

	assert(bp->object_flag < NZOBJFLAG);

	r = make_routine(label, 3);

	if(pred->dynamic->linkage_flags & LINKF_CLEAR) {
		zi = append_instr(r, Z_CALL2S);
		zi->oper[0] = ROUTINE(R_DEREF);
		zi->oper[1] = VALUE(REG_LOCAL+0);
		zi->store = REG_LOCAL+0;

		zi = append_instr(r, Z_JGE);
		zi->oper[0] = VALUE(REG_LOCAL+0);
		zi->oper[1] = VALUE(REG_C000);
		zi->branch = 1;

		// Clear flag for all objects:

		zi = append_instr(r, Z_JZ);
		zi->oper[0] = USERGLOBAL(bp->user_global);
		zi->branch = RFALSE;
		
		zi = append_instr(r, OP_LABEL(2));

		zi = append_instr(r, Z_CLEAR_ATTR);
		zi->oper[0] = USERGLOBAL(bp->user_global);
		zi->oper[1] = SMALL(bp->object_flag);

		zi = append_instr(r, Z_LOADW);
		zi->oper[0] = REF(bp->propbase_label);
		zi->oper[1] = USERGLOBAL(bp->user_global);
		zi->store = DEST_USERGLOBAL(bp->user_global);
		zi = append_instr(r, Z_JNZ);
		zi->oper[0] = USERGLOBAL(bp->user_global);
		zi->branch = 2;

		zi = append_instr(r, Z_RFALSE);

		zi = append_instr(r, OP_LABEL(1));

		zi = append_instr(r, Z_JL);
		zi->oper[0] = VALUE(REG_LOCAL+0);
		zi->oper[1] = SMALL(1);
		zi->branch = RFALSE;
		zi = append_instr(r, Z_JGE);
		zi->oper[0] = VALUE(REG_LOCAL+0);
		zi->oper[1] = VALUE(REG_NIL);
		zi->branch = RFALSE;
	} else {
		zi = append_instr(r, Z_CALL2S);
		zi->oper[0] = ROUTINE(R_DEREF_OBJ_FAIL);
		zi->oper[1] = VALUE(REG_LOCAL+0);
		zi->store = REG_LOCAL+0;
	}

	// Clear flag for one (resolved) object:
	zi = append_instr(r, Z_JNA);
	zi->oper[0] = VALUE(REG_LOCAL+0);
	zi->oper[1] = SMALL(bp->object_flag);
	zi->branch = RFALSE;

	zi = append_instr(r, Z_CLEAR_ATTR);
	zi->oper[0] = VALUE(REG_LOCAL+0);
	zi->oper[1] = SMALL(bp->object_flag);

	zi = append_instr(r, Z_JE);
	zi->oper[0] = VALUE(REG_LOCAL+0);
	zi->oper[1] = USERGLOBAL(bp->user_global);
	zi->branch = 3;

	zi = append_instr(r, Z_STORE);
	zi->oper[0] = SMALL(REG_LOCAL + 1);
	zi->oper[1] = USERGLOBAL(bp->user_global);

	zi = append_instr(r, OP_LABEL(4));
	zi = append_instr(r, Z_LOADW);
	zi->oper[0] = REF(bp->propbase_label);
	zi->oper[1] = VALUE(REG_LOCAL+1);
	zi->store = REG_LOCAL+2;
	zi = append_instr(r, Z_JE);
	zi->oper[0] = VALUE(REG_LOCAL+0);
	zi->oper[1] = VALUE(REG_LOCAL+2);
	zi->branch = 5;
	zi = append_instr(r, Z_STORE);
	zi->oper[0] = SMALL(REG_LOCAL+1);
	zi->oper[1] = VALUE(REG_LOCAL+2);
	zi = append_instr(r, Z_JUMP);
	zi->oper[0] = REL_LABEL(4);

	zi = append_instr(r, OP_LABEL(5));
	zi = append_instr(r, Z_LOADW);
	zi->oper[0] = REF(bp->propbase_label);
	zi->oper[1] = VALUE(REG_LOCAL+2);
	zi->store = REG_LOCAL+2;
	zi = append_instr(r, Z_STOREW);
	zi->oper[0] = REF(bp->propbase_label);
	zi->oper[1] = VALUE(REG_LOCAL+1);
	zi->oper[2] = VALUE(REG_LOCAL+2);
	zi = append_instr(r, Z_RFALSE);

	// Special case: Remove first element of list
	zi = append_instr(r, OP_LABEL(3));
	zi = append_instr(r, Z_LOADW);
	zi->oper[0] = REF(bp->propbase_label);
	zi->oper[1] = USERGLOBAL(bp->user_global);
	zi->store = DEST_USERGLOBAL(bp->user_global);
	zi = append_instr(r, Z_RFALSE);
}

void compile_dynlinkage_list(struct predicate *pred, uint16_t label) {
	// Backtrack over all objects having the flag set.
	// This is a predicate with one argument.

	struct routine *r;
	struct zinstr *zi;
	struct backend_pred *bp = pred->backend;
	uint16_t sublab = make_routine_label();

	assert(bp->object_flag < NZOBJFLAG);

	r = make_routine(label, 0);

	zi = append_instr(r, Z_CALL2S);
	zi->oper[0] = ROUTINE(R_DEREF);
	zi->oper[1] = VALUE(REG_A+0);
	zi->store = REG_A+0;
	zi = append_instr(r, Z_JL);
	zi->oper[0] = VALUE(REG_A+0);
	zi->oper[1] = VALUE(REG_C000);
	zi->branch = 1;

	// Case: The argument is already bound.
	zi = append_instr(r, Z_JL);
	zi->oper[0] = VALUE(REG_A+0);
	zi->oper[1] = SMALL(1);
	zi->branch = RFALSE;
	zi = append_instr(r, Z_JGE);
	zi->oper[0] = VALUE(REG_A+0);
	zi->oper[1] = VALUE(REG_NIL);
	zi->branch = RFALSE;
	zi = append_instr(r, Z_JA);
	zi->oper[0] = VALUE(REG_A+0);
	zi->oper[1] = SMALL(bp->object_flag);
	zi->branch = 3;

	zi = append_instr(r, Z_RFALSE);

	zi = append_instr(r, OP_LABEL(1));

	// Case: There are no objects with the flag set.
	zi = append_instr(r, Z_JZ);
	zi->oper[0] = USERGLOBAL(bp->user_global);
	zi->branch = RFALSE;
	zi = append_instr(r, Z_LOADW);
	zi->oper[0] = REF(bp->propbase_label);
	zi->oper[1] = USERGLOBAL(bp->user_global);
	zi->store = REG_A + 1;
	zi = append_instr(r, Z_JZ);
	zi->oper[0] = VALUE(REG_A+1);
	zi->branch = 2; // Case: There is exactly one object with the flag set.

	// Case: There are at least two objects with the flag set.
	zi = append_instr(r, Z_CALLVN);
	zi->oper[0] = ROUTINE(R_TRY_ME_ELSE);
	zi->oper[1] = SMALL((2 + CHOICE_SIZEOF) * 2);
	zi->oper[2] = ROUTINE(sublab);

	zi = append_instr(r, OP_LABEL(2));

	zi = append_instr(r, Z_CALLVN);
	zi->oper[0] = VALUE(REG_R_USIMPLE);
	zi->oper[1] = USERGLOBAL(bp->user_global);
	zi->oper[2] = VALUE(REG_A+0);

	zi = append_instr(r, OP_LABEL(3));

	zi = append_instr(r, Z_LOAD);
	zi->oper[0] = VALUE(REG_SIMPLEREF);
	zi->store = REG_CHOICE;
	zi = append_instr(r, Z_RET);
	zi->oper[0] = VALUE(REG_CONT);

	r = make_routine(sublab, 0);

	zi = append_instr(r, Z_CALLVN);
	zi->oper[0] = ROUTINE(R_RETRY_ME_ELSE);
	zi->oper[1] = SMALL(2);
	zi->oper[2] = ROUTINE(sublab);
	zi = append_instr(r, Z_LOADW);
	zi->oper[0] = REF(bp->propbase_label);
	zi->oper[1] = VALUE(REG_A+1);
	zi->store = REG_TEMP;
	zi = append_instr(r, Z_JZ);
	zi->oper[0] = VALUE(REG_TEMP);
	zi->branch = 1;

	zi = append_instr(r, Z_STOREW);
	zi->oper[0] = VALUE(REG_CHOICE);
	zi->oper[1] = SMALL(CHOICE_SIZEOF + 1);
	zi->oper[2] = VALUE(REG_TEMP);

	zi = append_instr(r, OP_LABEL(2));

	zi = append_instr(r, Z_CALLVN);
	zi->oper[0] = VALUE(REG_R_USIMPLE);
	zi->oper[1] = VALUE(REG_A + 1);
	zi->oper[2] = VALUE(REG_A + 0);
	zi = append_instr(r, Z_LOAD);
	zi->oper[0] = VALUE(REG_SIMPLEREF);
	zi->store = REG_CHOICE;
	zi = append_instr(r, Z_RET);
	zi->oper[0] = VALUE(REG_CONT);

	zi = append_instr(r, OP_LABEL(1));

	zi = append_instr(r, Z_CALL1N);
	zi->oper[0] = ROUTINE(R_TRUST_ME_0);
	zi = append_instr(r, Z_JUMP);
	zi->oper[0] = REL_LABEL(2);
}

void compile_extflag_reader(uint16_t routine_label, uint16_t array_label) {
	// Read external fixed flag byte from a hardcoded array.
	// The single parameter is the object, and it must be bound.
	// The return value is the byte value from the array.

	struct routine *r;
	struct zinstr *zi;

	r = make_routine(routine_label, 1);

	// Deref the object.

	zi = append_instr(r, Z_JGE);
	zi->oper[0] = VALUE(REG_LOCAL+0);
	zi->oper[1] = VALUE(REG_C000);
	zi->branch = 2;
	zi = append_instr(r, OP_LABEL(1));
	zi = append_instr(r, Z_ADD);
	zi->oper[0] = VALUE(REG_LOCAL+0);
	zi->oper[1] = VALUE(REG_LOCAL+0);
	zi->store = REG_LOCAL+0;
	zi = append_instr(r, Z_LOADW);
	zi->oper[0] = VALUE(REG_LOCAL+0);
	zi->oper[1] = SMALL(0);
	zi->store = REG_LOCAL+0;
	zi = append_instr(r, Z_JL);
	zi->oper[0] = VALUE(REG_LOCAL+0);
	zi->oper[1] = VALUE(REG_C000);
	zi->branch = 1;
	zi = append_instr(r, OP_LABEL(2));

	// Check the type. All flags are clear for non-objects.

	zi = append_instr(r, Z_JLE);
	zi->oper[0] = VALUE(REG_LOCAL+0);
	zi->oper[1] = SMALL(0);
	zi->branch = RFALSE;
	zi = append_instr(r, Z_JGE);
	zi->oper[0] = VALUE(REG_LOCAL+0);
	zi->oper[1] = VALUE(REG_NIL);
	zi->branch = RFALSE;

	// Read the array byte (1-indexed)

	zi = append_instr(r, Z_LOADB);
	zi->oper[0] = REF(array_label);
	zi->oper[1] = VALUE(REG_LOCAL+0);
	zi->store = REG_LOCAL+0;

	zi = append_instr(r, Z_RET);
	zi->oper[0] = VALUE(REG_LOCAL+0);
}

void compile_body(struct astnode *an, struct routine **rptr, struct predicate *pred, int envflags, struct var *vars, int tail, struct program *prg);
void compile_if(struct astnode *an, struct routine **rptr, struct predicate *pred, int envflags, struct var *vars, int tail, struct program *prg);

struct astnode *compile_simple_astnode(struct routine *r, struct astnode *an, struct predicate *pred, int envflags, struct var *vars, struct program *prg) {
	struct zinstr *zi;
	uint32_t o1;
	struct routine *rcopy;
	uint16_t ll1, ll2;
	struct astnode *next1, *next2;

	switch(an->kind) {
	case AN_BAREWORD:
		if(envflags & ENVF_MAYBE_FOR_WORDS) {
			if(pred->flags & PREDF_INVOKED_NORMALLY) {
				ll1 = r->next_label++;
				ll2 = r->next_label++;
				zi = append_instr(r, Z_JZ);
				zi->oper[0] = VALUE(REG_FORWORDS);
				zi->branch = ll1;
				next1 = compile_add_words(r, an);
				zi = append_instr(r, Z_JUMP);
				zi->oper[0] = REL_LABEL(ll2);
				zi = append_instr(r, OP_LABEL(ll1));
				next2 = compile_output(r, an);
				zi = append_instr(r, OP_LABEL(ll2));
				assert(next1 == next2);
				return next2;
			} else {
				return compile_add_words(r, an);
			}
		} else {
			assert(pred->flags & PREDF_INVOKED_NORMALLY);
			return compile_output(r, an);
		}
	case AN_RULE:
		assert(!(an->predicate->pred->flags & PREDF_FAIL));
		compile_simple_builtin(r, an, pred, envflags, vars, prg);
		return an->next_in_body;
	case AN_NEG_RULE:
		if(!(an->predicate->pred->flags & PREDF_FAIL)) {
			compile_simple_builtin(r, an, pred, envflags, vars, prg);
		}
		return an->next_in_body;
	case AN_TAG:
	case AN_VARIABLE:
	case AN_INTEGER:
	case AN_PAIR:
	case AN_EMPTY_LIST:
	case AN_DICTWORD:
		o1 = compile_ast_to_oper(r, an, vars, 1);
		zi = append_instr(r, Z_CALL2N);
		zi->oper[0] = ROUTINE(R_PRINT_OR_PUSH);
		zi->oper[1] = o1;
		return an->next_in_body;
	case AN_JUST:
		if(envflags & ENVF_CUT_SAVED) {
			zi = append_instr(r, Z_LOADW);
			zi->oper[0] = VALUE(REG_ENV);
			zi->oper[1] = SMALL(ENV_CUT);
			zi->store = REG_CHOICE;
		} else {
			zi = append_instr(r, Z_STORE);
			zi->oper[0] = SMALL(REG_CHOICE);
			zi->oper[1] = VALUE(REG_A + (envflags & ENVF_ARITY_MASK));
		}
		return an->next_in_body;
	case AN_NOW:
		compile_now(r, an->children[0], vars, prg);
		return an->next_in_body;
	case AN_IF:
		rcopy = r;
		compile_if(an, &rcopy, pred, envflags, vars, 0, prg);
		assert(r == rcopy);
		return an->next_in_body;
	case AN_BLOCK:
	case AN_FIRSTRESULT:
		rcopy = r;
		compile_body(an->children[0], &rcopy, pred, envflags, vars, 0, prg);
		assert(r == rcopy);
		return an->next_in_body;
	default:
		assert(0); // unimplemented simple operation
		exit(1);
	}
}

static void find_vars(struct astnode *an, struct var **vars, int *subgoal) {
	struct var *v;
	int i;

	while(an) {
		if(an->kind == AN_VARIABLE) {
			if(an->word->name[0]) {
				for(v = *vars; v; v = v->next) {
					if(v->name == an->word) {
						if(!subgoal || v->used_in_subgoal != *subgoal) {
							v->persistent = 1;
						}
						break;
					}
				}
				if(!v) {
					v = calloc(1, sizeof(*v));
					v->name = an->word;
					if(subgoal) {
						v->used_in_subgoal = *subgoal;
					} else {
						v->persistent = 1;
					}
					v->next = *vars;
					*vars = v;
				}
				v->occurrences++;
			}
		} else {
			if(an->kind == AN_IF
			|| an->kind == AN_FIRSTRESULT
			|| an->kind == AN_NEG_RULE
			|| an->kind == AN_NEG_BLOCK) {
				v = calloc(1, sizeof(*v));
				v->name = an->word;
				v->persistent = 1;
				v->next = *vars;
				*vars = v;
			}
			if(an->kind == AN_IF
			|| an->kind == AN_OR
			|| an->kind == AN_EXHAUST
			|| an->kind == AN_FIRSTRESULT
			|| an->kind == AN_COLLECT
			|| an->kind == AN_COLLECT_WORDS
			|| an->kind == AN_DETERMINE_OBJECT
			|| an->kind == AN_SELECT
			|| an->kind == AN_STOPPABLE
			|| an->kind == AN_STATUSBAR) {
				for(i = 0; i < an->nchild; i++) {
					find_vars(an->children[i], vars, subgoal);
					if(subgoal) (*subgoal)++;
				}
			} else {
				for(i = 0; i < an->nchild; i++) {
					find_vars(an->children[i], vars, subgoal);
				}
			}
			if(an->kind == AN_RULE
			|| an->kind == AN_NEG_RULE) {
				if(subgoal
				&& !(an->predicate->builtin && !((struct backend_pred *) an->predicate->pred->backend)->global_label)) {
					(*subgoal)++;
				}
			}
		}
		an = an->next_in_body;
	}
}

static int count_occurrences(struct astnode *an, struct word *name) {
	int n = 0, i;

	while(an) {
		if(an->kind == AN_VARIABLE && an->word == name) {
			n++;
		} else {
			for(i = 0; i < an->nchild; i++) {
				n += count_occurrences(an->children[i], name);
			}
		}
		an = an->next_in_body;
	}

	return n;
}

void free_vars(struct var *v) {
	struct var *next;

	while(v) {
		next = v->next;
		free(v);
		v = next;
	}
}

struct var *deep_copy_vars(struct var *vars) {
	struct var *first, **dest = &first;

	while(vars) {
		*dest = malloc(sizeof(struct var));
		memcpy(*dest, vars, sizeof(struct var));
		dest = &(*dest)->next;
		vars = vars->next;
	}
	*dest = 0;

	return first;
}

uint16_t alloc_user_flag(uint16_t *mask) {
	if(next_user_flag < 16) {
		*mask = 1 << (next_user_flag++);
	} else {
		user_flags_global = next_user_global++;
		*mask = 1;
		next_user_flag = 1;
	}
	return user_flags_global;
}

void dump_routine(struct routine *r) {
	int i, j;
	struct zinstr *zi;

	printf("Routine:\n");
	for(i = 0; i < r->ninstr; i++) {
		zi = &r->instr[i];

		printf(" %03x", zi->op);
		for(j = 0; j < 4; j++) {
			if(zi->oper[j]) {
				printf(" %05x", zi->oper[j]);
			} else {
				printf(" -----");
			}
		}
		if(zi->store) {
			printf(" --> %02x", zi->store);
		}
		if(zi->branch) {
			printf(" ? %02x", zi->branch);
		}
		if(zi->string) {
			printf(" \"%s\"", zi->string);
		}
		printf("\n");
	}
}

void compile_deallocate(struct routine *r, int envflags) {
	struct zinstr *zi;

	if(envflags & ENVF_ENV) {
		if(envflags & ENVF_SIMPLEREF_SAVED) {
			zi = append_instr(r, Z_LOADW);
			zi->oper[0] = VALUE(REG_ENV);
			zi->oper[1] = SMALL((envflags & ENVF_CUT_SAVED)? 4 : 3);
			zi->store = REG_SIMPLEREF;
		}

		zi = append_instr(r, Z_CALL1N);
		if((envflags & ENVF_CUT_SAVED)
		&& (envflags & ENVF_SIMPLE_SAVED)) {
			zi->oper[0] = ROUTINE(R_DEALLOCATE_CS);
		} else if(envflags & ENVF_SIMPLE_SAVED) {
			zi->oper[0] = ROUTINE(R_DEALLOCATE_S);
		} else {
			zi->oper[0] = ROUTINE(R_DEALLOCATE);
		}
	}
};

void collect_vars(struct astnode *an, struct var *vars, struct var ***dest, int *nalloc, int *n) {
	struct var *v;
	int i;

	while(an) {
		if(an->kind == AN_VARIABLE) {
			for(v = vars; v; v = v->next) {
				if(v->name == an->word
				&& v->persistent
				&& !v->used) {
					if(*n >= *nalloc) {
						*nalloc = (*n) * 2 + 4;
						*dest = realloc(*dest, sizeof(struct var *) * (*nalloc));
					}
					(*dest)[(*n)++] = v;
					v->used = 1;
				}
			}
		} else {
			for(i = 0; i < an->nchild; i++) {
				collect_vars(an->children[i], vars, dest, nalloc, n);
			}
		}
		an = an->next_in_body;
	}
}

static int cmp_varslot(const void *a, const void *b) {
	const struct var *aa = *(const struct var * const *) a;
	const struct var *bb = *(const struct var * const *) b;

	return aa->slot - bb->slot;
}

void ensure_vars_exist(struct astnode *an, struct routine *r, struct var *vars, int only_children) {
	struct var **array = 0;
	int nalloc = 0, n = 0, i, count;
	struct zinstr *zi;

	if(only_children) {
		for(i = 0; i < an->nchild; i++) {
			collect_vars(an->children[i], vars, &array, &nalloc, &n);
		}
	} else {
		collect_vars(an, vars, &array, &nalloc, &n);
	}
	qsort(array, n, sizeof(struct var *), cmp_varslot);
	for(i = 0; i < n; i += count) {
		count = 1;
		while(i + count < n && array[i + count]->slot == array[i + count - 1]->slot + 1) {
			count++;
		}
		if(count == 1) {
			zi = append_instr(r, Z_CALL2N);
			zi->oper[0] = ROUTINE(R_PUSH_VAR_SETENV);
			zi->oper[1] = SMALL(2 + array[i]->slot);
		} else {
			zi = append_instr(r, Z_CALLVN);
			zi->oper[0] = ROUTINE(R_PUSH_VARS_SETENV);
			zi->oper[1] = SMALL(2 + array[i]->slot);
			zi->oper[2] = SMALL(2 + array[i]->slot + count - 1);
		}
	}
}

void add_trace_code(struct routine *r, uint16_t variant, uint16_t printer, line_t line) {
	struct zinstr *zi;

	zi = append_instr(r, Z_CALLVN);
	zi->oper[0] = ROUTINE(variant);
	zi->oper[1] = ROUTINE(printer);
	zi->oper[2] = LARGE(LINEPART(line));
	zi->oper[3] = SMALL(FILENUMPART(line));
}

void custom_trace_printer(uint16_t label, struct valuelist *removed, uint16_t nextlabel) {
	struct routine *r;
	struct zinstr *zi;

	r = make_routine(label, 1);

	zi = append_instr(r, Z_STORE);
	zi->oper[0] = SMALL(REG_LOCAL + 0);
	zi->oper[1] = VALUE(REG_TOP);

	while(removed) {
		zi = append_instr(r, Z_CALLVS);
		zi->oper[0] = ROUTINE(R_PUSH_PAIR_VV);
		zi->oper[1] = SMALL_OR_LARGE(removed->value);
		zi->oper[2] = VALUE(REG_A + 0);
		zi->store = REG_A + 0;
		removed = removed->next;
	}

	zi = append_instr(r, Z_CALL1N);
	zi->oper[0] = ROUTINE(nextlabel);

	zi = append_instr(r, Z_STORE);
	zi->oper[0] = SMALL(REG_TOP);
	zi->oper[1] = VALUE(REG_LOCAL + 0);

	zi = append_instr(r, Z_RFALSE);
}

int simple_constant_list(struct astnode *an) {
	while(an && an->kind == AN_PAIR) {
		if(an->children[0]->kind != AN_TAG
		&& an->children[0]->kind != AN_INTEGER
		&& an->children[0]->kind != AN_DICTWORD
		&& an->children[0]->kind != AN_EMPTY_LIST) {
			return 0;
		}
		an = an->children[1];
	}

	return an && an->kind == AN_EMPTY_LIST;
}

struct rev_lookup_entry {
	uint16_t			value;
	uint16_t			count;
	uint16_t			mapindex;
	uint16_t			label;
	struct rev_lookup_entry		*alias;
	uint16_t			*onumtable;
};

int cmp_rev_lookup_entry(const void *a, const void *b) {
	const struct rev_lookup_entry *aa = a;
	const struct rev_lookup_entry *bb = b;
	int diff;

	diff = aa->value - bb->value;
	if(!diff) diff = aa->mapindex - bb->mapindex;
	return diff;
}

int cmp_uint16(const void *a, const void *b) {
	const uint16_t *aa = a;
	const uint16_t *bb = b;

	return *aa - *bb;
}

static void compile_rev_lookup_index(struct rev_lookup_entry *table, int nentry, struct wordmap *map, struct routine *r) {
	struct zinstr *zi;
	int i, j, pos;
	uint16_t llnext;

	if(nentry >= TWEAK_BINSEARCH) {
		pos = nentry / 2;
		llnext = r->next_label++;
		zi = append_instr(r, Z_JGE);
		zi->oper[0] = VALUE(REG_TEMP);
		zi->oper[1] = LARGE(table[pos].value);
		zi->branch = llnext;
		compile_rev_lookup_index(table, pos, map, r);
		zi = append_instr(r, OP_LABEL(llnext));
		compile_rev_lookup_index(table + pos, nentry - pos, map, r);
	} else {
		for(i = 0; i < nentry; i++) {
			if(table[i].count == 0xffff) {
				zi = append_instr(r, Z_JE);
				zi->oper[0] = VALUE(REG_TEMP);
				zi->oper[1] = LARGE(table[i].value);
				zi->branch = RFALSE;
			} else {
				if(table[i].alias) {
					zi = append_instr(r, Z_JE);
					zi->oper[0] = VALUE(REG_TEMP);
					zi->oper[1] = LARGE(table[i].value);
					zi->branch = table[i].alias->label;
				} else {
					llnext = r->next_label++;
					zi = append_instr(r, Z_JNE);
					zi->oper[0] = VALUE(REG_TEMP);
					zi->oper[1] = LARGE(table[i].value);
					zi->branch = llnext;
					if(table[i].label) {
						zi = append_instr(r, OP_LABEL(table[i].label));
					}
					for(j = 0; j < table[i].count; ) {
						if(j + 3 <= table[i].count) {
							zi = append_instr(r, Z_CALLVN);
							zi->oper[0] = ROUTINE(R_AUX_PUSH3);
							zi->oper[1] = SMALL_OR_LARGE(1 + table[i].onumtable[j]); j++;
							zi->oper[2] = SMALL_OR_LARGE(1 + table[i].onumtable[j]); j++;
							zi->oper[3] = SMALL_OR_LARGE(1 + table[i].onumtable[j]); j++;
						} else if(j + 2 <= table[i].count) {
							zi = append_instr(r, Z_CALLVN);
							zi->oper[0] = ROUTINE(R_AUX_PUSH2);
							zi->oper[1] = SMALL_OR_LARGE(1 + table[i].onumtable[j]); j++;
							zi->oper[2] = SMALL_OR_LARGE(1 + table[i].onumtable[j]); j++;
						} else {
							zi = append_instr(r, Z_CALL2N);
							zi->oper[0] = ROUTINE(R_AUX_PUSH1);
							zi->oper[1] = SMALL_OR_LARGE(1 + table[i].onumtable[j]); j++;
						}
					}
					zi = append_instr(r, Z_RTRUE);
					zi = append_instr(r, OP_LABEL(llnext));
				}
			}
		}
		zi = append_instr(r, Z_RTRUE);
	}
}

static void compile_rev_lookup(struct program *prg, struct routine **rptr, struct wordmap *map) {
	struct zinstr *zi;
	struct routine *r = *rptr;
	int have_always = map->nmap && map->dict_ids[map->nmap - 1] == 256 + prg->ndictword;
	int nmap = map->nmap - have_always;
	struct rev_lookup_entry table[nmap];
	uint16_t labloop = make_routine_label();
	uint16_t labdispatch = make_routine_label();
	uint16_t labfoundloop = make_routine_label();
	uint16_t labnext = make_routine_label();
	uint16_t labend = make_routine_label();
	int i, j;
	int t = next_temp++;

	// a0 = output value
	// a1 = words iterator

	// v0 = object iterator

	// t0 = current word
	// t1 = current object

	zi = append_instr(r, Z_CALL2N);
	zi->oper[0] = ROUTINE(R_ALLOCATE);
	zi->oper[1] = SMALL(4 + 1 * 2);
	zi = append_instr(r, Z_RET);
	zi->oper[0] = ROUTINE(labloop);

	r = make_routine(labloop, 0);
	zi = append_instr(r, OP_LABEL(3));
	zi = append_instr(r, Z_CALL2S);
	zi->oper[0] = ROUTINE(R_DEREF);
	zi->oper[1] = VALUE(REG_A + 1);
	zi->store = REG_TEMP;
	zi = append_instr(r, Z_JNE);
	zi->oper[0] = VALUE(REG_TEMP);
	zi->oper[1] = VALUE(REG_NIL);
	zi->branch = 1;
	zi = append_instr(r, Z_RET);
	zi->oper[0] = ROUTINE(labend);
	zi = append_instr(r, OP_LABEL(1));

	zi = append_instr(r, Z_CALLVN);
	zi->oper[0] = ROUTINE(R_GET_PAIR_RR);
	zi->oper[1] = VALUE(REG_TEMP);
	zi->oper[2] = SMALL(REG_TEMP);
	zi->oper[3] = SMALL(REG_A + 1);
	zi = append_instr(r, Z_CALL2S);
	zi->oper[0] = ROUTINE(R_DEREF_UNBOX);
	zi->oper[1] = VALUE(REG_TEMP);
	zi->store = REG_TEMP;

	zi = append_instr(r, Z_CALL1N);
	zi->oper[0] = ROUTINE(R_COLLECT_BEGIN);

	zi = append_instr(r, Z_CALL1S);
	zi->oper[0] = ROUTINE(labdispatch);
	zi->store = REG_TEMP;

	// 1 = this word can only refer to the objects pushed (+ always)
	zi = append_instr(r, Z_JNZ);
	zi->oper[0] = VALUE(REG_TEMP);
	zi->branch = 2;

	// 0 = ignore this word
	zi = append_instr(r, Z_CALL1N);
	zi->oper[0] = ROUTINE(R_COLLECT_END);
	zi = append_instr(r, Z_JUMP);
	zi->oper[0] = REL_LABEL(3);

	zi = append_instr(r, OP_LABEL(2));
	if(have_always) {
		for(j = 0; j < map->objects[nmap].count; j++) {
			zi = append_instr(r, Z_CALL2N);
			zi->oper[0] = ROUTINE(R_AUX_PUSH1);
			zi->oper[1] = SMALL_OR_LARGE(1 + map->objects[nmap].onumtable[j]);
		}
	}
	zi = append_instr(r, Z_CALL2S);
	zi->oper[0] = ROUTINE(R_DEREF);
	zi->oper[1] = VALUE(REG_A + 0);
	zi->store = REG_TEMP;
	zi = append_instr(r, Z_JL);
	zi->oper[0] = VALUE(REG_TEMP);
	zi->oper[1] = SMALL(0);
	zi->branch = 4;

	zi = append_instr(r, Z_CALL2S);
	zi->oper[0] = ROUTINE(R_COLLECT_CHECK);
	zi->oper[1] = VALUE(REG_TEMP);
	zi->store = REG_TEMP;
	zi = append_instr(r, Z_JNZ);
	zi->oper[0] = VALUE(REG_TEMP);
	zi->branch = 3;
	zi = append_instr(r, Z_RFALSE);

	zi = append_instr(r, OP_LABEL(4));
	zi = append_instr(r, Z_CALL1S);
	zi->oper[0] = ROUTINE(R_COLLECT_END);
	zi->store = REG_TEMP;
	zi = append_instr(r, Z_STOREW);
	zi->oper[0] = VALUE(REG_ENV);
	zi->oper[1] = SMALL(2);
	zi->oper[2] = VALUE(REG_TEMP);
	zi = append_instr(r, Z_RET);
	zi->oper[0] = ROUTINE(labfoundloop);

	r = make_routine(labfoundloop, 0);
	zi = append_instr(r, Z_LOADW);
	zi->oper[0] = VALUE(REG_ENV);
	zi->oper[1] = SMALL(2);
	zi->store = REG_TEMP;
	zi = append_instr(r, Z_CALLVN);
	zi->oper[0] = ROUTINE(R_GET_PAIR_RR);
	zi->oper[1] = VALUE(REG_TEMP);
	zi->oper[2] = SMALL(REG_X + t);
	zi->oper[3] = SMALL(REG_TEMP);
	zi = append_instr(r, Z_STOREW);
	zi->oper[0] = VALUE(REG_ENV);
	zi->oper[1] = SMALL(2);
	zi->oper[2] = VALUE(REG_TEMP);

	zi = append_instr(r, Z_CALLVN);
	zi->oper[0] = ROUTINE(R_TRY_ME_ELSE);
	zi->oper[1] = SMALL((2 + CHOICE_SIZEOF) * 2);
	zi->oper[2] = ROUTINE(labnext);

	zi = append_instr(r, Z_CALLVN);
	zi->oper[0] = ROUTINE(R_UNIFY);
	zi->oper[1] = VALUE(REG_X + t);
	zi->oper[2] = VALUE(REG_A + 0);
	zi = append_instr(r, Z_RET);
	zi->oper[0] = ROUTINE(labloop);

	r = make_routine(labnext, 0);
	zi = append_instr(r, Z_CALL2N);
	zi->oper[0] = ROUTINE(R_TRUST_ME);
	zi->oper[1] = SMALL(2);
	zi = append_instr(r, Z_RET);
	zi->oper[0] = ROUTINE(labfoundloop);

	r = make_routine(labdispatch, 0);
	for(i = 0; i < nmap; i++) {
		table[i].mapindex = i;
		table[i].value = dict_id_tag(map->dict_ids[i]);
		if(map->objects[i].count > MAXWORDMAP) {
			table[i].count = 0xffff;
			table[i].onumtable = 0;
		} else {
			table[i].count = map->objects[i].count;
			table[i].onumtable = malloc(table[i].count * sizeof(uint16_t));
			memcpy(table[i].onumtable, map->objects[i].onumtable, table[i].count * sizeof(uint16_t));
		}
	}
	qsort(table, nmap, sizeof(struct rev_lookup_entry), cmp_rev_lookup_entry);
	for(i = 0; i < nmap - 1; ) {
		if(table[i].value == table[i + 1].value) {
			if(table[i].count == 0xffff || table[i + 1].count == 0xffff) {
				table[i].count = 0xffff;
				free(table[i].onumtable);
				free(table[i + 1].onumtable);
				table[i].onumtable = 0;
			} else {
				table[i].onumtable = realloc(table[i].onumtable, (table[i].count + table[i + 1].count) * sizeof(uint16_t));
				memcpy(table[i].onumtable + table[i].count, table[i + 1].onumtable, table[i + 1].count * sizeof(uint16_t));
				free(table[i + 1].onumtable);
				table[i].count += table[i + 1].count;
			}
			memmove(table + i + 1, table + i + 2, (nmap - i - 2) * sizeof(struct rev_lookup_entry));
			nmap--;
		} else {
			i++;
		}
	}
	for(i = nmap - 1; i >= 0; i--) {
		if(table[i].count != 0xffff) {
			qsort(table[i].onumtable, table[i].count, sizeof(uint16_t), cmp_uint16);
			for(j = 0; j < table[i].count - 1; ) {
				if(table[i].onumtable[j] == table[i].onumtable[j + 1]) {
					memmove(table[i].onumtable + j + 1, table[i].onumtable + j + 2, (table[i].count - j - 2) * sizeof(uint16_t));
					table[i].count--;
				} else {
					j++;
				}
			}
			for(j = nmap - 1; j > i; j--) {
				if(table[i].count == table[j].count
				&& !memcmp(table[i].onumtable, table[j].onumtable, table[i].count * sizeof(uint16_t))) {
					table[i].alias = &table[j];
					if(!table[j].label) {
						table[j].label = r->next_label++;
					}
					break;
				}
			}
			if(j == i) {
				table[i].alias = 0;
				table[i].label = 0;
			}
		}
	}

	compile_rev_lookup_index(table, nmap, map, r);

	for(i = 0; i < nmap; i++) {
		free(table[i].onumtable);
	}

	*rptr = r = make_routine(labend, 0);
	zi = append_instr(r, Z_CALL1N);
	zi->oper[0] = ROUTINE(R_DEALLOCATE);
}

static uint16_t tag_simple_value(struct astnode *an) {
	switch(an->kind) {
	case AN_TAG:
		return 1 + an->word->obj_id;
	case AN_EMPTY_LIST:
		return 0x1fff;
	case AN_DICTWORD:
		return dictword_tag(an->word);
	case AN_INTEGER:
		return 0x4000 + an->value;
	}
	//printf("line %d\n", LINEPART(an->line));
	assert(an->kind != AN_PAIR);
	assert(an->kind != AN_VARIABLE);
	assert(0);
	exit(1);
}

void compile_rule(struct astnode *an, struct routine **rptr, struct predicate *me_pred, int envflags, struct var *vars, int tail, struct program *prg) {
	int i, n;
	struct zinstr *zi;
	struct routine *r = *rptr;
	struct predname *predname = an->predicate;
	struct predicate *pred = predname->pred;
	struct backend_pred *bp = pred->backend;
	uint16_t cont_lab, call_lab = bp->global_label;
	uint32_t o1;
	struct astnode *sub;
	uint16_t lab1;
	struct scantable *st;

	if(!call_lab) printf("%s\n", predname->printed_name);
	assert(call_lab);

	if(predname->builtin == BI_IS_ONE_OF
	&& an->subkind == RULE_SIMPLE
	&& !an->children[0]->unbound) {
		if(an->children[1]->kind == AN_PAIR	// at least one element required
		&& simple_constant_list(an->children[1])) {
#if 0
			printf("optimised contains: ");
			pp_expr(an);
			printf("\n");
#endif
			lab1 = r->next_label++;
			o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
			zi = append_instr(r, Z_CALL2S);
			zi->oper[0] = ROUTINE(R_DEREF_UNBOX);
			zi->oper[1] = o1;
			zi->store = REG_TEMP;
			for(sub = an->children[1]; sub->kind == AN_PAIR; sub = sub->children[1]) {
				if(zi->op == Z_JE && !zi->oper[2]) {
					zi->oper[2] = compile_ast_to_oper(r, sub->children[0], vars, 0);
				} else if(zi->op == Z_JE && !zi->oper[3]) {
					zi->oper[3] = compile_ast_to_oper(r, sub->children[0], vars, 0);
				} else {
					zi = append_instr(r, Z_JE);
					zi->oper[0] = VALUE(REG_TEMP);
					zi->oper[1] = compile_ast_to_oper(r, sub->children[0], vars, 0);
					zi->branch = lab1;
				}
			}
			assert(zi->op == Z_JE);
			zi->op = Z_JNE;
			zi->branch = RFALSE;
			zi = append_instr(r, OP_LABEL(lab1));
			if(tail) {
				assert(!an->next_in_body);
				compile_body(0, rptr, me_pred, envflags, vars, tail, prg);
			}
			return;
		} else {
			call_lab = R_CONTAINSCHK_PRED;
		}
	}

	if(pred->flags & PREDF_FAIL) {
#if 1
		report(LVL_DEBUG, an->line, "Eliminating %srule call to %s",
			tail? "" : "non-tail ",
			an->predicate->printed_name);
#endif
		if(tail) {
			zi = append_instr(r, Z_RFALSE);
		} else {
			zi = append_instr(r, Z_JZ);	// don't confuse txd by dead code
			zi->oper[0] = SMALL(0);
			zi->branch = RFALSE;
		}
		return;
	}

	if(predname->builtin == BI_SPLIT
	&& an->children[1]->kind == AN_PAIR	// at least one element required
	&& simple_constant_list(an->children[1])) {
#if 0
		printf("optimised split: ");
		pp_expr(an);
		printf("\n");
#endif
		n = 0;
		for(sub = an->children[1]; sub->kind == AN_PAIR; sub = sub->children[1]) {
			n++;
		}
		scantable = realloc(scantable, (nscantable + 1) * sizeof(struct scantable));
		st = &scantable[nscantable++];

		st->label = make_global_label();
		st->length = n;
		st->value = malloc(n * sizeof(uint16_t));

		n = 0;
		for(sub = an->children[1]; sub->kind == AN_PAIR; sub = sub->children[1]) {
			st->value[n++] = tag_simple_value(sub->children[0]);
		}

		compile_put_ast_in_reg(r, an->children[0], REG_A + 0, vars);
		zi = append_instr(r, Z_STORE);
		zi->oper[0] = SMALL(REG_A+1);
		zi->oper[1] = REF(st->label);
		zi = append_instr(r, Z_STORE);
		zi->oper[0] = SMALL(REG_A+2);
		zi->oper[1] = SMALL(st->length);
		compile_put_ast_in_reg(r, an->children[2], REG_A + 3, vars);
		compile_put_ast_in_reg(r, an->children[3], REG_A + 4, vars);
		call_lab = R_SPLIT_SPECIAL_PRED;
	} else {
		for(i = 0; i < an->nchild; i++) {
			compile_put_ast_in_reg(r, an->children[i], REG_A + i, vars);
		}

		if(bp->trace_output_label) {
			add_trace_code(r, R_TRACE_QUERY, bp->trace_output_label, an->line);
		}
	}

	if(tail == TAIL_CONT) {
		assert(!an->next_in_body);

		// TAIL_CONT indicates that we're at the end of the clause.
		compile_deallocate(r, envflags);

		if(an->subkind == RULE_MULTI) {
			// multi-call in tail position

			if((pred->flags & PREDF_INVOKED_MULTI)
			&& (pred->flags & PREDF_INVOKED_SIMPLE)) {
				// Callee expects SIMPLEREF to be valid.
				if(!(envflags & ENVF_CAN_BE_MULTI)) {
					zi = append_instr(r, Z_STORE);
					zi->oper[0] = SMALL(REG_SIMPLEREF);
					zi->oper[1] = SMALL(REG_SIMPLE);
				} else if(!(envflags & ENVF_CAN_BE_SIMPLE)) {
					zi = append_instr(r, Z_STORE);
					zi->oper[0] = SMALL(REG_SIMPLEREF);
					zi->oper[1] = SMALL(REG_CHOICE);
				} // otherwise it is already valid
			}
			zi = append_instr(r, Z_RET);
			zi->oper[0] = ROUTINE(call_lab);
		} else {
			// simple call in tail position

			// If the caller is a multi-call, copy CHOICE into SIMPLE.
			// This sets up a cut for later.
			if(envflags & ENVF_CAN_BE_MULTI) {
				if(envflags & ENVF_CAN_BE_SIMPLE) {
					zi = append_instr(r, Z_LOAD);
					zi->oper[0] = VALUE(REG_SIMPLEREF);
					zi->store = REG_SIMPLE;
				} else {
					zi = append_instr(r, Z_STORE);
					zi->oper[0] = SMALL(REG_SIMPLE);
					zi->oper[1] = VALUE(REG_CHOICE);
				}
			}

			if((pred->flags & PREDF_INVOKED_MULTI)
			&& (pred->flags & PREDF_INVOKED_SIMPLE)) {
				// Callee expects SIMPLEREF to be valid.
				zi = append_instr(r, Z_STORE);
				zi->oper[0] = SMALL(REG_SIMPLEREF);
				zi->oper[1] = SMALL(REG_SIMPLE);
			}
			zi = append_instr(r, Z_RET);
			zi->oper[0] = ROUTINE(call_lab);
		}
	} else {
		if(tail) {
			// We're somewhere in the middle of a clause, e.g. at the end
			// of a then-body that's not itself in tail position. So this
			// is actually a non-tail call, and we don't deallocate the env.

			assert(!an->next_in_body);
			cont_lab = tail;
		} else {
			cont_lab = make_routine_label();
		}
	
		if(an->subkind == RULE_MULTI) {
			// multi-call in non-tail position

			if((pred->flags & PREDF_INVOKED_MULTI)
			&& (pred->flags & PREDF_INVOKED_SIMPLE)) {
				// Callee expects SIMPLEREF to be valid.
				zi = append_instr(r, Z_STORE);
				zi->oper[0] = SMALL(REG_SIMPLEREF);
				zi->oper[1] = SMALL(REG_CHOICE);
			}
		} else {
			// simple call in non-tail position

			if((pred->flags & PREDF_INVOKED_MULTI)
			&& (pred->flags & PREDF_INVOKED_SIMPLE)) {
				// Callee expects SIMPLEREF to be valid.
				zi = append_instr(r, Z_STORE);
				zi->oper[0] = SMALL(REG_SIMPLEREF);
				zi->oper[1] = SMALL(REG_SIMPLE);
			}
			zi = append_instr(r, Z_STORE);
			zi->oper[0] = SMALL(REG_SIMPLE);
			zi->oper[1] = VALUE(REG_CHOICE);
		}

		zi = append_instr(r, Z_STORE);
		zi->oper[0] = SMALL(REG_CONT);
		zi->oper[1] = ROUTINE(cont_lab);
		zi = append_instr(r, Z_RET);
		zi->oper[0] = ROUTINE(call_lab);

		if(!tail) {
			clear_lingering(vars);
			r = *rptr = make_routine(cont_lab, 0);
		}
	}
}

int is_simple_kind(int kind) {
	return kind == AN_TAG || kind == AN_INTEGER || kind == AN_DICTWORD || kind == AN_EMPTY_LIST;
}

// A trivial condition can be evaluated without calling predicates, binding
// variables, creating choice points, or clobbering the argument registers.

struct astnode *strip_trivial_conditions(struct astnode *an);

int is_trivial_condition(struct astnode *an) {
	if(an->kind == AN_RULE && (an->predicate->pred->flags & PREDF_FAIL)) {
		return 1;
	}
	if((an->kind == AN_RULE || an->kind == AN_NEG_RULE) && (an->predicate->builtin == BI_HAVE_UNDO)) {
		return 1;
	}
	if((an->kind == AN_RULE || an->kind == AN_NEG_RULE)
	&& an->subkind == RULE_SIMPLE
	&& an->predicate->pred->dynamic) {
		if(an->predicate->arity == 0) {
			return 1;
		}
		if(an->predicate->arity == 1
		&& !an->children[0]->unbound
		&& !(an->predicate->pred->flags & PREDF_GLOBAL_VAR)) {
			return 1;
		}
		if(an->predicate->builtin == BI_HASPARENT
		&& !an->children[0]->unbound
		&& !an->children[1]->unbound) {
			return 1;
		}
	}
	if((an->kind == AN_RULE || an->kind == AN_NEG_RULE)
	&& an->predicate->builtin == BI_UNIFY) {
		if((is_simple_kind(an->children[0]->kind) && !an->children[1]->unbound)
		|| (is_simple_kind(an->children[1]->kind) && !an->children[0]->unbound)) {
			return 1;
		}
	}
	if(an->kind == AN_BLOCK || an->kind == AN_FIRSTRESULT) {
		if(!strip_trivial_conditions(an->children[0])) {
			return 1;
		}
	}
#if 0
	if(an->kind == AN_OR) {
		int i;

		for(i = 0; i < an->nchild; i++) {
			if(strip_trivial_conditions(an->children[i])) break;
		}
		if(i == an->nchild) {
			printf("trivial disjunction ");
			pp_expr(an);
			printf("\n");
		}
	}
#endif
	return 0;
}

void compile_trivial_conditions(struct routine *r, struct astnode *an, uint16_t failure, struct var *vars);

void compile_trivial_condition(struct routine *r, struct astnode *an, uint16_t failure, struct var *vars) {
	struct zinstr *zi;
	struct backend_pred *bp;
	uint32_t o1, o2;
	uint16_t ll;
	int treg;

	if(an->kind == AN_RULE && (an->predicate->pred->flags & PREDF_FAIL)) {
		if(failure == RFALSE) {
			zi = append_instr(r, Z_RFALSE);
		} else {
			zi = append_instr(r, Z_JUMP);
			zi->oper[0] = REL_LABEL(failure);
		}
		return;
	}

	if((an->kind == AN_RULE || an->kind == AN_NEG_RULE) && (an->predicate->builtin == BI_HAVE_UNDO)) {
		zi = append_instr(r, Z_LOADB);
		zi->oper[0] = SMALL(0);
		zi->oper[1] = SMALL(0x11);
		zi->store = REG_TEMP;
		zi = append_instr(r, (an->kind == AN_RULE)? Z_TESTN : Z_TEST);
		zi->oper[0] = VALUE(REG_TEMP);
		zi->oper[1] = SMALL(0x10);
		zi->branch = failure;
		return;
	}

	if((an->kind == AN_RULE || an->kind == AN_NEG_RULE)
	&& an->subkind == RULE_SIMPLE
	&& an->predicate->pred->dynamic) {
		bp = an->predicate->pred->backend;

		if(an->predicate->arity == 0) {
			zi = append_instr(r, (an->kind == AN_RULE)? Z_TESTN : Z_TEST);
			zi->oper[0] = USERGLOBAL(bp->user_global);
			zi->oper[1] = SMALL_OR_LARGE(bp->user_flag_mask);
			zi->branch = failure;
		} else if(an->predicate->arity == 1
		&& !an->children[0]->unbound
		&& !(an->predicate->pred->flags & PREDF_GLOBAL_VAR)) {
			if(an->children[0]->kind == AN_TAG && bp->object_flag < NZOBJFLAG) {
				o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
				zi = append_instr(r, (an->kind == AN_RULE)? Z_JNA : Z_JA);
				zi->oper[0] = o1;
				zi->oper[1] = SMALL(bp->object_flag);
				zi->branch = failure;
			} else if(bp->object_flag >= NZOBJFLAG) {
				o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
				zi = append_instr(r, Z_CALL2S);
				zi->oper[0] = ROUTINE(extflagreaders[(bp->object_flag - NZOBJFLAG) / 8]);
				zi->oper[1] = o1;
				zi->store = REG_TEMP;
				zi = append_instr(r, (an->kind == AN_RULE)? Z_TESTN : Z_TEST);
				zi->oper[0] = VALUE(REG_TEMP);
				zi->oper[1] = SMALL(0x80 >> ((bp->object_flag - NZOBJFLAG) & 7));
				zi->branch = failure;
			} else {
				o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
				zi = append_instr(r, Z_CALLVS);
				zi->oper[0] = ROUTINE(R_READ_FLAG);
				zi->oper[1] = o1;
				zi->oper[2] = SMALL(bp->object_flag);
				zi->store = REG_TEMP;
				zi = append_instr(r, (an->kind == AN_RULE)? Z_JZ : Z_JNZ);
				zi->oper[0] = VALUE(REG_TEMP);
				zi->branch = failure;
			}
		} else if(an->predicate->builtin == BI_HASPARENT) {
			assert(!an->children[0]->unbound);
			assert(!an->children[1]->unbound);
			ll = r->next_label++;
			o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
			if(an->children[0]->kind != AN_TAG) {
				zi = append_instr(r, Z_CALL2S);
				zi->oper[0] = ROUTINE(R_DEREF_OBJ);
				zi->oper[1] = o1;
				zi->store = REG_TEMP;
				zi = append_instr(r, Z_JZ);
				zi->oper[0] = VALUE(REG_TEMP);
				if(an->kind == AN_NEG_RULE) {
					zi->branch = ll;
				} else {
					zi->branch = failure;
				}
				o1 = VALUE(REG_TEMP);
			}
			o2 = compile_ast_to_oper(r, an->children[1], vars, 0);
			if(an->children[1]->kind != AN_TAG) {
				treg = (o1 == VALUE(REG_TEMP))? (REG_X + next_temp++) : REG_TEMP;
				zi = append_instr(r, Z_CALL2S);
				zi->oper[0] = ROUTINE(R_DEREF_OBJ);
				zi->oper[1] = o2;
				zi->store = treg;
				zi = append_instr(r, Z_JZ);
				zi->oper[0] = VALUE(treg);
				if(an->kind == AN_NEG_RULE) {
					zi->branch = ll;
				} else {
					zi->branch = failure;
				}
				o2 = VALUE(treg);
			}
			zi = append_instr(r, (an->kind == AN_NEG_RULE)? Z_JIN : Z_JIN_N);
			zi->oper[0] = o1;
			zi->oper[1] = o2;
			zi->branch = failure;
			zi = append_instr(r, OP_LABEL(ll));
		} else {
			assert(0);
			exit(1);
		}
	} else if((an->kind == AN_RULE || an->kind == AN_NEG_RULE)
	&& an->predicate->builtin == BI_UNIFY) {
		if(is_simple_kind(an->children[0]->kind) && !an->children[1]->unbound) {
			o1 = compile_ast_to_oper(r, an->children[1], vars, 0);
			o2 = compile_ast_to_oper(r, an->children[0], vars, 0);
		} else if(is_simple_kind(an->children[1]->kind) && !an->children[0]->unbound) {
			o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
			o2 = compile_ast_to_oper(r, an->children[1], vars, 0);
		} else {
			assert(0);
			exit(1);
		}
		zi = append_instr(r, Z_CALL2S);
		zi->oper[0] = ROUTINE(R_DEREF_UNBOX);
		zi->oper[1] = o1;
		zi->store = REG_TEMP;
		zi = append_instr(r, (an->kind == AN_RULE)? Z_JNE : Z_JE);
		zi->oper[0] = VALUE(REG_TEMP);
		zi->oper[1] = o2;
		zi->branch = failure;
	} else if(an->kind == AN_BLOCK || an->kind == AN_FIRSTRESULT) {
		compile_trivial_conditions(r, an->children[0], failure, vars);
	} else {
		assert(0);
		exit(1);
	}
}

void compile_trivial_conditions(struct routine *r, struct astnode *an, uint16_t failure, struct var *vars) {
	uint8_t *stash;

	if(an && is_trivial_condition(an)) {
		compile_trivial_condition(r, an, failure, vars);
		an = an->next_in_body;
	}

	stash = stash_lingering(vars);

	while(an && is_trivial_condition(an)) {
		compile_trivial_condition(r, an, failure, vars);
		an = an->next_in_body;
	}

	reapply_lingering(vars, stash);
	free(stash);
}

struct astnode *strip_trivial_conditions(struct astnode *an) {
	while(an && is_trivial_condition(an)) {
		an = an->next_in_body;
	}
	return an;
}

int only_simple_builtins(struct astnode *an) {
	while(an) {
		if(!is_simple_builtin(an, 0)) {
			return 0;
		}
		an = an->next_in_body;
	}

	return 1;
}

void compile_if(struct astnode *an, struct routine **rptr, struct predicate *pred, int envflags, struct var *vars, int tail, struct program *prg) {
	struct routine *r = *rptr;
	struct zinstr *zi;
	uint16_t elselab = 0xffff, endlab = 0xffff;
	struct var *v;
	struct astnode *more;
	uint8_t *stash;

	if(an->children[0]
	&& an->children[0]->kind == AN_RULE
	&& (an->children[0]->predicate->pred->flags & PREDF_FAIL)) {
		compile_body(an->children[2], rptr, pred, envflags, vars, tail, prg);
	} else if(body_succeeds(an->children[0])) {
		for(v = vars; v; v = v->next) {
			if(v->name == an->word) break;
		}
		assert(v);
		zi = append_instr(r, Z_STOREW);
		zi->oper[0] = VALUE(REG_ENV);
		zi->oper[1] = SMALL(2 + v->slot);
		zi->oper[2] = VALUE(REG_CHOICE);
		compile_body(an->children[0], rptr, pred, envflags, vars, 0, prg);
		r = *rptr;
		zi = append_instr(r, Z_LOADW);
		zi->oper[0] = VALUE(REG_ENV);
		zi->oper[1] = SMALL(2 + v->slot);
		zi->store = REG_CHOICE;
		compile_body(an->children[1], rptr, pred, envflags, vars, tail, prg);
	} else {
		ensure_vars_exist(an, r, vars, 1);

		more = strip_trivial_conditions(an->children[0]);
		if(!more) {
#if 0
			printf("Trivial conditions detected: ");
			pp_body(an->children[0]);
			printf("\n");
#endif
			if(only_simple_builtins(an->children[1])
			&& only_simple_builtins(an->children[2])) {
				elselab = r->next_label++;
				endlab = r->next_label++;
				compile_trivial_conditions(r, an->children[0], elselab, vars);
				stash = stash_lingering(vars);
				compile_body(an->children[1], rptr, pred, envflags, vars, 0, prg);
				assert(r == *rptr);
				if(an->children[2]) {
					zi = append_instr(r, Z_JUMP);
					zi->oper[0] = REL_LABEL(endlab);
				}
				zi = append_instr(r, OP_LABEL(elselab));
				reapply_lingering(vars, stash);
				compile_body(an->children[2], rptr, pred, envflags, vars, 0, prg);
				assert(r == *rptr);
				zi = append_instr(r, OP_LABEL(endlab));
				reapply_lingering(vars, stash);
				free(stash);
				compile_body(0, rptr, pred, envflags, vars, tail, prg);
			} else {
				elselab = r->next_label++;
				if(!tail) endlab = make_routine_label();
				compile_trivial_conditions(r, an->children[0], elselab, vars);
				stash = stash_lingering(vars);
				compile_body(an->children[1], rptr, pred, envflags, vars, tail? tail : endlab, prg);
				// rptr may have changed, r remains
				zi = append_instr(r, OP_LABEL(elselab));
				reapply_lingering(vars, stash);
				free(stash);
				compile_body(an->children[2], &r, pred, envflags, vars, tail? tail : endlab, prg);
				if(!tail) {
					clear_lingering(vars);
					r = *rptr = make_routine(endlab, 0);
				}
			}
		} else {
			for(v = vars; v; v = v->next) {
				if(v->name == an->word) break;
			}

			if(!tail) endlab = make_routine_label();
			elselab = make_routine_label();

			zi = append_instr(r, Z_STOREW);
			zi->oper[0] = VALUE(REG_ENV);
			zi->oper[1] = SMALL(2 + v->slot);
			zi->oper[2] = VALUE(REG_CHOICE);
			zi = append_instr(r, Z_CALL2N);
			zi->oper[0] = ROUTINE(R_TRY_ME_ELSE_0);
			zi->oper[1] = ROUTINE(elselab);
			compile_body(an->children[0], rptr, pred, envflags, vars, 0, prg);
			r = *rptr;
			zi = append_instr(r, Z_LOADW);
			zi->oper[0] = VALUE(REG_ENV);
			zi->oper[1] = SMALL(2 + v->slot);
			zi->store = REG_CHOICE;
			compile_body(an->children[1], rptr, pred, envflags, vars, tail? tail : endlab, prg);

			clear_lingering(vars);
			r = *rptr = make_routine(elselab, 0);
			zi = append_instr(r, Z_CALL1N);
			zi->oper[0] = ROUTINE(R_TRUST_ME_0);
			compile_body(an->children[2], rptr, pred, envflags, vars, tail? tail : endlab, prg);

			if(!tail) {
				clear_lingering(vars);
				r = *rptr = make_routine(endlab, 0);
			}
		}
	}
}

void compile_neg(struct astnode *an, struct routine **rptr, struct predicate *pred, int envflags, struct var *vars, struct program *prg) {
	struct routine *r = *rptr;
	struct zinstr *zi;
	struct var *v;
	uint16_t lab;

	if(an->kind == AN_NEG_RULE
	&& (an->predicate->pred->flags & PREDF_FAIL)) {
		/* do nothing */
	} else {
		for(v = vars; v; v = v->next) {
			if(v->name == an->word) break;
		}

		ensure_vars_exist(an, r, vars, 0);

		lab = make_routine_label();

		zi = append_instr(r, Z_STOREW);
		zi->oper[0] = VALUE(REG_ENV);
		zi->oper[1] = SMALL(2 + v->slot);
		zi->oper[2] = VALUE(REG_CHOICE);
		zi = append_instr(r, Z_CALL2N);
		zi->oper[0] = ROUTINE(R_TRY_ME_ELSE_0);
		zi->oper[1] = ROUTINE(lab);
		if(an->kind == AN_NEG_BLOCK) {
			compile_body(an->children[0], rptr, pred, envflags, vars, 0, prg);
		} else {
			if(is_simple_builtin(an, 1)) {
				compile_simple_builtin(r, an, pred, envflags, vars, prg);
			} else {
				compile_rule(an, rptr, pred, envflags, vars, 0, prg);
			}
		}
		r = *rptr;
		zi = append_instr(r, Z_LOADW);
		zi->oper[0] = VALUE(REG_ENV);
		zi->oper[1] = SMALL(2 + v->slot);
		zi->store = REG_CHOICE;
		zi = append_instr(r, Z_RFALSE);

		clear_lingering(vars);
		r = *rptr = make_routine(lab, 0);
		zi = append_instr(r, Z_CALL1N);
		zi->oper[0] = ROUTINE(R_TRUST_ME_0);
	}
}

void compile_disjunction(struct astnode *an, struct routine **rptr, struct predicate *pred, int envflags, struct var *vars, int tail, struct program *prg) {
	struct routine *r = *rptr;
	struct zinstr *zi;
	int i;
	uint16_t lab = 0xffff, endlab = 0xffff;
	struct astnode **table;
	int nfork;

	ensure_vars_exist(an, r, vars, 1);

	if(!tail) endlab = make_routine_label();

	nfork = an->nchild;
	table = malloc(nfork * sizeof(struct astnode *));
	memcpy(table, an->children, nfork * sizeof(struct astnode *));

	for(i = 0; i < nfork; i++) {
		if(table[i]
		&& table[i]->kind == AN_RULE
		&& (table[i]->predicate->pred->flags & PREDF_FAIL)) {
			memmove(table + i, table + i + 1, (nfork - i - 1) * sizeof(struct astnode *));
			nfork--;
			i--;
		}
	}

	if(nfork == 0) {
		if(tail) {
			zi = append_instr(r, Z_RFALSE);
		} else {
			zi = append_instr(r, Z_JZ);	// don't confuse txd by dead code
			zi->oper[0] = SMALL(0);
			zi->branch = RFALSE;
		}
	} else {
		for(i = 0; i < nfork; i++) {
			if(i == 0 && i < nfork - 1) {
				lab = make_routine_label();
				zi = append_instr(r, Z_CALL2N);
				zi->oper[0] = ROUTINE(R_TRY_ME_ELSE_0);
				zi->oper[1] = ROUTINE(lab);
			} else if(i < nfork - 1) {
				lab = make_routine_label();
				zi = append_instr(r, Z_CALL2N);
				zi->oper[0] = ROUTINE(R_RETRY_ME_ELSE_0);
				zi->oper[1] = ROUTINE(lab);
			} else if(i != 0) {
				zi = append_instr(r, Z_CALL1N);
				zi->oper[0] = ROUTINE(R_TRUST_ME_0);
			}
			compile_body(table[i], rptr, pred, envflags, vars, tail? tail : endlab, prg);
			if(i < nfork - 1) {
				clear_lingering(vars);
				r = *rptr = make_routine(lab, 0);
			} else if(!tail) {
				clear_lingering(vars);
				r = *rptr = make_routine(endlab, 0);
			}
		}
	}

	free(table);
}

void compile_exhaust(struct astnode *an, struct routine **rptr, struct predicate *pred, int envflags, struct var *vars, struct program *prg) {
	struct routine *r = *rptr;
	struct zinstr *zi;
	uint16_t lab;

	ensure_vars_exist(an->children[0], r, vars, 0);

	lab = make_routine_label();
	zi = append_instr(r, Z_CALL2N);
	zi->oper[0] = ROUTINE(R_TRY_ME_ELSE_0);
	zi->oper[1] = ROUTINE(lab);
	compile_body(an->children[0], rptr, pred, envflags, vars, ROUTINE(R_FAIL_PRED), prg);

	clear_lingering(vars);
	r = *rptr = make_routine(lab, 0);
	zi = append_instr(r, Z_CALL1N);
	zi->oper[0] = ROUTINE(R_TRUST_ME_0);
}

void compile_firstresult(struct astnode *an, struct routine **rptr, struct predicate *pred, int envflags, struct var *vars, struct program *prg) {
	struct routine *r = *rptr;
	struct zinstr *zi;
	struct var *v;
	struct astnode *sub;

	assert(envflags & ENVF_ENV);

	for(v = vars; v; v = v->next) {
		if(v->name == an->word) break;
	}
	assert(v);

	for(sub = an->children[0]; sub; sub = sub->next_in_body) {
		if((sub->kind == AN_RULE && sub->subkind == RULE_SIMPLE)
		|| (sub->kind == AN_NEG_RULE)
		|| is_simple_builtin(sub, 0)) {
			/* known to succeed at most once */
		} else {
			break;
		}
	}

#if 0
	if(!sub) {
		printf("at most one: ");
		pp_body(an->children[0]);
		printf("\n");
	} else {
		printf("possibly multi: ");
		pp_body(an->children[0]);
		printf(" (%s:%d)\n", FILEPART(an->line), LINEPART(an->line));
	}
#endif

	// todo tail position
	// todo just one multiquery

	if(!sub) {
		compile_body(an->children[0], rptr, pred, envflags, vars, 0, prg);
	} else {
		zi = append_instr(r, Z_STOREW);
		zi->oper[0] = VALUE(REG_ENV);
		zi->oper[1] = SMALL(2 + v->slot);
		zi->oper[2] = VALUE(REG_CHOICE);
		compile_body(an->children[0], rptr, pred, envflags, vars, 0, prg);
		r = *rptr;
		zi = append_instr(r, Z_LOADW);
		zi->oper[0] = VALUE(REG_ENV);
		zi->oper[1] = SMALL(2 + v->slot);
		zi->store = REG_CHOICE;
	}
}

void compile_collect(struct astnode *an, struct routine **rptr, struct predicate *pred, int envflags, struct var *vars, struct program *prg) {
	struct routine *r = *rptr;
	struct zinstr *zi;
	uint16_t lab;
	uint32_t o1, o2;

	ensure_vars_exist(an, r, vars, 1);

	if(an->children[0]
	&& an->children[0]->kind == AN_RULE
	&& (an->children[0]->predicate->pred->flags & PREDF_FAIL)) {
		o2 = compile_ast_to_oper(r, an->children[2], vars, 0);
		zi = append_instr(r, Z_CALLVN);
		zi->oper[0] = VALUE(REG_R_USIMPLE);
		zi->oper[1] = VALUE(REG_NIL);
		zi->oper[2] = o2;
	} else {
		zi = append_instr(r, Z_CALL1N);
		zi->oper[0] = ROUTINE(R_COLLECT_BEGIN);
		lab = make_routine_label();
		zi = append_instr(r, Z_CALL2N);
		zi->oper[0] = ROUTINE(R_TRY_ME_ELSE_0);
		zi->oper[1] = ROUTINE(lab);
		compile_body(an->children[0], rptr, pred, envflags, vars, 0, prg);
		r = *rptr;
		o1 = compile_ast_to_oper(r, an->children[1], vars, 1);
		zi = append_instr(r, Z_CALL2N);
		zi->oper[0] = ROUTINE(R_COLLECT_PUSH);
		zi->oper[1] = o1;
		zi = append_instr(r, Z_RFALSE);

		clear_lingering(vars);
		r = *rptr = make_routine(lab, 0);
		zi = append_instr(r, Z_CALL1N);
		zi->oper[0] = ROUTINE(R_TRUST_ME_0);
		o2 = compile_ast_to_oper(r, an->children[2], vars, 0);
		zi = append_instr(r, Z_CALL1S);
		zi->oper[0] = ROUTINE(R_COLLECT_END);
		zi->store = REG_TEMP;
		zi = append_instr(r, Z_CALLVN);
		zi->oper[0] = ROUTINE(R_UNIFY);
		zi->oper[1] = VALUE(REG_TEMP);
		zi->oper[2] = o2;
	}
}

void compile_collect_words(struct astnode *an, struct routine **rptr, struct predicate *pred, int envflags, struct var *vars, struct program *prg) {
	struct routine *r = *rptr;
	struct zinstr *zi;
	uint16_t lab;
	uint32_t o1;

	ensure_vars_exist(an, r, vars, 1);

	if(an->children[0]
	&& an->children[0]->kind == AN_RULE
	&& (an->children[0]->predicate->pred->flags & PREDF_FAIL)) {
		o1 = compile_ast_to_oper(r, an->children[1], vars, 0);
		zi = append_instr(r, Z_CALLVN);
		zi->oper[0] = VALUE(REG_R_USIMPLE);
		zi->oper[1] = VALUE(REG_NIL);
		zi->oper[2] = o1;
	} else {
		zi = append_instr(r, Z_CALL1N);
		zi->oper[0] = ROUTINE(R_COLLECT_BEGIN);
		lab = make_routine_label();

		zi = append_instr(r, Z_INC);
		zi->oper[0] = SMALL(REG_FORWORDS);
		zi = append_instr(r, Z_CALL2N);
		zi->oper[0] = ROUTINE(R_TRY_ME_ELSE_0);
		zi->oper[1] = ROUTINE(lab);
		compile_body(an->children[0], rptr, pred, envflags | ENVF_MAYBE_FOR_WORDS, vars, 0, prg);
		r = *rptr;
		zi = append_instr(r, Z_RFALSE);

		clear_lingering(vars);
		r = *rptr = make_routine(lab, 0);
		zi = append_instr(r, Z_CALL1N);
		zi->oper[0] = ROUTINE(R_TRUST_ME_0);
		zi = append_instr(r, Z_DEC);
		zi->oper[0] = SMALL(REG_FORWORDS);

		o1 = compile_ast_to_oper(r, an->children[1], vars, 0);
		zi = append_instr(r, Z_CALL1S);
		zi->oper[0] = ROUTINE(R_COLLECT_END);
		zi->store = REG_TEMP;
		zi = append_instr(r, Z_CALLVN);
		zi->oper[0] = ROUTINE(R_UNIFY);
		zi->oper[1] = VALUE(REG_TEMP);
		zi->oper[2] = o1;
	}
}

void compile_determine_obj(struct astnode *an, struct routine **rptr, struct predicate *pred, int envflags, struct var *vars, struct program *prg) {
	struct routine *r = *rptr;
	struct zinstr *zi;
	uint16_t lab;
	uint32_t o1;

	ensure_vars_exist(an, r, vars, 0);

	if(an->value >= 0) {
		assert(an->value < pred->nwordmap);
		compile_put_ast_in_reg(r, an->children[0], REG_A + 0, vars);
		compile_put_ast_in_reg(r, an->children[3], REG_A + 1, vars);
		compile_rev_lookup(prg, rptr, &pred->wordmaps[an->value]);
	}

	compile_body(an->children[1], rptr, pred, envflags, vars, 0, prg);
	r = *rptr;

	zi = append_instr(r, Z_CALL1N);
	zi->oper[0] = ROUTINE(R_COLLECT_BEGIN);
	zi = append_instr(r, Z_INC);
	zi->oper[0] = SMALL(REG_FORWORDS);

	lab = make_routine_label();
	zi = append_instr(r, Z_CALL2N);
	zi->oper[0] = ROUTINE(R_TRY_ME_ELSE_0);
	zi->oper[1] = ROUTINE(lab);
	compile_body(an->children[2], rptr, pred, envflags, vars, 0, prg);
	r = *rptr;
	zi = append_instr(r, Z_RFALSE);

	clear_lingering(vars);
	r = *rptr = make_routine(lab, 0);
	zi = append_instr(r, Z_CALL1N);
	zi->oper[0] = ROUTINE(R_TRUST_ME_0);
	zi = append_instr(r, Z_DEC);
	zi->oper[0] = SMALL(REG_FORWORDS);
	o1 = compile_ast_to_oper(r, an->children[3], vars, 0);
	zi = append_instr(r, Z_CALL2N);
	zi->oper[0] = ROUTINE(R_COLLECT_MATCH_ALL);
	zi->oper[1] = o1;
}

void compile_select(struct astnode *an, struct routine **rptr, struct predicate *pred, int envflags, struct var *vars, int tail, struct program *prg) {
	int i;
	struct routine *r = *rptr;
	struct zinstr *zi;
	uint16_t endlab = 0xffff;
	uint16_t ll;

	ensure_vars_exist(an, r, vars, 1);

	if(an->nchild == 1) {
		compile_body(an->children[0], rptr, pred, envflags, vars, tail, prg);
		return;
	}

	if(!tail) endlab = make_routine_label();

	assert(envflags & ENVF_ENV);

	if(an->subkind == SEL_STOPPING && an->nchild == 2) {
		uint16_t flagmask, flagglobal;

		flagglobal = alloc_user_flag(&flagmask);

		ll = r->next_label++;
		zi = append_instr(r, Z_TEST);
		zi->oper[0] = USERGLOBAL(flagglobal);
		zi->oper[1] = SMALL_OR_LARGE(flagmask);
		zi->branch = ll;

		zi = append_instr(r, Z_OR);
		zi->oper[0] = USERGLOBAL(flagglobal);
		zi->oper[1] = SMALL_OR_LARGE(flagmask);
		zi->store = DEST_USERGLOBAL(flagglobal);

		compile_body(an->children[0], rptr, pred, envflags, vars, tail? tail : endlab, prg);
		// rptr may change, r remains
		zi = append_instr(r, OP_LABEL(ll));
		clear_lingering(vars);
		compile_body(an->children[1], rptr, pred, envflags, vars, tail? tail : endlab, prg);
	} else {
		if(an->subkind == SEL_STOPPING) {
			int sel = next_select++;

			zi = append_instr(r, Z_CALLVS);
			zi->oper[0] = ROUTINE(R_SEL_STOPPING);
			zi->oper[1] = SMALL(sel);
			zi->oper[2] = SMALL(an->nchild - 1);
			zi->store = REG_TEMP;
		} else if(an->subkind == SEL_RANDOM) {
			int sel = next_select++;

			if(an->nchild < 2) {
				report(LVL_ERR, an->line, "(select) ... (at random) requires at least two alternatives.");
				exit(1);
			}

			zi = append_instr(r, Z_CALLVS);
			zi->oper[0] = ROUTINE(R_SEL_RANDOM);
			zi->oper[1] = SMALL(sel);
			zi->oper[2] = SMALL(an->nchild);
			zi->store = REG_TEMP;
		} else if(an->subkind == SEL_T_RANDOM) {
			int sel = next_select++;

			if(an->nchild < 2) {
				report(LVL_ERR, an->line, "(select) ... (then at random) requires at least two alternatives.");
				exit(1);
			}

			zi = append_instr(r, Z_CALLVS);
			zi->oper[0] = ROUTINE(R_SEL_T_RANDOM);
			zi->oper[1] = SMALL(sel);
			zi->oper[2] = SMALL(an->nchild);
			zi->store = REG_TEMP;
		} else if(an->subkind == SEL_P_RANDOM) {
			zi = append_instr(r, Z_RANDOM);
			zi->oper[0] = SMALL(an->nchild);
			zi->store = REG_TEMP;
		} else if(an->subkind == SEL_T_P_RANDOM) {
			int sel = next_select++;

			zi = append_instr(r, Z_CALLVS);
			zi->oper[0] = ROUTINE(R_SEL_T_P_RANDOM);
			zi->oper[1] = SMALL(sel);
			zi->oper[2] = SMALL(an->nchild);
			zi->store = REG_TEMP;
		} else if(an->subkind == SEL_CYCLING) {
			int sel = next_select++;

			zi = append_instr(r, Z_CALLVS);
			zi->oper[0] = ROUTINE(R_SEL_CYCLING);
			zi->oper[1] = SMALL(sel);
			zi->oper[2] = SMALL(an->nchild);
			zi->store = REG_TEMP;
		} else {
			assert(0); // unimplemented select variant.
			exit(1);
		}

		for(i = 0; i < an->nchild - 1; i++) {
			ll = r->next_label++;
			zi = append_instr(r, Z_JNE);
			zi->oper[0] = VALUE(REG_TEMP);
			zi->oper[1] = SMALL((an->subkind == SEL_P_RANDOM)? i + 1 : i);
			zi->branch = ll;
			*rptr = r;
			clear_lingering(vars);
			compile_body(an->children[i], rptr, pred, envflags, vars, tail? tail : endlab, prg);
			// rptr may change, r remains
			zi = append_instr(r, OP_LABEL(ll));
		}
		*rptr = r;
		clear_lingering(vars);
		compile_body(an->children[i], rptr, pred, envflags, vars, tail? tail : endlab, prg);
	}

	if(!tail) {
		clear_lingering(vars);
		*rptr = make_routine(endlab, 0);
	}
}

void compile_stoppable(struct astnode *an, struct routine **rptr, struct predicate *pred, int envflags, struct var *vars, struct program *prg) {
	struct routine *r = *rptr;
	struct zinstr *zi;
	uint16_t endlab = make_routine_label();

	zi = append_instr(r, Z_CALL2N);
	zi->oper[0] = ROUTINE(R_TRY_ME_ELSE_0);
	zi->oper[1] = ROUTINE(endlab);

	zi = append_instr(r, Z_CALL1N);
	zi->oper[0] = ROUTINE(R_PUSH_STOP);

	compile_body(an->children[0], rptr, pred, envflags, vars, R_STOP_PRED, prg);

	clear_lingering(vars);

	r = *rptr = make_routine(endlab, 0);
	zi = append_instr(r, Z_CALL1N);
	zi->oper[0] = ROUTINE(R_TRUST_ME_0);
}

void compile_statusbar(struct astnode *an, struct routine **rptr, struct predicate *pred, int envflags, struct var *vars, struct program *prg) {
	struct routine *r = *rptr;
	struct zinstr *zi;
	uint16_t endlab = make_routine_label();
	uint32_t o1;

	o1 = compile_ast_to_oper(r, an->children[0], vars, 0);
	zi = append_instr(r, Z_CALLVN);
	zi->oper[0] = ROUTINE(R_BEGINSTATUS);
	zi->oper[1] = o1;
	zi->oper[2] = ROUTINE(endlab);

	compile_body(an->children[1], rptr, pred, envflags, vars, R_STOP_PRED, prg);

	clear_lingering(vars);

	r = *rptr = make_routine(endlab, 0);
	zi = append_instr(r, Z_CALL1N);
	zi->oper[0] = ROUTINE(R_TRUST_ME_0);

	zi = append_instr(r, Z_CALL1N);
	zi->oper[0] = ROUTINE(R_ENDSTATUS);
}

void compile_body(struct astnode *an, struct routine **rptr, struct predicate *pred, int envflags, struct var *vars, int tail, struct program *prg) {
	struct routine *r = *rptr;
	struct zinstr *zi;

#if 0
	// this doesn't seem to improve performance, and the code gets bigger
	struct astnode *more = strip_trivial_conditions(an);
	compile_trivial_conditions(r, an, RFALSE, vars);
	an = more;
#endif

	for(;;) {
		r = *rptr;
		if(!an) {
			if(tail) {
				if(tail == TAIL_CONT) {
					compile_deallocate(r, envflags);
					if(envflags & ENVF_CAN_BE_SIMPLE) {
						if(envflags & ENVF_CAN_BE_MULTI) {
							zi = append_instr(r, Z_LOAD);
							zi->oper[0] = VALUE(REG_SIMPLEREF);
							zi->store = REG_CHOICE;
						} else {
							zi = append_instr(r, Z_STORE);
							zi->oper[0] = SMALL(REG_CHOICE);
							zi->oper[1] = VALUE(REG_SIMPLE);
						}
					}
					zi = append_instr(r, Z_RET);
					zi->oper[0] = VALUE(REG_CONT);
				} else {
					// This could be e.g. the end of a then-body, or an or-branch.

					zi = append_instr(r, Z_RET);
					zi->oper[0] = ROUTINE(tail);
				}
			}
			return;
		} else if(an->kind == AN_RULE && (an->predicate->pred->flags & PREDF_FAIL)) {
#if 0
			printf("eliminating body call to %s", an->predicate->printed_name);
			if(an->next_in_body) printf(" non-tail!");
			printf("\n");
#endif
			if(tail) {
				zi = append_instr(r, Z_RFALSE);
			} else {
				zi = append_instr(r, Z_JZ);	// don't confuse txd by dead code
				zi->oper[0] = SMALL(0);
				zi->branch = RFALSE;
			}
			return;
		} else if(an->kind == AN_BLOCK) {
			compile_body(an->children[0], rptr, pred, envflags, vars, an->next_in_body? 0 : tail, prg);
			an = an->next_in_body;
			if(tail && !an) return;
		} else if(an->kind == AN_OR) {
			compile_disjunction(an, rptr, pred, envflags, vars, an->next_in_body? 0 : tail, prg);
			an = an->next_in_body;
			if(tail && !an) return;
		} else if(an->kind == AN_IF) {
			compile_if(an, rptr, pred, envflags, vars, an->next_in_body? 0 : tail, prg);
			an = an->next_in_body;
			if(tail && !an) return;
		} else if(an->kind == AN_EXHAUST) {
			compile_exhaust(an, rptr, pred, envflags, vars, prg);
			an = an->next_in_body;
		} else if(an->kind == AN_FIRSTRESULT) {
			compile_firstresult(an, rptr, pred, envflags, vars, prg);
			an = an->next_in_body;
		} else if(an->kind == AN_COLLECT) {
			compile_collect(an, rptr, pred, envflags, vars, prg);
			an = an->next_in_body;
		} else if(an->kind == AN_COLLECT_WORDS) {
			compile_collect_words(an, rptr, pred, envflags, vars, prg);
			an = an->next_in_body;
		} else if(an->kind == AN_DETERMINE_OBJECT) {
			compile_determine_obj(an, rptr, pred, envflags, vars, prg);
			an = an->next_in_body;
		} else if(an->kind == AN_SELECT) {
			compile_select(an, rptr, pred, envflags, vars, an->next_in_body? 0 : tail, prg);
			an = an->next_in_body;
			if(tail && !an) return;
		} else if(an->kind == AN_STOPPABLE) {
			compile_stoppable(an, rptr, pred, envflags, vars, prg);
			an = an->next_in_body;
		} else if(an->kind == AN_STATUSBAR) {
			compile_statusbar(an, rptr, pred, envflags, vars, prg);
			an = an->next_in_body;
		} else if(is_simple_builtin(an, 0)) {
			an = compile_simple_astnode(r, an, pred, envflags, vars, prg);
		} else if(an->kind == AN_RULE) {
			assert(!(an->predicate->pred->flags & PREDF_FAIL));
			compile_rule(an, rptr, pred, envflags, vars, an->next_in_body? 0 : tail, prg);
			an = an->next_in_body;
			if(tail && !an) return;
		} else if(an->kind == AN_NEG_RULE || an->kind == AN_NEG_BLOCK) {
			compile_neg(an, rptr, pred, envflags, vars, prg);
			an = an->next_in_body;
		} else {
			assert(0); // unimplemented astnode
			exit(1);
		}
	}
}

int contains_nontail_call(struct astnode *an) {
	while(an && is_simple_builtin(an, 0)) {
		an = an->next_in_body;
	}

	return an && (an->kind != AN_RULE || an->next_in_body);
}

int contains_simple_call(struct astnode *an, int exclude_tail) {
	int i;

	while(an) {
		if(an->kind == AN_RULE
		&& an->subkind == RULE_SIMPLE) {
			if(an->next_in_body || !exclude_tail) {
				return 1;
			}
		} else if(an->kind == AN_COLLECT || an->kind == AN_COLLECT_WORDS || an->kind == AN_DETERMINE_OBJECT || an->kind == AN_SELECT || an->kind == AN_STOPPABLE || an->kind == AN_STATUSBAR || an->kind == AN_BLOCK || an->kind == AN_NEG_BLOCK) {
			// todo tighten this condition
			return 1;
		} else {
			for(i = 0; i < an->nchild; i++) {
				if(contains_simple_call(an->children[i], 0)) {
					return 1;
				}
			}
		}
		an = an->next_in_body;
	}

	return 0;
}

void prepare_clause(struct clause *cl) {
	struct backend_clause *bc = cl->backend;
	struct predname *predname = cl->predicate;
	struct predicate *pred = predname->pred;
	struct astnode *body = cl->body, *an;
	int subgoal = 0, envflags = predname->arity, npersistent = 0, ntemp = 0;
	struct var *v;
	int i;

	if(pred->flags & PREDF_INVOKED_MULTI) {
		envflags |= ENVF_CAN_BE_MULTI;
	}
	if(pred->flags & PREDF_INVOKED_SIMPLE) {
		envflags |= ENVF_CAN_BE_SIMPLE;
	}
	if(pred->flags & PREDF_INVOKED_FOR_WORDS) {
		envflags |= ENVF_MAYBE_FOR_WORDS;
	}

	for(an = body; an && is_simple_builtin(an, 0); an = an->next_in_body);
	if(contains_just(an)) {
		envflags |= ENVF_ENV | ENVF_CUT_SAVED;
		npersistent = 1;	// deep cut is always env entry #0
	}

	if(contains_nontail_call(body)) {
		envflags |= ENVF_ENV | ENVF_SIMPLE_SAVED;
		npersistent++;
		if((pred->flags & PREDF_INVOKED_MULTI)
		&& (pred->flags & PREDF_INVOKED_SIMPLE)) {
			envflags |= ENVF_SIMPLEREF_SAVED;
			npersistent++;
		}
	}

	for(i = 0; i < predname->arity; i++) {
		find_vars(cl->params[i], &bc->vars, &subgoal);
	}
	find_vars(body, &bc->vars, &subgoal);
	for(v = bc->vars; v; v = v->next) {
		if(v->persistent) {
			v->slot = npersistent++;
			envflags |= ENVF_ENV;
		} else {
			v->slot = ntemp++;
		}
		if(v->occurrences == 1) {
			report(LVL_WARN, cl->line, "Singleton variable: $%s", v->name->name);
		}
	}

	if(contains_nontail_call(body)) envflags |= ENVF_ENV;

	bc->envflags = envflags;
	bc->npersistent = npersistent;
	bc->first_free_temp = ntemp;
}

void compile_clause(struct clause *original_clause, struct astnode *body, struct astnode **params, struct routine *r, line_t line, struct program *prg) {
	struct zinstr *zi;
	int i;
	struct backend_clause *bc = original_clause->backend;
	struct predname *predname = original_clause->predicate;
	struct backend_pred *bp = predname->pred->backend;
	int envflags = bc->envflags;
	struct var *v;
	uint16_t traceroutine;

	//printf("compile_clause, line %d\n", LINEPART(original_clause->line));

	for(v = bc->vars; v; v = v->next) {
		v->used = 0;
		v->still_in_reg = 0;
		v->remaining_occurrences = count_occurrences(body, v->name);
		for(i = 0; i < predname->arity; i++) {
			v->remaining_occurrences += count_occurrences(params[i], v->name);
		}
	}

	if(envflags & ENVF_ENV) {
		if(envflags & ENVF_SIMPLE_SAVED
		&& !(envflags & ENVF_CUT_SAVED)) {
			// This is a very common case.

			zi = append_instr(r, Z_CALL2N);
			zi->oper[0] = ROUTINE(R_ALLOCATE_S);
			zi->oper[1] = SMALL(4 + bc->npersistent * 2);
		} else {
			zi = append_instr(r, Z_CALL2N);
			zi->oper[0] = ROUTINE(R_ALLOCATE);
			zi->oper[1] = SMALL(4 + bc->npersistent * 2);

			if(envflags & ENVF_CUT_SAVED) {
				zi = append_instr(r, Z_STOREW);
				zi->oper[0] = VALUE(REG_ENV);
				zi->oper[1] = SMALL(ENV_CUT);
				zi->oper[2] = VALUE(REG_A + predname->arity);
			}

			if(envflags & ENVF_SIMPLE_SAVED) {
				zi = append_instr(r, Z_STOREW);
				zi->oper[0] = VALUE(REG_ENV);
				zi->oper[1] = SMALL((envflags & ENVF_CUT_SAVED)? 3 : 2);
				zi->oper[2] = VALUE(REG_SIMPLE);
			}
		}

		if(envflags & ENVF_SIMPLEREF_SAVED) {
			assert(envflags & ENVF_SIMPLE_SAVED);
			zi = append_instr(r, Z_STOREW);
			zi->oper[0] = VALUE(REG_ENV);
			zi->oper[1] = SMALL((envflags & ENVF_CUT_SAVED)? 4 : 3);
			zi->oper[2] = VALUE(REG_SIMPLEREF);
		}
	}

	next_temp = bc->first_free_temp;

	// Check the simple parameters first, to conserve heap space.
	for(i = 0; i < predname->arity; i++) {
		if(is_simple_kind(params[i]->kind)) {
			compile_param(r, params[i], REG_A + i, bc->vars, !!(predname->pred->unbound_in & (1 << i)));
		}
	}

	for(i = 0; i < predname->arity; i++) {
		if(!is_simple_kind(params[i]->kind)) {
			compile_param(r, params[i], REG_A + i, bc->vars, !!(predname->pred->unbound_in & (1 << i)));
		}
	}

	if(bp->trace_output_label) {
		if(bc->removed) {
			traceroutine = make_routine_label();
			add_trace_code(r, R_TRACE_ENTER, traceroutine, line);
			custom_trace_printer(traceroutine, bc->removed, bp->trace_output_label);
		} else {
			add_trace_code(r, R_TRACE_ENTER, bp->trace_output_label, line);
		}
	}

	compile_body(body, &r, predname->pred, envflags, bc->vars, TAIL_CONT, prg);

	if(next_temp > max_temp) max_temp = next_temp;
}

void compile_trace_output(struct predname *predname, uint16_t label) {
	int i, j, arg = 0;
	char buf[128];
	uint8_t zbuf[128];
	int bufpos = 0;
	uint16_t lab;
	struct routine *r;
	struct zinstr *zi;
	uint32_t uchar;

	r = make_routine(label, 0);
	for(i = 0; i < predname->nword; i++) {
		if(predname->words[i]) {
			if(i && (bufpos < sizeof(buf) - 1)) {
				buf[bufpos++] = ' ';
			}
			for(j = 0; predname->words[i]->name[j]; j++) {
				if(bufpos < sizeof(buf) - 1) {
					buf[bufpos++] = predname->words[i]->name[j];
				}
			}
		} else {
			if(bufpos) {
				buf[bufpos] = 0;
				utf8_to_zscii(zbuf, sizeof(zbuf), buf, &uchar);
				if(uchar) {
					report(LVL_ERR, 0, "Unsupported character U+%04x in part of predicate name %s", uchar, predname->printed_name);
					exit(1);
				}
				lab = find_global_string(zbuf)->global_label;
				zi = append_instr(r, Z_PRINTPADDR);
				zi->oper[0] = REF(lab);
				bufpos = 0;
			}
			if(i) {
				zi = append_instr(r, Z_STORE);
				zi->oper[0] = SMALL(REG_SPACE);
				zi->oper[1] = SMALL(0);
			}
			zi = append_instr(r, Z_CALL2N);
			zi->oper[0] = ROUTINE(R_TRACE_VALUE);
			zi->oper[1] = VALUE(REG_A + arg++);
		}
	}
	if(bufpos) {
		buf[bufpos] = 0;
		utf8_to_zscii(zbuf, sizeof(zbuf), buf, &uchar);
		if(uchar) {
			report(LVL_ERR, 0, "Unsupported character U+%04x in part of predicate name %s", uchar, predname->printed_name);
			exit(1);
		}
		lab = find_global_string(zbuf)->global_label;
		zi = append_instr(r, Z_PRINTPADDR);
		zi->oper[0] = REF(lab);
	}
	append_instr(r, Z_RFALSE);
	//append_instr(r, Z_END);
}

static uint16_t tag_eval_value(value_t v, struct program *prg) {
	switch(v.tag) {
	case VAL_NUM:
		assert(v.value >= 0);
		assert(v.value <= 0x3fff);
		return 0x4000 + v.value;
	case VAL_OBJ:
		assert(v.value < prg->nworldobj);
		assert(v.value < 0x1ffe);
		return 1 + v.value;
	case VAL_DICT:
		if(v.value < 256) {
			return 0x3e00 | v.value;
		} else {
			assert(dictmap[v.value - 256] < ndict);
			return 0x2000 | dictmap[v.value - 256];
		}
	case VAL_NIL:
		return 0x1fff;
	}
	assert(0); exit(1);
}

static int render_eval_value(uint8_t *buffer, int nword, value_t v, struct eval_state *es, struct predname *predname) {
	int count, size = 1, n;
	uint16_t value;

	// Simple elements (including the empty list) are serialised as themselves.
	// Proper lists are serialised as the elements, followed by c000+n.
	// Improper lists are serialised as the elements, followed by the improper tail element, followed by e000+n.

	switch(v.tag) {
	case VAL_NUM:
	case VAL_OBJ:
	case VAL_DICT:
	case VAL_NIL:
		value = tag_eval_value(v, es->program);
		break;
	case VAL_PAIR:
		count = 0;
		for(;;) {
			n = render_eval_value(buffer, nword, eval_gethead(v, es), es, predname);
			size += n;
			nword -= n;
			buffer += 2 * n;
			count++;
			v = eval_gettail(v, es);
			if(v.tag == VAL_NIL) {
				value = 0xc000 | count;
				break;
			} else if(v.tag != VAL_PAIR) {
				n = render_eval_value(buffer, nword, v, es, predname);
				size += n;
				nword -= n;
				buffer += 2 * n;
				value = 0xe000 | count;
				break;
			}
		}
		break;
	case VAL_REF:
		report(
			LVL_ERR,
			0,
			"Initial value of global variable %s must be bound.",
			predname->printed_name);
		exit(1);
	default:
		assert(0);
		exit(1);
	}

	if(nword <= 0) {
		report(
			LVL_ERR,
			0,
			"Cannot fit initial values into long-term heap. Use commandline option -L to enlarge it.");
		exit(1);
	}

	buffer[0] = value >> 8;
	buffer[1] = value & 0xff;
	return size;
}

struct memoized_index *add_memoized_index(struct clause **clauses, struct indexvalue *values, int nvalue, uint16_t label, struct memoized_index **ptr) {
	struct memoized_index *mi = malloc(sizeof(*mi));

	mi->next = *ptr;
	mi->clauses = clauses;
	mi->values = values;
	mi->nvalue = nvalue;
	mi->label = label;
	mi->pending = 1;
	*ptr = mi;

	return mi;
}

struct memoized_index *find_memoized_index(struct clause **clauses, struct indexvalue *values, int nvalue, struct memoized_index **ptr) {
	struct memoized_index *mi;
	int v, i;
	struct clause *cl1, *cl2;

	for(mi = *ptr; mi; mi = mi->next) {
		if(nvalue == mi->nvalue) {
			for(v = 0; v < nvalue; v++) {
				if(values[v].drop_first != mi->values[v].drop_first) break;
				cl1 = clauses[values[v].offset];
				cl2 = mi->clauses[mi->values[v].offset];
				if(cl1->body != cl2->body) break;
				assert(cl1->predicate->arity == cl2->predicate->arity);
				for(i = 0; i < cl1->predicate->arity; i++) {
					if(cl1->params[i] != cl2->params[i]) break;
				}
				if(i != cl1->predicate->arity) break;
			}
			if(v == nvalue) return mi;
		}
	}

	return 0;
}

void compile_clause_chain(struct clause **clauses, int nclause, struct routine *r, int truly_first, struct program *prg);

void compile_index_entry(
	struct routine *r,
	int arity,
	struct clause **clauses,
	struct indexvalue *values,
	int n,
	int indirect,
	struct program *prg)
{
	int m;
	int offs;
	struct astnode *body;

	if(n == 1) {
		struct astnode *modparams[arity];
		struct clause subclause;
		struct backend_clause subbc;

		offs = values[0].offset;
		if(values[0].drop_first) {
			assert(clauses[offs]->body->kind == AN_RULE);
			body = clauses[offs]->body->next_in_body;
		} else {
			body = clauses[offs]->body;
		}
		memcpy(&subclause, clauses[offs], sizeof(subclause));
		memcpy(&subbc, subclause.backend, sizeof(subbc));
		subclause.backend = &subbc;
		memcpy(modparams, clauses[offs]->params, arity * sizeof(struct astnode *));
		if(indirect) {
			assert(modparams[0]->kind == AN_PAIR);
			modparams[0] = modparams[0]->children[1];
			subbc.removed = malloc(sizeof(struct valuelist));
			subbc.removed->value = values[0].value;
			subbc.removed->next = ((struct backend_clause *) clauses[offs]->backend)->removed;
		} else if(!values[0].drop_first) {
			modparams[0] = mkast(AN_VARIABLE, 0, clauses[offs]->arena, clauses[offs]->line);
			modparams[0]->word = find_word(prg, "");
		}
		compile_clause(
			&subclause,
			body,
			modparams,
			r,
			clauses[offs]->line,
			prg);
	} else {
		struct clause *subclauses[n];
		struct backend_clause *bc;
#if 0
		printf("%s case split into %d branches.\n", indirect? "Indirect" : "Direct", n);
#endif
		for(m = 0; m < n; m++) {
			offs = values[m].offset;
			subclauses[m] = calloc(1, sizeof(struct clause));
			if(values[m].drop_first) {
				assert(clauses[offs]->body->kind == AN_RULE);
				subclauses[m]->body = clauses[offs]->body->next_in_body;
			} else {
				subclauses[m]->body = clauses[offs]->body;
			}
			subclauses[m]->predicate = clauses[offs]->predicate;
			subclauses[m]->arena = clauses[offs]->arena;
			assert(subclauses[m]->predicate->arity == arity);
			subclauses[m]->line = clauses[offs]->line;
			bc = subclauses[m]->backend = malloc(sizeof(struct backend_clause));
			memcpy(bc, clauses[offs]->backend, sizeof(struct backend_clause));
			subclauses[m]->params = malloc(arity * sizeof(struct astnode *));
			memcpy(subclauses[m]->params, clauses[offs]->params, arity * sizeof(struct astnode *));
			if(indirect) {
				assert(subclauses[m]->params[0]->kind == AN_PAIR);
				subclauses[m]->params[0] = subclauses[m]->params[0]->children[1];
				bc->removed = malloc(sizeof(struct valuelist));
				bc->removed->value = values[m].value;
				bc->removed->next = ((struct backend_clause *) clauses[offs]->backend)->removed;
			} else if(!values[m].drop_first) {
				subclauses[m]->params[0] = mkast(AN_VARIABLE, 0, clauses[offs]->arena, clauses[offs]->line);
				subclauses[m]->params[0]->word = find_word(prg, "");
			}
		}
		compile_clause_chain(subclauses, n, r, 0, prg);
		for(m = 0; m < n; m++) {
			free(subclauses[m]->backend);
			free(subclauses[m]->params);
			free(subclauses[m]);
		}
	}
}

void compile_index(
	int n,
	struct indexvalue *values,
	uint32_t oper,
	struct routine *r,
	uint16_t failure,
	int arity,
	struct clause **clauses,
	int indirect,
	struct memoized_index **memos,
	struct program *prg)
{
	int i, j, k, mid;
	struct zinstr *zi;
	uint16_t ll;
	int ncase = 0, count = 0;
	struct memoized_index *mi;
	int jetable[n];
	int nje = 0;

	for(i = 0; i < n; i += k) {
		for(k = 1; i + k < n && values[i + k].value == values[i].value; k++);
		ncase++;
	}

	if(ncase >= TWEAK_BINSEARCH) {
		for(i = 0; i < n && count < ncase / 2; i += k) {
			for(k = 1; i + k < n && values[i + k].value == values[i].value; k++);
			count++;
		}
		mid = i;
		assert(mid < n);
		ll = r->next_label++;
		zi = append_instr(r, Z_JGE);
		zi->oper[0] = oper;
		zi->oper[1] = (values[mid].value == 0x1fff)? VALUE(REG_NIL) : SMALL_OR_LARGE(values[mid].value);
		zi->branch = ll;
		compile_index(mid, values, oper, r, failure, arity, clauses, indirect, memos, prg);
		zi = append_instr(r, OP_LABEL(ll));
		compile_index(n - mid, values + mid, oper, r, failure, arity, clauses, indirect, memos, prg);
	} else {
		for(i = 0; i < n; i += k) {
			for(k = 1; i + k < n && values[i + k].value == values[i].value; k++);
			mi = find_memoized_index(clauses, &values[i], k, memos);
			if(!mi) {
				mi = add_memoized_index(clauses, &values[i], k, r->next_label++, memos);
			}

			if(i + k >= n && mi->pending) {
				zi = append_instr(r, Z_JNE);
				zi->oper[0] = oper;
				zi->oper[1] = (values[i].value == 0x1fff)? VALUE(REG_NIL) : SMALL_OR_LARGE(values[i].value);
				zi->branch = failure;

				mi->pending = 0;
				zi = append_instr(r, OP_LABEL(mi->label));
				compile_index_entry(r, arity, clauses, &values[i], k, indirect, prg);
			} else {
				for(j = 0; j < nje; j++) {
					zi = &r->instr[jetable[j]];
					if(zi->branch == mi->label) {
						if(!zi->oper[2]) {
							zi->oper[2] = (values[i].value == 0x1fff)? VALUE(REG_NIL) : SMALL_OR_LARGE(values[i].value);
							break;
						} else if(!zi->oper[3]) {
							zi->oper[3] = (values[i].value == 0x1fff)? VALUE(REG_NIL) : SMALL_OR_LARGE(values[i].value);
							break;
						}
					}
				}
				if(j == nje) {
					jetable[nje++] = r->ninstr;
					zi = append_instr(r, Z_JE);
					zi->oper[0] = oper;
					zi->oper[1] = (values[i].value == 0x1fff)? VALUE(REG_NIL) : SMALL_OR_LARGE(values[i].value);
					zi->branch = mi->label;
				}
				if(i + k >= n) {
					if(failure == RFALSE) {
						zi = append_instr(r, Z_RFALSE);
					} else {
						zi = append_instr(r, Z_JUMP);
						zi->oper[0] = REL_LABEL(failure);
					}
				}
			}
		}
	}
}

void complete_index(struct routine *r, int arity, int indirect, struct memoized_index *mi, struct program *prg) {
	struct memoized_index *next;

	while(mi) {
		if(mi->pending) {
			(void) append_instr(r, OP_LABEL(mi->label));
			compile_index_entry(r, arity, mi->clauses, mi->values, mi->nvalue, indirect, prg);
		}
		next = mi->next;
		free(mi);
		mi = next;
	}
}

int cmp_indexvalue(const void *a, const void *b) {
	const struct indexvalue *aa = (const struct indexvalue *) a;
	const struct indexvalue *bb = (const struct indexvalue *) b;

	if(aa->value == bb->value) {
		return aa->offset - bb->offset;
	} else {
		return aa->value - bb->value;
	}
}

int is_initial_check(struct astnode *an, struct astnode **args, int narg) {
	int i, j;

	if((an->kind == AN_RULE || an->kind == AN_NEG_RULE)
	&& an->subkind == RULE_SIMPLE
	&& an->predicate->pred->dynamic
	&& an->predicate->arity == 1
	&& !(an->predicate->pred->flags & PREDF_GLOBAL_VAR)) {
		for(i = 0; i < an->predicate->arity; i++) {
			if(an->children[0]->kind != AN_VARIABLE) return 0;
			for(j = 0; j < narg; j++) {
				if(an->children[0]->word == args[j]->word) break;
			}
			if(j == narg) return 0;
		}
		return 1;
	}

	return 0;
}

struct astnode *split_initial_checks(struct astnode *body, struct astnode **args, int narg) {
	while(body && is_initial_check(body, args, narg)) {
		body = body->next_in_body;
	}

	return body;
}

struct astnode *compile_initial_checks(struct routine *r, struct astnode *an, struct astnode **args, int narg, uint16_t failure) {
	int arg;
	struct backend_pred *bp;
	uint16_t ll, lfail = 0;
	struct zinstr *zi;

	if(an && is_initial_check(an, args, narg)) {
		do {
			assert(an->predicate->pred->dynamic);
			assert(an->predicate->arity == 1);
			assert(!an->children[0]->unbound);
			assert(!(an->predicate->pred->flags & PREDF_GLOBAL_VAR));

			for(arg = 0; arg < narg; arg++) {
				if(an->children[0]->word == args[arg]->word) break;
			}
			assert(arg < narg);

			bp = an->predicate->pred->backend;

			if(bp->object_flag >= NZOBJFLAG) {
				zi = append_instr(r, Z_CALL2S);
				zi->oper[0] = ROUTINE(extflagreaders[(bp->object_flag - NZOBJFLAG) / 8]);
				zi->oper[1] = VALUE(REG_A + arg);
				zi->store = REG_TEMP;
				zi = append_instr(r, (an->kind == AN_RULE)? Z_TESTN : Z_TEST);
				zi->oper[0] = VALUE(REG_TEMP);
				zi->oper[1] = SMALL(0x80 >> ((bp->object_flag - NZOBJFLAG) & 7));
			} else {
				zi = append_instr(r, Z_CALLVS);
				zi->oper[0] = ROUTINE(R_READ_FLAG);
				zi->oper[1] = VALUE(REG_A + arg);
				zi->oper[2] = SMALL(bp->object_flag);
				zi->store = REG_TEMP;
				zi = append_instr(r, (an->kind == AN_RULE)? Z_JZ : Z_JNZ);
				zi->oper[0] = VALUE(REG_TEMP);
			}
			if(lfail) {
				zi->branch = lfail;
			} else {
				zi->op ^= OP_NOT;
				zi->branch = ll = r->next_label++;
				lfail = r->next_label++;
				zi = append_instr(r, OP_LABEL(lfail));
				zi = append_instr(r, Z_RET);
				zi->oper[0] = ROUTINE(failure);
				zi = append_instr(r, OP_LABEL(ll));
			}
			an = an->next_in_body;
		} while(an && is_initial_check(an, args, narg));
	}

	return an;
}

#define CFLAG_INDIRECT	0x01
#define CFLAG_DIRECT	0x02
#define CFLAG_OBJECT	0x04
#define CFLAG_OBJFLAG	0x08
#define CFLAG_FROMLIST	0x10

void compile_clause_chain(struct clause **clauses, int nclause, struct routine *r, int truly_first, struct program *prg) {
	uint16_t cont = 0, ll, ll2;
	int i, j, k, m, nc, nv;
	struct indexvalue *values = 0;
	int last;
	struct zinstr *zi;
	int indirect, property;
	struct backend_wobj *wobj;
	struct backend_clause *bc;
	int have_choice = 0, need_choice, valid_args = 1, have_initial_checks;
	struct astnode *an, *sub;
	uint8_t clauseflags[nclause];
	struct var *v;
	uint16_t not_just_objs = 0;
	struct memoized_index *memos;
	struct predname *predname;
	struct predicate *pred;

	assert(nclause);

	predname = clauses[0]->predicate;
	pred = predname->pred;

#if 0
	printf("begin compile_clause_chain %s", predname->printed_name);
	printf(" n = %d\n", nclause);
#endif

	memset(clauseflags, 0, nclause);
	if(predname->arity && !(pred->unbound_in & 1)) {
		for(i = 0; i < nclause; i++) {
			bc = clauses[i]->backend;
			if(clauses[i]->params[0]->kind == AN_PAIR) {
				an = clauses[i]->params[0]->children[0];
				clauseflags[i] = CFLAG_INDIRECT;
			} else {
				an = clauses[i]->params[0];
				clauseflags[i] = CFLAG_DIRECT;
			}
			if(an->kind == AN_TAG) {
				clauseflags[i] |= CFLAG_OBJECT;
			} else if(!is_simple_kind(an->kind)) {
				if(an->kind == AN_VARIABLE
				&& clauses[i]->body
				&& clauses[i]->body->kind == AN_RULE
				&& clauses[i]->body->subkind == RULE_SIMPLE
				&& (clauses[i]->body->predicate->pred->flags & PREDF_FIXED_FLAG)
				&& clauses[i]->body->children[0]->kind == AN_VARIABLE
				&& clauses[i]->body->children[0]->word == an->word) {
					for(v = bc->vars; v; v = v->next) {
						if(v->name == an->word) break;
					}
					assert(v && v->occurrences >= 2);
#if 0
					printf("occurrences %d\n", v->occurrences);
#endif
					if(v->occurrences <= 2 || (clauseflags[i] & CFLAG_DIRECT)) {
						clauseflags[i] |= CFLAG_OBJECT | CFLAG_OBJFLAG;
					} else {
						clauseflags[i] = 0;
					}
				} else if(an->kind == AN_VARIABLE
				&& clauses[i]->body
				&& clauses[i]->body->kind == AN_RULE
				&& clauses[i]->body->subkind == RULE_MULTI
				&& clauses[i]->body->predicate->builtin == BI_IS_ONE_OF
				&& clauses[i]->body->children[0]->kind == AN_VARIABLE
				&& clauses[i]->body->children[0]->word == an->word
				&& is_list_without_vars_or_pairs(clauses[i]->body->children[1])) {
					for(v = bc->vars; v; v = v->next) {
						if(v->name == an->word) break;
					}
					assert(v && v->occurrences >= 2);
					if(v->occurrences <= 2 || (clauseflags[i] & CFLAG_DIRECT)) {
						clauseflags[i] |= CFLAG_FROMLIST;
					} else {
						clauseflags[i] = 0;
					}
				} else {
					clauseflags[i] = 0;
				}
			}
		}
	}
#if 0
	if(predname->arity && !(pred->unbound_in & 1)) {
		printf("Considering %s of clauses for %s\n",
			truly_first? "the chain" : "a subchain",
			predname->printed_name);
		for(i = 0; i < nclause; i++) {
			if(clauseflags[i]) {
				char ch = 's';
				if(clauseflags[i] & CFLAG_OBJECT) ch = 'o';
				if(clauseflags[i] & CFLAG_OBJFLAG) ch = 'f';
				if(clauseflags[i] & CFLAG_FROMLIST) ch = 'l';
				if(clauseflags[i] & CFLAG_INDIRECT) ch ^= 0x20;
				printf("%c", ch);
			} else {
				printf(".");
			}
		}
		printf("\n");
	}
#endif

	// Remove short stretches of objects/objflags to conserve property usage.
	// For instance, if there's a single rule with the formal parameter (item $),
	// don't put a vector to the exact same routine inside every item.
	for(i = 0; i < nclause; i += j) {
		j = 1;
		k = next_free_prop;
		if(clauseflags[i] & CFLAG_OBJECT) {
			while(i + j < nclause && (clauseflags[i + j] & CFLAG_OBJECT)) {
				j++;
			}
			if(j < TWEAK_PROPDISPATCH || k >= 64) {
				for(m = 0; m < j; m++) {
					if(clauseflags[i + m] & CFLAG_OBJFLAG) {
						clauseflags[i + m] = 0;
					} else {
						clauseflags[i + m] &= ~CFLAG_OBJECT;
					}
				}
			} else {
				k++;
			}
		}
	}

#if 0
	if(predname->arity && !(pred->unbound_in & 1)) {
		for(i = 0; i < nclause; i++) {
			if(clauseflags[i]) {
				char ch = 's';
				if(clauseflags[i] & CFLAG_OBJECT) ch = 'o';
				if(clauseflags[i] & CFLAG_OBJFLAG) ch = 'f';
				if(clauseflags[i] & CFLAG_FROMLIST) ch = 'l';
				if(clauseflags[i] & CFLAG_INDIRECT) ch ^= 0x20;
				printf("%c", ch);
			} else {
				printf(".");
			}
		}
		printf("\n");
	}
#endif

	for(i = 0; i < nclause; i += nc) {
		nc = 1;
		indirect = 0;
		property = 0;
		if(predname->arity && !(pred->unbound_in & 1)) {
			if(clauseflags[i]) {
				indirect = !!(clauseflags[i] & CFLAG_INDIRECT);
				if(clauseflags[i] & CFLAG_OBJECT) {
					property = next_free_prop++;
					assert(property < 64);
				}
				while(i + nc < nclause) {
					if(!clauseflags[i + nc]
					|| ((clauseflags[i + nc] ^ clauseflags[i]) & CFLAG_INDIRECT)) {
						break;
					}
					nc++;
				}
			}
		}

		if(i + nc < nclause) {
			cont = make_routine_label();
			last = 0;
		} else {
			cont = 0;
			last = 1;
		}

		need_choice = !last;
		have_initial_checks = 0;
#if 1
		// If all these clauses are guaranteed to succeed, and we're only ever
		// invoked as a simple call, then we don't need to save a choice point.

		// Only do it for direct indices for now, else we trash REG_A+0.

		if(nc > 1 && !indirect && !(pred->flags & PREDF_INVOKED_MULTI) && need_choice) {
			for(j = 0; j < nc; j++) {
				for(k = 1; k < predname->arity; k++) {
					if(clauses[i + j]->params[k]->kind != AN_VARIABLE) break;
					for(m = 1; m < k; m++) {
						if(clauses[i + j]->params[k]->word
						== clauses[i + j]->params[m]->word) {
							break;
						}
					}
					if(m < k) break;
				}
				if(k < predname->arity
				|| !body_succeeds(clauses[i + j]->body)) {
					break;
				}
			}
			if(j == nc) {
				need_choice = 0;
			}
		}
#endif
		nv = 0;
		for(j = 0; j < nc; j++) {
			if(clauseflags[i + j] & CFLAG_OBJFLAG) {
				assert(clauses[i + j]->body->kind == AN_RULE);
				assert(clauses[i + j]->body->predicate->pred->flags & PREDF_FIXED_FLAG);
				for(k = 0; k < prg->nworldobj; k++) {
					if(clauses[i + j]->body->predicate->fixedvalues[k]) {
						nv++;
					}
				}
			} else if(clauseflags[i + j] & CFLAG_FROMLIST) {
				assert(clauses[i + j]->body->kind == AN_RULE);
				assert(clauses[i + j]->body->predicate->builtin == BI_IS_ONE_OF);
				for(an = clauses[i + j]->body->children[1]; an->kind == AN_PAIR; an = an->children[1]) {
					nv++;
				}
			} else {
				nv++;
			}
		}

		if(clauseflags[i]) {
			values = calloc(nv, sizeof(struct indexvalue));
			m = 0;
			for(j = 0; j < nc; j++) {
				an = (clauseflags[i + j] & CFLAG_INDIRECT)?
					clauses[i + j]->params[0]->children[0] :
					clauses[i + j]->params[0];
				if(clauseflags[i + j] & CFLAG_OBJFLAG) {
					assert(an->kind == AN_VARIABLE);
					for(k = 0; k < prg->nworldobj; k++) {
						if(clauses[i + j]->body->predicate->fixedvalues[k]) {
							values[m].offset = j;
							values[m].an = an;
							values[m].value = 1 + k;
							values[m].drop_first = 1;
							m++;
						}
					}
				} else if(clauseflags[i + j] & CFLAG_FROMLIST) {
					assert(an->kind == AN_VARIABLE);
					for(sub = clauses[i + j]->body->children[1]; sub->kind == AN_PAIR; sub = sub->children[1]) {
						values[m].offset = j;
						values[m].an = an;
						values[m].value = tag_simple_value(sub->children[0]);
						values[m].drop_first = 1;
						m++;
					}
				} else {
					values[m].offset = j;
					values[m].an = an;
					values[m].value = tag_simple_value(an);
					m++;
				}
			}
			assert(m == nv);
			qsort(values, nv, sizeof(struct indexvalue), cmp_indexvalue);
		} else {
			if(!last && !(pred->flags & PREDF_INVOKED_MULTI)) {
				for(k = 0; k < predname->arity; k++) {
					if(pred->unbound_in & (1 << k)) break;
					if(clauses[i]->params[k]->kind != AN_VARIABLE) break;
					for(m = 1; m < k; m++) {
						if(clauses[i]->params[k]->word
						== clauses[i]->params[m]->word) {
							break;
						}
					}
					if(m < k) break;
				}
				if(k == predname->arity) {
					an = split_initial_checks(clauses[i]->body, clauses[i]->params, k);
					if(body_succeeds(an)) {
#if 0
						printf("Simple checks & succeeding body:");
						pp_clause(clauses[i]);
#endif
						need_choice = 0;
						have_initial_checks = 1;
					}
				}
			}
		}

		if(property) {
			for(j = 0; j < nv; j++) {
				if(values[j].value < 0x1fff) {
					assert(values[j].value);
					wobj = &backendwobj[values[j].value - 1];
					if(!wobj->npropword[property]) {
						values[j].label = make_routine_label();
						wobj->npropword[property] = 1;
						wobj->propword[property] = malloc(2);
						wobj->propword[property][0] = 0x8000 + values[j].label;
					}
				} else {
					not_just_objs = 1;
					break;
				}
			}
			if(need_choice || last) {
				propdefault[property] = 0;
			} else {
				assert(cont);
				propdefault[property] = 0x8000 + cont;
			}
		}

		if(truly_first && i == 0 && (pred->flags & PREDF_CONTAINS_JUST)) {
			zi = append_instr(r, Z_STORE);
			zi->oper[0] = SMALL(REG_A + predname->arity);
			zi->oper[1] = VALUE(REG_CHOICE);
		}

		if(last && have_choice) {
			if(predname->arity + !!(pred->flags & PREDF_CONTAINS_JUST)) {
				zi = append_instr(r, Z_CALL2N);
				zi->oper[0] = ROUTINE(R_TRUST_ME);
				zi->oper[1] = SMALL(predname->arity + !!(pred->flags & PREDF_CONTAINS_JUST));
			} else {
				zi = append_instr(r, Z_CALL1N);
				zi->oper[0] = ROUTINE(R_TRUST_ME_0);
			}
			have_choice = 0;
			valid_args = 1;
		}

		if(!valid_args) {
			assert(have_choice);
			assert(!last);
			if(need_choice) {
				if(predname->arity + !!(pred->flags & PREDF_CONTAINS_JUST)) {
					zi = append_instr(r, Z_CALLVN);
					zi->oper[0] = ROUTINE(R_RETRY_ME_ELSE);
					zi->oper[1] = SMALL(predname->arity + !!(pred->flags & PREDF_CONTAINS_JUST));
					zi->oper[2] = ROUTINE(cont);
				} else {
					zi = append_instr(r, Z_CALL2N);
					zi->oper[0] = ROUTINE(R_RETRY_ME_ELSE_0);
					zi->oper[1] = ROUTINE(cont);
				}
			} else {
				if(predname->arity + !!(pred->flags & PREDF_CONTAINS_JUST)) {
					zi = append_instr(r, Z_CALL2N);
					zi->oper[0] = ROUTINE(R_TRUST_ME);
					zi->oper[1] = SMALL(predname->arity + !!(pred->flags & PREDF_CONTAINS_JUST));
				} else {
					zi = append_instr(r, Z_CALL1N);
					zi->oper[0] = ROUTINE(R_TRUST_ME_0);
				}
				have_choice = 0;
			}
			valid_args = 1;
		}

		if(need_choice) {
			assert(valid_args);
			if(!have_choice) {
				if(predname->arity + !!(pred->flags & PREDF_CONTAINS_JUST)) {
					zi = append_instr(r, Z_CALLVN);
					zi->oper[0] = ROUTINE(R_TRY_ME_ELSE);
					zi->oper[1] = SMALL((predname->arity + !!(pred->flags & PREDF_CONTAINS_JUST) + CHOICE_SIZEOF) * 2);
					zi->oper[2] = ROUTINE(cont);
				} else {
					zi = append_instr(r, Z_CALL2N);
					zi->oper[0] = ROUTINE(R_TRY_ME_ELSE_0);
					zi->oper[1] = ROUTINE(cont);
				}
				have_choice = 1;
			}
			valid_args = 0;
		}

		if(nv > 1) {
#if 0
			printf("%sMaking %s index for %d cases (obtained from %d clauses) of %s",
				truly_first? "" : "   ",
				indirect? "indirect" : "direct",
				nv,
				nc,
				predname->printed_name);
			if(property) printf(" using property %d", property);
			printf("\n");
#endif
			if(need_choice || last) {
				ll = RFALSE;
			} else {
				ll = r->next_label++;
			}

			if(property) {
				if(indirect) {
					zi = append_instr(r, Z_CALL1N);
					zi->oper[0] = ROUTINE(R_GRAB_ARG1);
				}
				if(not_just_objs) {
					ll2 = r->next_label++;
					zi = append_instr(r, Z_CALL2S);
					zi->oper[0] = ROUTINE(R_DEREF_UNBOX);
					zi->oper[1] = indirect? VALUE(REG_X+0) : VALUE(REG_A+0);
					zi->store = REG_TEMP;
					zi = append_instr(r, Z_JGE);
					zi->oper[0] = VALUE(REG_TEMP);
					zi->oper[1] = VALUE(REG_NIL);
					zi->branch = ll2;
					zi = append_instr(r, Z_JLE);
					zi->oper[0] = VALUE(REG_TEMP);
					zi->oper[1] = SMALL(0);
					zi->branch = ll2;
					zi = append_instr(r, Z_GETPROP);
					zi->oper[0] = VALUE(REG_TEMP);
					zi->oper[1] = SMALL(property);
					zi->store = REG_PUSH;
					zi = append_instr(r, Z_RET_POPPED);

					zi = append_instr(r, OP_LABEL(ll2));
					for(j = 0; j < nv && values[j].value < 0x1fff; j++);
					assert(j < nv);
					memos = 0;
					compile_index(nv - j, values + j, VALUE(REG_TEMP), r, ll, predname->arity, clauses + i, indirect, &memos, prg);
					complete_index(r, predname->arity, indirect, memos, prg);
				} else {
					if(ll == RFALSE) {
						zi = append_instr(r, Z_CALL2S);
						zi->oper[0] = ROUTINE(R_DEREF_OBJ_FAIL);
						zi->oper[1] = indirect? VALUE(REG_X+0) : VALUE(REG_A+0);
						zi->store = REG_TEMP;
					} else {
						zi = append_instr(r, Z_CALL2S);
						zi->oper[0] = ROUTINE(R_DEREF_OBJ);
						zi->oper[1] = indirect? VALUE(REG_X+0) : VALUE(REG_A+0);
						zi->store = REG_TEMP;
						zi = append_instr(r, Z_JZ);
						zi->oper[0] = VALUE(REG_TEMP);
						zi->branch = ll;
					}
					zi = append_instr(r, Z_GETPROP);
					zi->oper[0] = VALUE(REG_TEMP);
					zi->oper[1] = SMALL(property);
					zi->store = REG_PUSH;
					zi = append_instr(r, Z_RET_POPPED);
				}
			} else {
				if(indirect) {
					zi = append_instr(r, Z_CALL1N);
					zi->oper[0] = ROUTINE(R_GRAB_ARG1);
				} else {
					zi = append_instr(r, Z_CALL2S);
					zi->oper[0] = ROUTINE(R_DEREF_UNBOX);
					zi->oper[1] = VALUE(REG_A+0);
					zi->store = REG_X+0;
				}
				memos = 0;
				compile_index(nv, values, VALUE(REG_X+0), r, ll, predname->arity, clauses + i, indirect, &memos, prg);
				complete_index(r, predname->arity, indirect, memos, prg);
			}

			if(ll != RFALSE) {
				zi = append_instr(r, OP_LABEL(ll));
				zi = append_instr(r, Z_RET);
				zi->oper[0] = ROUTINE(cont);
			}

			if(property) {
				for(j = 0; j < nv && values[j].value < 0x1fff; j += k) {
					for(k = 1; j + k < nv && values[j + k].value == values[j].value; k++);
					r = make_routine(values[j].label, 0);
					compile_index_entry(r, predname->arity, clauses + i, &values[j], k, indirect, prg);
				}
			}
		} else {
			struct astnode *body = clauses[i]->body;
			if(have_initial_checks) {
				body = compile_initial_checks(
					r,
					body,
					clauses[i]->params,
					k,
					cont);
			}
			compile_clause(
				clauses[i],
				body,
				clauses[i]->params,
				r,
				clauses[i]->line,
				prg);
		}

		if(cont) {
			r = make_routine(cont, 0);
		}
		free(values);
		values = 0;
	}

#if 0
	printf("end compile_clause_chain %s", predname->printed_name);
#endif
}

void compile_predicate(struct predname *predname, struct program *prg) {
	struct predicate *pred = predname->pred;
	struct backend_pred *bp = pred->backend;
	struct routine *r;
	int i;

	if(!predname->builtin || (predname->nameflags & PREDNF_DEFINABLE_BI)) {
		for(i = 0; i < pred->nclause; i++) {
			prepare_clause(pred->clauses[i]);
		}

		if(bp->trace_output_label) {
			compile_trace_output(predname, bp->trace_output_label);
		}

		if(pred->dynamic && !(pred->flags & PREDF_FIXED_FLAG)) {
			if(bp->global_label) {
				assert(pred->dynamic->linkage_flags & LINKF_LIST);
				compile_dynlinkage_list(pred, bp->global_label);
			}
		} else {
			if(pred->nclause) {
				if(bp->global_label) {
					r = make_routine(bp->global_label, 0);
					compile_clause_chain(pred->clauses, pred->nclause, r, 1, prg);
				}
			}
		}
	}
}

char *decode_metadata_str(int builtin, struct program *prg) {
	struct predname *predname;
	struct predicate *pred;
	struct astnode *an;
	char *buf;

	predname = find_builtin(prg, builtin);
	if(predname && (pred = predname->pred)->nclause) {
		an = decode_output(&buf, pred->clauses[0]->body, 0, 0);
		if(an) {
			report(
				LVL_ERR,
				pred->clauses[0]->line,
				"Story metadata may only consist of static text.");
			exit(1);
		}
		return buf;
	} else {
		return 0;
	}
}

void compile_endings_check(struct routine *r, struct endings_point *pt, int level, int have_allocated) {
	int i, j;
	struct zinstr *zi;
	uint16_t ll, ll2;
	uint8_t zscii;

	zi = append_instr(r, Z_DEC_JL);
	zi->oper[0] = SMALL(REG_LOCAL+1);
	zi->oper[1] = SMALL(0);
	zi->branch = RFALSE;
	zi = append_instr(r, Z_LOADB);
	zi->oper[0] = VALUE(REG_LOCAL+0);
	zi->oper[1] = VALUE(REG_LOCAL+1);
	zi->store = REG_LOCAL+2;
	for(i = 0; i < pt->nway; i++) {
		if(i == pt->nway - 1) {
			ll = RFALSE;
		} else {
			ll = r->next_label++;
		}
		zscii = unicode_to_zscii(pt->ways[i]->letter);
		if(verbose >= 2) {
			for(j = 0; j < level; j++) printf("    ");
			if(zscii >= 'a' && zscii <= 'z') {
				printf("If word[len - %d] == '%c'\n", level + 1, zscii);
			} else {
				printf("If word[len - %d] == zscii(%d)\n", level + 1, zscii);
			}
		}
		zi = append_instr(r, Z_JNE);
		zi->oper[0] = VALUE(REG_LOCAL+2);
		zi->oper[1] = SMALL(zscii);
		zi->branch = ll;
		if(pt->ways[i]->final) {
			if(!have_allocated) {
				if(verbose >= 2) {
					for(j = 0; j < level + 1; j++) printf("    ");
					printf("Allocate copy of length len - %d.\n", level + 1);
				}
				zi = append_instr(r, Z_CALLVS);
				zi->oper[0] = ROUTINE(R_COPY_INPUT_WORD);
				zi->oper[1] = VALUE(REG_LOCAL+0);
				zi->oper[2] = VALUE(REG_LOCAL+1);
				zi->store = REG_LOCAL+3;
			} else {
				zi = append_instr(r, Z_STOREB);
				zi->oper[0] = VALUE(REG_LOCAL+3);
				zi->oper[1] = SMALL(7);
				zi->oper[2] = VALUE(REG_LOCAL+1);
			}
			if(verbose >= 2) {
				for(j = 0; j < level + 1; j++) printf("    ");
				printf("Try with length len - %d.\n", level + 1);
			}
			zi = append_instr(r, Z_ADD);
			zi->oper[0] = VALUE(REG_LOCAL+3);
			zi->oper[1] = SMALL(6);
			zi->store = REG_LOCAL+2;
			zi = append_instr(r, Z_TOKENISE);
			zi->oper[0] = VALUE(REG_LOCAL+2);
			zi->oper[1] = VALUE(REG_LOCAL+3);
			zi = append_instr(r, Z_LOADW);
			zi->oper[0] = VALUE(REG_LOCAL+3);
			zi->oper[1] = SMALL(1);
			zi->store = REG_LOCAL+2;
			if(pt->ways[i]->more.nway) {
				ll2 = r->next_label++;
				zi = append_instr(r, Z_JZ);
				zi->oper[0] = VALUE(REG_LOCAL+2);
				zi->branch = ll2;
				zi = append_instr(r, Z_RET);
				zi->oper[0] = VALUE(REG_LOCAL+2);
				zi = append_instr(r, OP_LABEL(ll2));
			} else {
				zi = append_instr(r, Z_RET);
				zi->oper[0] = VALUE(REG_LOCAL+2);
			}
		}
		if(pt->ways[i]->more.nway) {
			compile_endings_check(r, &pt->ways[i]->more, level + 1, have_allocated || pt->ways[i]->final);
		}
		if(ll != RFALSE) {
			zi = append_instr(r, OP_LABEL(ll));
		}
	}
}

static void set_initial_reg(uint8_t *core, int reg, uint16_t value) {
	core[2 * (reg - 0x10) + 0] = value >> 8;
	core[2 * (reg - 0x10) + 1] = value & 0xff;
}

static void initial_values(struct program *prg, uint8_t *zcore, uint16_t addr_globals, uint16_t addr_lts, int ltssize, uint16_t *extflagarrays, int n_extflag) {
	struct eval_state es;
	int i, j, status;
	struct predname *predname;
	struct predicate *pred;
	struct backend_pred *bp;
	struct backend_wobj *wobj, *parent;
	struct dynamic *dyn;
	uint8_t seen[prg->nworldobj];
	value_t eval_args[2];
	uint16_t addr, value;
	int lttop = 0;

	init_evalstate(&es, prg);

	for(i = 0; i < prg->npredicate; i++) {
		predname = prg->predicates[i];
		pred = predname->pred;
		bp = pred->backend;
		if(pred->flags & PREDF_DYNAMIC) {
			dyn = pred->dynamic;

			if(predname->arity == 0) {
				eval_reinitialize(&es);
				if(eval_initial(&es, predname, 0)) {
					zcore[addr_globals + 2 * (user_global_base + bp->user_global) + 0] |= bp->user_flag_mask >> 8;
					zcore[addr_globals + 2 * (user_global_base + bp->user_global) + 1] |= bp->user_flag_mask & 0xff;
				}
			} else if(predname->arity == 1) {
				if(pred->flags & PREDF_GLOBAL_VAR) {
					eval_reinitialize(&es);
					eval_args[0] = eval_makevar(&es);
					if(eval_initial(&es, predname, eval_args)) {
						addr = addr_globals + 2 * (user_global_base + bp->user_global);
						if(eval_args[0].tag == VAL_REF) {
							report(
								LVL_ERR,
								0,
								"Initial value of global variable %s must be bound.",
								predname->printed_name);
						} else if(eval_args[0].tag == VAL_PAIR) {
							value = 0x8000 | lttop;
							zcore[addr + 0] = value >> 8;
							zcore[addr + 1] = value & 0xff;
							value = addr;
							addr = addr_lts + lttop * 2;
							zcore[addr + 2] = value >> 8;
							zcore[addr + 3] = value & 0xff;
							value = 2 + render_eval_value(zcore + addr + 4, ltssize - lttop - 2, eval_args[0], &es, predname);
							zcore[addr + 0] = value >> 8;
							zcore[addr + 1] = value & 0xff;
							lttop += value;
						} else {
							value = tag_eval_value(eval_args[0], prg);
							zcore[addr + 0] = value >> 8;
							zcore[addr + 1] = value & 0xff;
						}
					}
				} else {
					int last_wobj = 0;

					for(j = 0; j < prg->nworldobj; j++) {
						wobj = &backendwobj[j];
						if(pred->flags & PREDF_FIXED_FLAG) {
							assert(predname->nfixedvalue >= prg->nworldobj);
							status = predname->fixedvalues[j];
						} else {
							eval_reinitialize(&es);
							eval_args[0] = (value_t) {VAL_OBJ, j};
							status = eval_initial(&es, predname, eval_args);
						}
						if(status) {
							if(bp->object_flag >= NZOBJFLAG) {
								int num = (bp->object_flag - NZOBJFLAG) / 8;
								int mask = 0x80 >> ((bp->object_flag - NZOBJFLAG) & 7);
								assert(num < n_extflag);
								addr = global_labels[extflagarrays[num]] + 1 + j;
								zcore[addr] |= mask;
							} else {
								zcore[wobj->addr_objtable + bp->object_flag / 8] |= 0x80 >> (bp->object_flag & 7);
							}
							if(dyn->linkage_flags & (LINKF_LIST | LINKF_CLEAR)) {
								addr = global_labels[bp->propbase_label] + 2 + j * 2;
								zcore[addr + 0] = last_wobj >> 8;
								zcore[addr + 1] = last_wobj & 0xff;
								last_wobj = j + 1;
							}
						}
					}
					if(dyn->linkage_flags & (LINKF_LIST | LINKF_CLEAR)) {
						zcore[addr_globals + 2 * (user_global_base + bp->user_global) + 0] = last_wobj >> 8;
						zcore[addr_globals + 2 * (user_global_base + bp->user_global) + 1] = last_wobj & 0xff;
					}
				}
			} else if(predname->arity == 2) {
				for(j = 0; j < prg->nworldobj; j++) {
					wobj = &backendwobj[j];
					eval_reinitialize(&es);
					eval_args[0] = (value_t) {VAL_OBJ, j};
					eval_args[1] = eval_makevar(&es);
					if(predname->builtin == BI_HASPARENT) {
						if(eval_initial(&es, predname, eval_args)) {
							if(eval_args[1].tag != VAL_OBJ) {
								report(
									LVL_ERR,
									0,
									"Initial value of ($ has parent $) must have objects in both parameters.");
							}
							wobj->initialparent = eval_args[1].value;
						} else {
							wobj->initialparent = -1;
						}
					} else {
						if(eval_initial(&es, predname, eval_args)) {
							if(eval_args[1].tag == VAL_REF) {
								report(
									LVL_ERR,
									0,
									"Initial value of dynamic per-object variable %s, for #%s, must be bound.",
									predname->printed_name,
									prg->worldobjnames[j]->name);
							} else if(eval_args[1].tag == VAL_PAIR) {
								report(
									LVL_ERR,
									0,
									"Too complex initial value of dynamic per-object variable %s, for #%s.",
									predname->printed_name,
									prg->worldobjnames[j]->name);
							}
							addr = global_labels[bp->propbase_label] + 2 + j * 2;
							value = tag_eval_value(eval_args[1], prg);
							zcore[addr + 0] = value >> 8;
							zcore[addr + 1] = value & 0xff;
						}
					}
				}
			}
		}
	}

	for(i = 0; i < prg->nworldobj; i++) {
		memset(seen, 0, prg->nworldobj);
		wobj = &backendwobj[i];
		for(j = i; j >= 0; j = backendwobj[j].initialparent) {
			if(seen[j]) {
				report(
					LVL_ERR,
					0,
					"Badly formed object tree! #%s is nested in itself.",
					prg->worldobjnames[j]->name);
				exit(1);
			}
			seen[j] = 1;
		}
		j = wobj->initialparent;
		if(j >= 0) {
			parent = &backendwobj[j];
			zcore[wobj->addr_objtable + 6] = (j + 1) >> 8;
			zcore[wobj->addr_objtable + 7] = (j + 1) & 0xff;
			zcore[wobj->addr_objtable + 8] = zcore[parent->addr_objtable + 10];
			zcore[wobj->addr_objtable + 9] = zcore[parent->addr_objtable + 11];
			zcore[parent->addr_objtable + 10] = (i + 1) >> 8;
			zcore[parent->addr_objtable + 11] = (i + 1) & 0xff;
		}
	}

	free_evalstate(&es);

	set_initial_reg(zcore + addr_globals, REG_LTTOP, lttop);
	set_initial_reg(zcore + addr_globals, REG_LTMAX, lttop);
}

const static char ifid_template[] = "NNNNNNNN-NNNN-NNNN-NNNN-NNNNNNNNNNNN";

int match_template(const char *txt, const char *template) {
	for(;;) {
		if(*template == 'N') {
			if(!((*txt >= '0' && *txt <= '9') || (*txt >= 'A' && *txt <= 'Z'))) {
				return 0;
			}
		} else if(*template != *txt) {
			return 0;
		}
		if(!*template) return 1;
		template++;
		txt++;
	}
}

void backend(char *filename, char *format, char *coverfname, char *coveralt, int heapsize, int auxsize, int ltssize, int strip, int linecount, struct program *prg) {
	int nglobal;
	uint16_t addr_abbrevtable, addr_abbrevstr, addr_objtable, addr_globals, addr_static, addr_heap, addr_heapend, addr_aux, addr_lts, addr_dictionary, addr_seltable;
	uint32_t org;
	uint32_t filesize;
	char compiletime[8], reldate[16];
	int i, j, k;
	struct backend_wobj *wobj;
	struct global_string *gs;
	struct predname *predname;
	struct predicate *pred;
	struct dynamic *dyn;
	uint16_t checksum = 0;
	char *ifid, *author, *title, *noun, *blurb;
	uint16_t release = 0;
	uint16_t entrypc, himem;
	int n_extflag = 0;
	uint16_t *extflagarrays = 0;
	struct routine *r;
	struct zinstr *zi;
	int zversion, packfactor;

	if(!strcmp(format, "z5")) {
		zversion = 5;
		packfactor = 4;
	} else {
		zversion = 8;
		packfactor = 8;
	}

	assert(!next_routine_num);
	assert(nrtroutine == R_FIRST_FREE);
	next_routine_num = R_FIRST_FREE;
	routines = malloc(next_routine_num * sizeof(struct routine *));
	for(i = 0; i < nrtroutine; i++) {
		r = calloc(1, sizeof(*r));
		routines[rtroutines[i].rnumber] = r;
		r->nlocal = rtroutines[i].nlocal;
		r->next_label = 2;
		r->actual_routine = 0xffff;
		while(rtroutines[i].instr[r->ninstr].op != Z_END) r->ninstr++;
		r->instr = malloc(r->ninstr * sizeof(struct zinstr));
		memcpy(r->instr, rtroutines[i].instr, r->ninstr * sizeof(struct zinstr));
	}

	r = routines[R_SRCFILENAME];
	for(i = 0; i < nsourcefile; i++) {
		zi = append_instr(r, Z_JE);
		zi->oper[0] = VALUE(REG_LOCAL+0);
		zi->oper[1] = SMALL(i);
		zi->branch = i + 1;
	}
	zi = append_instr(r, Z_PRINTLIT);
	zi->string = "?";
	zi = append_instr(r, Z_RFALSE);
	for(i = 0; i < nsourcefile; i++) {
		zi = append_instr(r, OP_LABEL(i + 1));
		zi = append_instr(r, Z_PRINTLIT);
		zi->string = sourcefile[i];
		zi = append_instr(r, Z_RFALSE);
	}

	init_backend_pred(find_builtin(prg, BI_PROGRAM_ENTRY)->pred)->global_label = make_routine_label();
	init_backend_pred(find_builtin(prg, BI_ERROR_ENTRY)->pred)->global_label = make_routine_label();

	init_backend_pred(find_builtin(prg, BI_GETINPUT)->pred)->global_label = R_GET_INPUT_PRED;
	init_backend_pred(find_builtin(prg, BI_GETRAWINPUT)->pred)->global_label = R_GET_RAW_INPUT_PRED;
	init_backend_pred(find_builtin(prg, BI_QUIT)->pred)->global_label = R_QUIT_PRED;
	init_backend_pred(find_builtin(prg, BI_SAVE)->pred)->global_label = R_SAVE_PRED;
	init_backend_pred(find_builtin(prg, BI_SAVE_UNDO)->pred)->global_label = R_SAVE_UNDO_PRED;
	init_backend_pred(find_builtin(prg, BI_SCRIPT_ON)->pred)->global_label = R_SCRIPT_ON_PRED;
	init_backend_pred(find_builtin(prg, BI_REPEAT)->pred)->global_label = R_REPEAT_PRED;
	init_backend_pred(find_builtin(prg, BI_OBJECT)->pred)->global_label = R_OBJECT_PRED;
	init_backend_pred(find_builtin(prg, BI_HASPARENT)->pred)->global_label = R_HASPARENT_PRED;
	init_backend_pred(find_builtin(prg, BI_IS_ONE_OF)->pred)->global_label = R_CONTAINS_PRED;
	init_backend_pred(find_builtin(prg, BI_SERIALNUMBER)->pred)->global_label = R_SERIALNUMBER_PRED;
	init_backend_pred(find_builtin(prg, BI_COMPILERVERSION)->pred)->global_label = R_COMPILERVERSION_PRED;
	init_backend_pred(find_builtin(prg, BI_MEMSTATS)->pred)->global_label = R_MEMSTATS_PRED;
	init_backend_pred(find_builtin(prg, BI_SPLIT)->pred)->global_label = R_SPLIT_PRED;
	init_backend_pred(find_builtin(prg, BI_STOP)->pred)->global_label = R_STOP_PRED;

	tracing_enabled = !!(find_builtin(prg, BI_TRACE_ON)->pred->flags & (PREDF_INVOKED_NORMALLY | PREDF_INVOKED_FOR_WORDS));

	backendwobj = calloc(prg->nworldobj, sizeof(struct backend_wobj));
	for(i = 0; i < prg->nworldobj; i++) {
		init_backend_wobj(prg, i, &backendwobj[i], strip);
	}

	for(i = 0; i < prg->npredicate; i++) {
		init_backend_pred(prg->predicates[i]->pred);
	}

	prepare_dictionary(prg);

	(void) make_global_label(); // ensure that the array is allocated

	if(prg->endings_root.nway) {
		if(verbose >= 2) printf("Stemming algorithm:\n");
		compile_endings_check(routines[R_TRY_STEMMING], &prg->endings_root, 0, 0);
	} else {
		zi = append_instr(routines[R_TRY_STEMMING], Z_RFALSE);
	}

	user_flags_global = next_user_global++;

	undoflag_global = alloc_user_flag(&undoflag_mask);

	assert(undoflag_global == 0);	// hardcoded in runtime_z.c
	assert(undoflag_mask == 1);	// hardcoded in runtime_z.c

	for(i = 0; i < prg->npredicate; i++) {
		predname = prg->predicates[i];
		pred = predname->pred;
		struct backend_pred *bp = pred->backend;
		dyn = pred->dynamic;

		// Reserve native z-machine flags for the dynamic per-object flags.
		if(dyn && predname->arity == 1 && !(pred->flags & (PREDF_GLOBAL_VAR | PREDF_FIXED_FLAG))) {
			if(verbose >= 2) {
				printf("Debug: Dynamic flag %d: %s", next_flag, predname->printed_name);
				if(dyn->linkage_flags & LINKF_SET) printf(" set");
				if(dyn->linkage_flags & LINKF_RESET) printf(" reset");
				if(dyn->linkage_flags & LINKF_LIST) printf(" list");
				if(dyn->linkage_flags & LINKF_CLEAR) printf(" clear");
				printf("\n");
			}
			if(next_flag >= NZOBJFLAG) {
				report(LVL_ERR, 0, "Too many dynamic per-object flags! Max %d.", NZOBJFLAG);
				exit(1);
			}
			bp->object_flag = next_flag++;
			if(dyn->linkage_flags & (LINKF_LIST | LINKF_CLEAR)) {
				bp->user_global = next_user_global++;
				bp->propbase_label = make_global_label();
				if(dyn->linkage_flags & LINKF_LIST) {
					bp->global_label = make_routine_label();
					pred->flags |= PREDF_INVOKED_SIMPLE | PREDF_INVOKED_MULTI;
				}
				if(dyn->linkage_flags & LINKF_SET) {
					bp->set_label = make_routine_label();
					compile_dynlinkage_set(pred, bp->set_label);
				}
				if(dyn->linkage_flags & (LINKF_RESET | LINKF_CLEAR)) {
					bp->clear_label = make_routine_label();
					compile_dynlinkage_clear(pred, bp->clear_label);
				}
#if 0
				printf("with linkage %d/%d/%d\n", bp->user_global, bp->objprop, bp->objpropslot);
#endif
			}
		}
	}

	for(i = 0; i < prg->npredicate; i++) {
		predname = prg->predicates[i];
		pred = predname->pred;
		struct backend_pred *bp = pred->backend;
		if(pred->nclause
		&& !(predname->builtin && !(predname->nameflags & PREDNF_DEFINABLE_BI))
		&& (!pred->dynamic || ((pred->flags & PREDF_FIXED_FLAG) && (pred->unbound_in & 1)))) {
			if(pred->flags & PREDF_INVOKED) {
				if(!bp->global_label) {
					bp->global_label = make_routine_label();
				}
			}
			if(tracing_enabled && bp->global_label) {
				bp->trace_output_label = make_routine_label();
			}
		}
		if((dyn = pred->dynamic) && predname->builtin != BI_HASPARENT) {
			if(predname->arity == 0) {
				bp->user_global = alloc_user_flag(&bp->user_flag_mask);
			} else if(predname->arity == 1) {
				if(pred->flags & PREDF_GLOBAL_VAR) {
#if 0
					printf("Global: %s\n", predname->printed_name);
#endif
					bp->user_global = next_user_global++;
				} else if(pred->flags & PREDF_FIXED_FLAG) {
					assert(!pred->dynamic->linkage_flags);
					if(verbose >= 2) {
						printf("Debug: %s flag %d: %s\n",
							(next_flag >= NZOBJFLAG)? "Extended fixed" : "Fixed",
							next_flag,
							predname->printed_name);
					}
					bp->object_flag = next_flag++;
				}
			} else if(predname->arity == 2) {
				bp->propbase_label = make_global_label();
			}
		}
	}

	if(next_flag > NZOBJFLAG) {
		n_extflag = (next_flag - NZOBJFLAG + 7) / 8;
		extflagreaders = malloc(n_extflag * sizeof(uint16_t));
		extflagarrays = malloc(n_extflag * sizeof(uint16_t));
		for(i = 0; i < n_extflag; i++) {
			extflagreaders[i] = make_routine_label();
			extflagarrays[i] = make_global_label();
			compile_extflag_reader(extflagreaders[i], extflagarrays[i]);
		}
	}

	for(i = 0; i < prg->npredicate; i++) {
		compile_predicate(prg->predicates[i], prg);
	}

#if 0
	printf("predicates compiled\n");
#endif

	nglobal = (REG_X - 0x10) + max_temp;
	user_global_base = nglobal;
	nglobal += next_user_global;

	resolve_rnum(R_ENTRY);
	for(i = 1; i < 64; i++) {
		if(propdefault[i] & 0x8000) {
			resolve_rnum(propdefault[i] & 0x7fff);
		}
	}
	for(i = 0; i < prg->nworldobj; i++) {
		wobj = &backendwobj[i];
		for(j = 63; j > 0; j--) {
			int n = wobj->npropword[j];
			for(k = 0; k < n; k++) {
				uint16_t value = wobj->propword[j][k];
				if(value & 0x8000) resolve_rnum(value & 0x7fff);
			}
		}
	}
	resolve_rnum(((struct backend_pred *) find_builtin(prg, BI_PROGRAM_ENTRY)->pred->backend)->global_label);
	resolve_rnum(((struct backend_pred *) find_builtin(prg, BI_ERROR_ENTRY)->pred->backend)->global_label);
	resolve_rnum(R_FAIL_PRED);

#if 0
	printf("routines traced\n");
#endif

	ifid = decode_metadata_str(BI_STORY_IFID, prg);
	if(!ifid) {
		if(linecount > 100) {
			report(LVL_WARN, 0, "No IFID declared.");
		}
	} else if(!match_template(ifid, ifid_template)) {
		report(LVL_WARN, find_builtin(prg, BI_STORY_IFID)->pred->clauses[0]->line, "Ignoring invalid IFID. It should have the format %s, where N is an uppercase hexadecimal digit.", ifid_template);
		ifid = 0;
	}

	author = decode_metadata_str(BI_STORY_AUTHOR, prg);
	if(!author && linecount > 100) {
		report(LVL_WARN, 0, "No author declared.");
	}
	title = decode_metadata_str(BI_STORY_TITLE, prg);
	if(!title && linecount > 100) {
		report(LVL_WARN, 0, "No title declared.");
	}
	noun = decode_metadata_str(BI_STORY_NOUN, prg);
	blurb = decode_metadata_str(BI_STORY_BLURB, prg);

	predname = find_builtin(prg, BI_STORY_RELEASE);
	if(predname && (pred = predname->pred)->nclause) {
		if(pred->clauses[0]->body || pred->clauses[0]->params[0]->kind != AN_INTEGER) {
			report(LVL_ERR, pred->clauses[0]->line, "Malformed story release declaration. Use e.g. (story release 1).");
			exit(1);
		}
		release = pred->clauses[0]->params[0]->value;
	} else {
		if(linecount > 100) {
			report(LVL_WARN, 0, "No release number declared.");
		}
	}

	addr_heap = 0x0040;
	addr_heapend = addr_heap + 2 * heapsize;
	assert(addr_heapend <= 0x7ffe);

	org = addr_heapend;

	addr_aux = org;
	org += auxsize * 2;

	addr_lts = org;
	org += ltssize * 2;

	addr_objtable = org;
	org += 63 * 2;

	for(i = 0; i < prg->nworldobj; i++) {
		wobj = &backendwobj[i];
		wobj->addr_objtable = org;
		org += 14;
	}

	for(i = 0; i < prg->nworldobj; i++) {
		wobj = &backendwobj[i];

		wobj->addr_proptable = org;
		org += 1 + wobj->n_encoded * 2;
		for(j = 63; j >= 1; j--) {
			if(wobj->npropword[j] == 1) {
				org += 1 + 2;
			} else if(wobj->npropword[j]) {
				org += 2 + 2 * wobj->npropword[j];
			}
		}
		org++;
	}

	addr_seltable = org;
	org += next_select;

	for(i = 0; i < n_extflag; i++) {
		set_global_label(extflagarrays[i], org - 1);
		org += prg->nworldobj;
	}

	if(org & 1) org++;

	addr_globals = org;
	org += nglobal * 2;

	for(i = 0; i < prg->npredicate; i++) {
		predname = prg->predicates[i];
		pred = predname->pred;
		struct backend_pred *bp = pred->backend;
		if(pred->dynamic
		&& predname->builtin != BI_HASPARENT
		&& (predname->arity == 2 || (pred->dynamic->linkage_flags & (LINKF_LIST | LINKF_CLEAR)))) {
			assert(bp->propbase_label);
			set_global_label(bp->propbase_label, org - 2); // -2 because it will be indexed by 1-based object id
			org += prg->nworldobj * 2;
		} else {
			assert(!bp->propbase_label);
		}
	}

	addr_abbrevstr = org;
	org += 2;

	addr_abbrevtable = org;
	org += 96 * 2;

	if(org < addr_globals + 2*240) {
		// Gargoyle complains if there isn't room for 240 globals in dynamic memory.
		org = addr_globals + 2*240;
	}

	addr_static = org;

	for(i = 0; i < nscantable; i++) {
		set_global_label(scantable[i].label, org);
		org += scantable[i].length * 2;
	}

	addr_dictionary = org;
	org += 4 + NSTOPCHAR + ndict * 6;

	if(org > 0xfff8) {
		report(LVL_ERR, 0, "Base memory exhausted. Decrease heap/aux/long-term size using commandline options -H, -A, and/or -L.");
		exit(1);
	}

	org = (org + 7) & ~7;
	himem = org;
	entrypc = org + 1;

	set_global_label(G_HEAPBASE, addr_heap);
	set_global_label(G_HEAPEND, addr_heapend);
	set_global_label(G_HEAPSIZE, (addr_heapend - addr_heap) / 2);
	set_global_label(G_AUXBASE, addr_aux);
	set_global_label(G_AUXSIZE, auxsize);
	set_global_label(G_LTBASE, addr_lts);
	set_global_label(G_LTBASE2, addr_lts + 2);
	set_global_label(G_LTSIZE, ltssize);
	set_global_label(G_USER_GLOBALS, addr_globals + user_global_base * 2);
	set_global_label(G_TEMPSPACE_REGISTERS, addr_globals + (REG_TEMP - 0x10) * 2);
	set_global_label(G_ARG_REGISTERS, addr_globals + (REG_A - 0x10) * 2);
	set_global_label(G_DICT_TABLE, addr_dictionary + 4 + NSTOPCHAR);
	set_global_label(G_OBJECT_ID_END, 1 + prg->nworldobj);
	set_global_label(G_SELTABLE, addr_seltable);

	assert(REG_SPACE == REG_TEMP + 1);

#if 0
	printf("pass 1 begins\n");
#endif

	org = entrypc - 1;
	for(i = 0; i < next_routine_num; i++) {
		if(i == R_TERPTEST) continue; // put a directly-called routine last, to help txd
		if(routines[i]->actual_routine == routines[R_FAIL_PRED]->actual_routine) {
			routines[i]->address = 0;
		} else if(routines[i]->actual_routine == i) {
			assert((org & (packfactor - 1)) == 0);
			routines[i]->address = org / packfactor;
			org += pass1(routines[i], org);
			org = (org + packfactor - 1) & ~(packfactor - 1);
		}
	}

	assert((org & (packfactor - 1)) == 0);
	routines[R_TERPTEST]->address = org / packfactor;
	org += pass1(routines[R_TERPTEST], org);
	org = (org + packfactor - 1) & ~(packfactor - 1);

#if 0
	printf("pass 1 complete\n");
#endif

	set_global_label(
		G_PROGRAM_ENTRY,
		routines[resolve_rnum(((struct backend_pred *) find_builtin(prg, BI_PROGRAM_ENTRY)->pred->backend)->global_label)]->address);
	set_global_label(
		G_ERROR_ENTRY,
		routines[resolve_rnum(((struct backend_pred *) find_builtin(prg, BI_ERROR_ENTRY)->pred->backend)->global_label)]->address);

	for(i = 0; i < BUCKETS; i++) {
		for(gs = stringhash[i]; gs; gs = gs->next) {
			uint8_t pentets[MAXSTRING * 3];
			uint16_t words[MAXSTRING];
			int n;

			set_global_label(gs->global_label, org / packfactor);

			n = encode_chars(pentets, sizeof(pentets), 0, gs->zscii);
			assert(n <= sizeof(pentets));
			n = pack_pentets(words, pentets, n);

			org += n * 2;
			org = (org + packfactor - 1) & ~(packfactor - 1);
		}
	}

	filesize = org;
	if(filesize >= ((zversion == 5)? (1<<18) : (1<<19))) {
		report(LVL_ERR, 0, "Story too big for selected output format!");
		exit(1);
	}
	zcore = calloc(1, filesize);

	zcore[0x00] = zversion;
	zcore[0x02] = release >> 8;
	zcore[0x03] = release & 0xff;
	zcore[0x04] = himem >> 8;
	zcore[0x05] = himem & 0xff;
	zcore[0x06] = entrypc >> 8;
	zcore[0x07] = entrypc & 0xff;
	zcore[0x08] = addr_dictionary >> 8;
	zcore[0x09] = addr_dictionary & 0xff;
	zcore[0x0a] = addr_objtable >> 8;
	zcore[0x0b] = addr_objtable & 0xff;
	zcore[0x0c] = addr_globals >> 8;
	zcore[0x0d] = addr_globals & 0xff;
	zcore[0x0e] = addr_static >> 8;
	zcore[0x0f] = addr_static & 0xff;
	zcore[0x11] = 0x10;	// flags2: need undo
	zcore[0x18] = addr_abbrevtable >> 8;
	zcore[0x19] = addr_abbrevtable & 0xff;
	zcore[0x1a] = (filesize / packfactor) >> 8;
	zcore[0x1b] = (filesize / packfactor) & 0xff;
	//zcore[0x2e] = addr_termchar >> 8;
	//zcore[0x2f] = addr_termchar & 0xff;
	zcore[0x39] = 'D';
	zcore[0x3a] = 'i';
	zcore[0x3b] = 'a';
	zcore[0x3c] = VERSION[0];
	zcore[0x3d] = VERSION[1];
	assert('/' == VERSION[2]);
	zcore[0x3e] = VERSION[3];
	zcore[0x3f] = VERSION[4];

	init_abbrev(addr_abbrevstr, addr_abbrevtable);

	for(i = 1; i < 64; i++) {
		uint16_t addr = addr_objtable + (i - 1) * 2;
		uint16_t value = propdefault[i];
		if(value & 0x8000) value = routines[resolve_rnum(value & 0x7fff)]->address;
		zcore[addr++] = value >> 8;
		zcore[addr++] = value & 0xff;
	}

	memset(zcore + addr_lts, 0x3f, ltssize * 2);

	initial_values(prg, zcore, addr_globals, addr_lts, ltssize, extflagarrays, n_extflag);

	for(i = 0; i < prg->nworldobj; i++) {
		wobj = &backendwobj[i];
		zcore[wobj->addr_objtable + 12] = wobj->addr_proptable >> 8;
		zcore[wobj->addr_objtable + 13] = wobj->addr_proptable & 0xff;
		zcore[wobj->addr_proptable + 0] = wobj->n_encoded;
		for(j = 0; j < wobj->n_encoded; j++) {
			zcore[wobj->addr_proptable + 1 + j * 2 + 0] = wobj->encoded_name[j] >> 8;
			zcore[wobj->addr_proptable + 1 + j * 2 + 1] = wobj->encoded_name[j] & 0xff;
		}
		uint16_t addr = wobj->addr_proptable + 1 + 2 * wobj->n_encoded;
		for(j = 63; j > 0; j--) {
			int n = wobj->npropword[j];
			if(n) {
				if(n == 1) {
					zcore[addr++] = 0x40 | j;
				} else {
					zcore[addr++] = 0x80 | j;
					zcore[addr++] = 0x80 | ((2 * n) & 63);
				}
				for(k = 0; k < n; k++) {
					uint16_t value = wobj->propword[j][k];
					if(value & 0x8000) value = routines[resolve_rnum(value & 0x7fff)]->address;
					zcore[addr++] = value >> 8;
					zcore[addr++] = value & 0xff;
				}
			}
		}
	}

	for(i = 0; i < nscantable; i++) {
		uint16_t addr = global_labels[scantable[i].label];
		for(j = 0; j < scantable[i].length; j++) {
			zcore[addr++] = scantable[i].value[j] >> 8;
			zcore[addr++] = scantable[i].value[j] & 0xff;
		}
	}

	zcore[addr_dictionary + 0] = NSTOPCHAR;
	for(i = 0; i < NSTOPCHAR; i++) {
		zcore[addr_dictionary + 1 + i] = STOPCHARS[i];
	}
	zcore[addr_dictionary + 1 + NSTOPCHAR + 0] = 6;
	zcore[addr_dictionary + 1 + NSTOPCHAR + 1] = ndict >> 8;
	zcore[addr_dictionary + 1 + NSTOPCHAR + 2] = ndict & 0xff;
	for(i = 0; i < ndict; i++) {
		for(j = 0; j < 3; j++) {
			zcore[addr_dictionary + 4 + NSTOPCHAR + i * 6 + j * 2 + 0] = dictionary[i].encoded[j] >> 8;
			zcore[addr_dictionary + 4 + NSTOPCHAR + i * 6 + j * 2 + 1] = dictionary[i].encoded[j] & 0xff;
		}
	}

	set_initial_reg(zcore + addr_globals, REG_NIL, 0x1fff);
	set_initial_reg(zcore + addr_globals, REG_3FFF, 0x3fff);
	set_initial_reg(zcore + addr_globals, REG_FFFF, 0xffff);
	set_initial_reg(zcore + addr_globals, REG_2000, 0x2000);
	set_initial_reg(zcore + addr_globals, REG_4000, 0x4000);
	set_initial_reg(zcore + addr_globals, REG_8000, 0x8000);
	set_initial_reg(zcore + addr_globals, REG_C000, 0xc000);
	set_initial_reg(zcore + addr_globals, REG_E000, 0xe000);

	get_timestamp(compiletime, reldate);
	for(i = 0; i < 6; i++) zcore[0x12 + i] = compiletime[i];

	memset(zcore + addr_heap, 0x1f, heapsize * 2);
	memset(zcore + addr_aux, 0x3f, auxsize * 2);

	if(ifid) {
		assert(strlen(ifid) == 36);
		memcpy(zcore + addr_heap, "UUID://", 7);
		memcpy(zcore + addr_heap + 7, ifid, 36);
		memcpy(zcore + addr_heap + 7 + 36, "//", 3);
	}

	for(i = 0; i < next_routine_num; i++) {
		if(routines[i]->actual_routine == routines[R_FAIL_PRED]->actual_routine) {
			/* skip */
		} else if(routines[i]->actual_routine == i) {
			uint32_t addr = routines[i]->address * packfactor;
			zcore[addr++] = routines[i]->nlocal;
			assemble(addr, routines[i]);
		}
	}

	for(i = 0; i < BUCKETS; i++) {
		for(gs = stringhash[i]; gs; gs = gs->next) {
			uint8_t pentets[MAXSTRING * 3];
			uint16_t words[MAXSTRING];
			int n;
			uint32_t addr = global_labels[gs->global_label] * packfactor;

			n = encode_chars(pentets, sizeof(pentets), 0, gs->zscii);
			assert(n <= sizeof(pentets));
			n = pack_pentets(words, pentets, n);

			for(j = 0; j < n; j++) {
				zcore[addr++] = words[j] >> 8;
				zcore[addr++] = words[j] & 0xff;
			}
		}
	}

	for(i = 0x40; i < filesize; i++) {
		checksum += zcore[i];
	}
	zcore[0x1c] = checksum >> 8;
	zcore[0x1d] = checksum & 0xff;

	report(LVL_DEBUG, 0, "Heap: %d words", heapsize);
	report(LVL_DEBUG, 0, "Auxiliary heap: %d words", auxsize);
	report(LVL_DEBUG, 0, "Long-term heap: %d words", ltssize);
	report(LVL_DEBUG, 0, "Global registers used: %d of 240", nglobal);
	report(LVL_DEBUG, 0, "Properties used: %d of 63", next_free_prop - 1);
	if(next_flag > NZOBJFLAG) {
		report(LVL_DEBUG, 0, "Flags used: %d native, %d extended", NZOBJFLAG, next_flag - NZOBJFLAG);
	} else {
		report(LVL_DEBUG, 0, "Flags used: %d native", next_flag);
	}

	if(!strcmp(format, "z8") || !strcmp(format, "z5")) {
		FILE *f = fopen(filename, "wb");
		if(!f) {
			report(LVL_ERR, 0, "Error opening \"%s\" for output: %s", filename, strerror(errno));
			exit(1);
		}

		if(1 != fwrite(zcore, filesize, 1, f)) {
			report(LVL_ERR, 0, "Error writing to \"%s\": %s", filename, strerror(errno));
			exit(1);
		}

		fclose(f);

		if(coverfname || coveralt) {
			report(LVL_WARN, 0, "Ignoring cover image options for the %s output format.", format);
		}
	} else if(!strcmp(format, "zblorb")) {
		emit_blorb(filename, zcore, filesize, ifid, compiletime, author, title, noun, blurb, release, reldate, coverfname, coveralt);
	} else {
		assert(0);
		exit(1);
	}

	if(tracing_enabled) {
		report(LVL_NOTE, 0, "In this build, the code has been instrumented to allow tracing.");
	}
}

void usage(char *prgname) {
	fprintf(stderr, "Dialog compiler " VERSION ".\n");
	fprintf(stderr, "Copyright 2018-2019 Linus Akesson.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage: %s [options] [source code filename ...]\n", prgname);
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "--version   -V    Display the program version.\n");
	fprintf(stderr, "--help      -h    Display this information.\n");
	fprintf(stderr, "--verbose   -v    Increase verbosity (may be used multiple times).\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "--output    -o    Set output filename.\n");
	fprintf(stderr, "--format    -t    Set output format (zblorb, z8, or z5).\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "--heap      -H    Set main heap size (default 1000 words).\n");
	fprintf(stderr, "--aux       -A    Set aux heap size (default 500 words).\n");
	fprintf(stderr, "--long-term -L    Set long-term heap size (default 500 words).\n");
	fprintf(stderr, "--strip     -s    Strip internal object names.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Only for zblorb format:\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "--cover     -c    Cover image filename (PNG, max 1200x1200).\n");
	fprintf(stderr, "--cover-alt -a    Textual description of cover image.\n");
	exit(1);
}

int main(int argc, char **argv) {
	struct option longopts[] = {
		{"help", 0, 0, 'h'},
		{"version", 0, 0, 'V'},
		{"verbose", 0, 0, 'v'},
		{"output", 1, 0, 'o'},
		{"format", 1, 0, 't'},
		{"cover", 1, 0, 'c'},
		{"cover-alt", 1, 0, 'a'},
		{"heap", 1, 0, 'H'},
		{"aux", 1, 0, 'A'},
		{"long-term", 1, 0, 'L'},
		{"strip", 0, 0, 's'},
		{0, 0, 0, 0}
	};

	char *prgname = argv[0];
	char *outname = 0;
	char *format = "zblorb";
	char *coverfname = 0;
	char *coveralt = 0;
	int auxsize = 500, heapsize = 1000, ltssize = 500;
	int strip = 0;
	int opt, i;
	struct program *prg;

	comp_init();

	do {
		opt = getopt_long(argc, argv, "?hVvo:t:c:a:H:A:L:s", longopts, 0);
		switch(opt) {
			case 0:
			case '?':
			case 'h':
				usage(prgname);
				break;
			case 'V':
				fprintf(stderr, "Dialog compiler " VERSION "\n");
				exit(0);
			case 'v':
				verbose++;
				break;
			case 'o':
				outname = strdup(optarg);
				break;
			case 't':
				format = strdup(optarg);
				break;
			case 'c':
				coverfname = strdup(optarg);
				break;
			case 'a':
				coveralt = strdup(optarg);
				break;
			case 'H':
				heapsize = strtol(optarg, 0, 10);
				if(heapsize < 23 || heapsize > 8192) {
					report(LVL_ERR, 0, "Bad main heap size (max 8192 words)");
					exit(1);
				}
				break;
			case 'A':
				auxsize = strtol(optarg, 0, 10);
				if(auxsize < 1 || auxsize > 16351) {
					report(LVL_ERR, 0, "Bad aux heap size (max 16351 words)");
					exit(1);
				}
				break;
			case 'L':
				ltssize = strtol(optarg, 0, 10);
				if(ltssize < 1 || ltssize > 16351) {
					report(LVL_ERR, 0, "Bad long-term heap size (max 16351 words)");
					exit(1);
				}
				break;
			case 's':
				strip = 1;
				break;
			default:
				if(opt >= 0) {
					report(LVL_ERR, 0, "Unimplemented option '%c'", opt);
					exit(1);
				}
				break;
		}
	} while(opt >= 0);

	if(strcmp(format, "zblorb")
	&& strcmp(format, "z8")
	&& strcmp(format, "z5")) {
		report(LVL_ERR, 0, "Unsupported output format \"%s\".", format);
		exit(1);
	}

	if(!outname) {
		if(optind < argc) {
			outname = malloc(strlen(argv[optind]) + 8);
			strcpy(outname, argv[optind]);
			for(i = strlen(outname) - 1; i >= 0; i--) {
				if(outname[i] == '.') break;
			}
			if(i < 0) {
				i = strlen(outname);
			}
			outname[i++] = '.';
			strcpy(outname + i, format);
		} else {
			report(LVL_ERR, 0, "No output filename specified, and none can be deduced from the input filenames.");
			exit(1);
		}
	}

	prg = new_program();
	frontend_add_builtins(prg);
	prg->optflags |= OPTF_BOUND_PARAMS;

	if(!frontend(prg, argc - optind, argv + optind)) {
		exit(1);
	}

	backend(outname, format, coverfname, coveralt, heapsize, auxsize, ltssize, strip, prg->totallines, prg);

	free_program(prg);

	return 0;
}
