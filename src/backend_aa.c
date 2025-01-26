#include <assert.h>
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
#include "backend_aa.h"
#include "report.h"
#include "unicode.h"
#include "compile.h"
#include "eval.h"
#include "aavm.h"
#include "crc32.h"

#define MAXWORDLEN 255
#define TWEAK_BINSEARCH 16

#define AAFAIL 0xffffff

#define COMPILERVERSION ("Dialog compiler version " VERSION)

#define NSTOPCHAR strlen(STOPCHARS)

struct charmap {
	uint32_t	glyph;
	uint8_t		tolower;
	uint8_t		toupper;
};

struct dictentry {
	struct word	*word;
	uint8_t		*chars;
};

struct datatable {
	uint16_t		address;
	uint16_t		length;
	uint8_t			*data;
};

struct wordtable {
	uint16_t		length;
	uint16_t		*words;
};

struct textstring {
	uint16_t		length;
	uint16_t		occurrences;
	uint32_t		address;
	uint16_t		bitlength;
	uint8_t			*chars;
};

struct segment {
	int			ninstr;
	struct aainstr		*instr;
	uint8_t			visited;
};

static struct charmap charmap[128];
static int ncharmap;
static uint32_t charbits[129];	// for chars 20..a0 where 7f is extended and a0 is end, lsb first, set stop bit
static uint8_t decodetable[128][2];
static int n_decodetable;

static struct dictentry *dictionary;
static int ndict;

static int first_sel_byte;
static int first_gvar;
static int first_ovar;
static int n_glb_word, n_obj_word;

static uint16_t *dynlink_id;
static int n_dynlink;

static int heap_sz, aux_sz, fixed_sz, longterm_sz;

static struct aainstr *aainstr;
static int ninstr, nalloc_instr;

static int nextlabel;

static struct datatable *datatable;
static int ndatatable;
static uint16_t datatable_org;

static struct wordtable *wordtable;
static int nwordtable;

static uint8_t *initial_sel;

static struct arena aa_arena;

static struct textstring *textstrings;
static int n_textstr, nalloc_textstr;
static int writ_size;
static int decode_esc_bits, decode_esc_boundary;

static char *tracelabels[N_TR_KIND];

static int code_sz;
static uint32_t *symbols;

static uint32_t aatotalsize;

static int *altstring;

static struct segment *segment;
static int nsegment, nalloc_segment;

static int cmp_aadict(const void *a, const void *b) {
	const struct dictentry *aa = a;
	const struct dictentry *bb = b;
	int i;

	for(i = 0; aa->chars[i] && bb->chars[i]; i++) {
		if(aa->chars[i] != bb->chars[i]) {
			return aa->chars[i] - bb->chars[i];
		}
	}
	return aa->chars[i] - bb->chars[i];
}

static int cmp_aadict_len(const void *a, const void *b) {
	const uint16_t *aa = a;
	const uint16_t *bb = b;
	int diff;

	diff = strlen((char *) dictionary[*bb].chars) - strlen((char *) dictionary[*aa].chars);
	if(!diff) diff = cmp_aadict(&dictionary[*aa], &dictionary[*bb]);

	return diff;
}

static uint8_t resolve_aachar(uint32_t uchar) {
	int i;

	if(uchar < 127) {
		return uchar;
	} else {
		for(i = 0; i < ncharmap; i++) {
			if(charmap[i].glyph == uchar) {
				return 128 + i;
			}
		}

		if(i >= 128) {
			report(LVL_ERR, 0, "Too many distinct unicode characters in the text.");
			exit(1);
		}

		charmap[i].glyph = uchar;
		ncharmap++;

		return 128 + i;
	}
}

static uint8_t aachar_to_lower(uint8_t ch) {
	if(ch >= 'A' && ch <= 'Z') {
		return ch - 'A' + 'a';
	} else if(ch >= 128) {
		charmap[ch - 128].tolower = resolve_aachar(unicode_to_lower(charmap[ch - 128].glyph));
		return charmap[ch - 128].tolower;
	} else {
		return ch;
	}
}

static int utf8_to_aabytes(uint8_t *dest, int ndest, uint8_t *src) {
	uint16_t ubuf[ndest];
	int nsrc;
	int i;

	nsrc = utf8_to_unicode(ubuf, ndest - 1, src);
	for(i = 0; ubuf[i]; i++) {
		dest[i] = resolve_aachar(ubuf[i]);
	}
	dest[i] = 0;

	return nsrc;
}

static struct aainstr *add_instr(uint8_t op) {
	if(ninstr >= nalloc_instr) {
		nalloc_instr = (2 * nalloc_instr) + 8;
		aainstr = realloc(aainstr, nalloc_instr * sizeof(struct aainstr));
		memset(aainstr + ninstr, 0, (nalloc_instr - ninstr) * sizeof(struct aainstr));
	}

	aainstr[ninstr].op = op;

	return &aainstr[ninstr++];
}

static void add_label(uint32_t lab) {
	struct aainstr *ai;

	ai = add_instr(AA_LABEL);
	ai->oper[0] = (aaoper_t) {AAO_LABEL, lab};
}

void prepare_dictionary_aa(struct program *prg) {
	int i, n;
	char chbuf[2];
	struct word *w;
	char runtime[7] = {8, 13, 32, 16, 17, 18, 19};
	uint16_t ubuf[MAXWORDLEN];
	uint8_t aabuf[MAXWORDLEN];

	for(i = 0; i < 7; i++) {
		chbuf[0] = runtime[i];
		chbuf[1] = 0;
		w = find_word(prg, chbuf);
		ensure_dict_word(prg, w);
	}

	if(prg->ndictword > 0x1e00) {
		report(LVL_ERR, 0, "Too many dictionary words.");
		exit(1);
	}

	prg->dictmap = arena_calloc(&prg->arena, prg->ndictword * sizeof(uint16_t));

	ndict = prg->ndictword;
	dictionary = calloc(ndict, sizeof(*dictionary));

	for(i = 0; i < ndict; i++) {
		w = prg->dictwordnames[i];
		assert(w->dict_id < prg->ndictword);
		assert(w->name[0]);
		(void) utf8_to_unicode(ubuf, MAXWORDLEN, (uint8_t *) w->name);
		for(n = 0; ubuf[n]; n++) {
			aabuf[n] = aachar_to_lower(resolve_aachar(ubuf[n]));
		}
		dictionary[i].word = w;
		dictionary[i].chars = arena_alloc(&prg->arena, n + 1);
		memcpy(dictionary[i].chars, aabuf, n);
		dictionary[i].chars[n] = 0;
	}

	qsort(dictionary, ndict, sizeof(struct dictentry), cmp_aadict);

	for(i = 0; i < ndict; ) {
		w = dictionary[i].word;
		if(!dictionary[i].chars[1]) {
			prg->dictmap[w->dict_id] = 0x3e00 | dictionary[i].chars[0];
			memmove(dictionary + i, dictionary + i + 1, (ndict - i - 1) * sizeof(struct dictentry));
			ndict--;
		} else {
			prg->dictmap[w->dict_id] = 0x2000 | i;
			if(i < ndict - 1 && !cmp_aadict(dictionary + i, dictionary + i + 1)) {
				report(LVL_INFO, 0, "Consolidating dictionary words \"%s\" and \"%s\".",
					dictionary[i].word->name,
					dictionary[i + 1].word->name);
				prg->dictmap[dictionary[i + 1].word->dict_id] = 0x2000 | i;
				memmove(dictionary + i, dictionary + i + 1, (ndict - i - 1) * sizeof(struct dictentry));
				ndict--;
			} else {
				i++;
			}
		}
	}
}

static void putbyte_crc(uint8_t b, FILE *f, uint32_t *crc) {
	fputc(b, f);
	*crc = crc32_table[((*crc) & 0xff) ^ b] ^ ((*crc) >> 8);
}

static void putword_crc(uint16_t w, FILE *f, uint32_t *crc) {
	putbyte_crc(w >> 8, f, crc);
	putbyte_crc(w & 0xff, f, crc);
}

static void putword(uint16_t w, FILE *f) {
	fputc(w >> 8, f);
	fputc(w & 0xff, f);
}

static int chunkheader(FILE *f, char *id, uint32_t size) {
	int pad = size & 1;

	if(ftell(f) != 8 + aatotalsize) {
		report(LVL_ERR, 0, "Unexpected file position when writing %s chunk. Disk full?", id);
		exit(1);
	}

	if(verbose >= 2) {
		fprintf(stderr, "Chunk %s: %8d bytes, %4d kB\n", id, size, (size + 512) / 1024);
	}
	aatotalsize += 8 + size + pad;

	fwrite(id, 4, 1, f);
	fputc((size >> 24) & 0xff, f);
	fputc((size >> 16) & 0xff, f);
	fputc((size >> 8) & 0xff, f);
	fputc((size >> 0) & 0xff, f);

	return pad;
}

static void chunk_dict(FILE *f, uint32_t *crc) {
	uint16_t refs[ndict];
	uint16_t offset[ndict];
	uint8_t jumble[65536];
	int pos = 0;
	int i, j, len, pad;
	struct dictentry *dict;

	for(i = 0; i < ndict; i++) {
		refs[i] = i;
	}
	qsort(refs, ndict, sizeof(uint16_t), cmp_aadict_len);

	for(i = 0; i < ndict; i++) {
		dict = &dictionary[refs[i]];
		len = strlen((char *) dict->chars);
		assert(len >= 2);
		assert(len <= 255);
		for(j = 0; j < pos - len; j++) {
			if(!memcmp(jumble + j, dict->chars, len)) {
				break;
			}
		}
		if(j < pos - len) {
			offset[refs[i]] = 2 + 3 * ndict + j;
		} else {
			if(2 + 3 * ndict + pos + len > 65536) {
				report(LVL_ERR, 0, "Dictionary too large.");
				exit(1);
			}
			offset[refs[i]] = 2 + 3 * ndict + pos;
			memcpy(jumble + pos, dict->chars, len);
			pos += len;
		}
	}

	pad = chunkheader(f, "DICT", 2 + 3 * ndict + pos);
	putword_crc(ndict, f, crc);
	for(i = 0; i < ndict; i++) {
		putbyte_crc(strlen((char *) dictionary[i].chars), f, crc);
		putword_crc(offset[i], f, crc);
	}
	for(i = 0; i < pos; i++) {
		putbyte_crc(jumble[i], f, crc);
	}
	if(pad) fputc(0, f);
}

static void chunk_tags(FILE *f, struct program *prg) {
	int i, pad;
	uint32_t size = 2 + prg->nworldobj * 2;
	uint8_t aabuf[256];
	uint16_t offset[prg->nworldobj];

	for(i = 0; i < prg->nworldobj; i++) {
		if(prg->worldobjnames[i]->name[utf8_to_aabytes(
			aabuf,
			sizeof(aabuf),
			(uint8_t *) prg->worldobjnames[i]->name)])
		{
			report(LVL_ERR, 0, "Object name too long.");
			exit(1);
		}
		offset[i] = size;
		size += strlen((char *) aabuf) + 1;
		if(size > 65536) {
			report(LVL_ERR, 0, "Overflow in table of object names.");
			exit(1);
		}
	}

	pad = chunkheader(f, "TAGS", size);
	putword(prg->nworldobj, f);
	for(i = 0; i < prg->nworldobj; i++) {
		putword(offset[i], f);
	}
	for(i = 0; i < prg->nworldobj; i++) {
		utf8_to_aabytes(aabuf, sizeof(aabuf), (uint8_t *) prg->worldobjnames[i]->name);
		fwrite(aabuf, strlen((char *) aabuf) + 1, 1, f);
	}
	if(pad) fputc(0, f);
}

static uint16_t tag_eval_value(value_t v, struct program *prg) {
	switch(v.tag) {
	case VAL_NUM:
		assert(v.value >= 0);
		assert(v.value <= 0x3fff);
		return 0x4000 + v.value;
	case VAL_OBJ:
		assert(v.value < prg->nworldobj);
		assert(v.value < 0x1fff);
		return 1 + v.value;
	case VAL_DICT:
		assert(prg->dictmap[v.value] != 0xffff);
		return prg->dictmap[v.value];
	case VAL_NIL:
		return 0x3f00;
	case VAL_NONE:
		return 0x0000;
	case VAL_RAW:
		return v.value;
	}
	printf("%d\n", v.tag);
	assert(0); exit(1);
}

static int render_eval_value(uint16_t *buffer, int nword, value_t v, struct eval_state *es, struct predname *predname, int rec_depth) {
	int count, size = 1, n;
	uint16_t value;

	// Simple elements (including the empty list) are serialised as themselves.
	// Proper lists are serialised as the elements, followed by c000+n.
	// Improper lists are serialised as the elements, followed by the improper tail element, followed by e000+n.

	if(rec_depth > 8) {
		report(
			LVL_ERR,
			0,
			"Initial value of global variable %s is too complex.",
			predname->printed_name);
		exit(1);
	}

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
			n = render_eval_value(buffer, nword, eval_gethead(v, es), es, predname, rec_depth + 1);
			size += n;
			nword -= n;
			buffer += n;
			count++;
			v = eval_gettail(v, es);
			if(v.tag == VAL_NIL) {
				value = 0xc000 | count;
				break;
			} else if(v.tag != VAL_PAIR) {
				n = render_eval_value(buffer, nword, v, es, predname, rec_depth + 1);
				size += n;
				nword -= n;
				buffer += n;
				value = 0xe000 | count;
				break;
			}
		}
		break;
	case VAL_REF:
		report(
			LVL_ERR,
			0,
			"Initial value of dynamic variable %s must be bound.",
			predname->printed_name);
		exit(1);
	default:
		assert(0); exit(1);
	}

	if(nword <= 0) {
		report(
			LVL_ERR,
			0,
			"Cannot fit initial values into long-term heap. Use commandline option -L to enlarge it.");
		exit(1);
	}

	buffer[0] = value;
	return size;
}

static int initial_values(struct program *prg, uint16_t *core, int ltt) {
	struct eval_state es;
	struct predname *predname;
	struct predicate *pred;
	int i, j;
	int baseaddr, addr, size, status;
	value_t eval_args[2], obj1, obj2;
	int16_t initialparent[prg->nworldobj];
	uint8_t seen[prg->nworldobj];
	int more;

	init_evalstate(&es, prg);

	for(i = 0; i < prg->npredicate; i++) {
		predname = prg->predicates[i];
		pred = predname->pred;
		if(pred->flags & PREDF_DYNAMIC) {
			if(predname->arity == 0) {
				eval_reinitialize(&es);
				if(eval_initial(&es, predname, 0)) {
					core[AA_N_INITREG + 1 + prg->nworldobj + predname->dyn_id / 16] |= 0x8000 >> (predname->dyn_id & 15);
				}
				if(es.errorflag) exit(1);
			} else if(predname->arity == 1) {
				if(pred->flags & PREDF_GLOBAL_VAR) {
					eval_reinitialize(&es);
					eval_args[0] = eval_makevar(&es);
					if(eval_initial(&es, predname, eval_args)) {
						addr = 1 + prg->nworldobj + first_gvar + predname->dyn_var_id;
						if(eval_args[0].tag == VAL_REF) {
							report(
								LVL_ERR,
								0,
								"Initial value of global variable %s must be bound.",
								predname->printed_name);
							exit(1);
						} else if(eval_args[0].tag == VAL_PAIR) {
							core[AA_N_INITREG + addr] = 0x8000 | ltt;
							core[AA_N_INITREG + ltt + 1] = addr;
							size = 2 + render_eval_value(
								core + AA_N_INITREG + ltt + 2,
								fixed_sz + longterm_sz - ltt - 2,
								eval_args[0],
								&es,
								predname,
								0);
							core[AA_N_INITREG + ltt + 0] = size;
							ltt += size;
						} else {
							core[AA_N_INITREG + addr] = tag_eval_value(eval_args[0], prg);
						}
					}
					if(es.errorflag) exit(1);
				} else {
					int last_wobj = 0;

					for(j = 0; j < prg->nworldobj; j++) {
						if(pred->flags & PREDF_FIXED_FLAG) {
							assert(predname->nfixedvalue >= prg->nworldobj);
							status = predname->fixedvalues[j];
						} else {
							eval_reinitialize(&es);
							eval_args[0] = (value_t) {VAL_OBJ, j};
							status = eval_initial(&es, predname, eval_args);
							if(es.errorflag) exit(1);
						}
						if(status) {
							core[AA_N_INITREG + 1 + prg->nworldobj + n_glb_word + j * n_obj_word + 3 + predname->dyn_id / 16] |= 0x8000 >> (predname->dyn_id & 15);
							if(pred->flags & PREDF_DYN_LINKAGE) {
								core[AA_N_INITREG + 1 + prg->nworldobj + n_glb_word + j * n_obj_word + first_ovar + (prg->nobjvar - 1) + dynlink_id[predname->dyn_id]] = last_wobj;
								last_wobj = j + 1;
							}
						}
					}
					if(pred->flags & PREDF_DYN_LINKAGE) {
						core[AA_N_INITREG + 1 + prg->nworldobj + first_gvar + prg->nglobalvar + dynlink_id[predname->dyn_id]] = last_wobj;
					}
				}
			} else if(predname->arity == 2 && predname->builtin != BI_HASPARENT) {
				for(j = 0; j < prg->nworldobj; j++) {
					eval_reinitialize(&es);
					eval_args[0] = (value_t) {VAL_OBJ, j};
					eval_args[1] = eval_makevar(&es);
					assert(predname->dyn_id);
					if(eval_initial(&es, predname, eval_args)) {
						addr = 1 + prg->nworldobj + n_glb_word + j * n_obj_word + first_ovar + (predname->dyn_id - 1);
						if(eval_args[1].tag == VAL_REF) {
							report(
								LVL_ERR,
								0,
								"Initial value of dynamic per-object variable %s, for #%s, must be bound.",
								predname->printed_name,
								prg->worldobjnames[j]->name);
						} else if(eval_args[1].tag == VAL_PAIR) {
							core[AA_N_INITREG + addr] = 0x8000 | ltt;
							core[AA_N_INITREG + ltt + 1] = addr;
							size = 2 + render_eval_value(
								core + AA_N_INITREG + ltt + 2,
								fixed_sz + longterm_sz - ltt - 2,
								eval_args[1],
								&es,
								predname,
								0);
							core[AA_N_INITREG + ltt + 0] = size;
							ltt += size;
						} else {
							core[AA_N_INITREG + addr] = tag_eval_value(eval_args[1], prg);
						}
					}
					if(es.errorflag) exit(1);
				}
			}
		}
	}

	for(i = 0; i < prg->nworldobj; i++) {
		initialparent[i] = -1;
		baseaddr = AA_N_INITREG + 1 + prg->nworldobj + n_glb_word + i * n_obj_word;
		core[baseaddr + OVAR_PARENT] = 0;
		core[baseaddr + OVAR_CHILD] = 0;
		core[baseaddr + OVAR_SIBLING] = 0;
	}

	eval_reinitialize(&es);
	eval_args[0] = eval_makevar(&es);
	eval_args[1] = eval_makevar(&es);
	more = eval_initial_multi(&es, find_builtin(prg, BI_HASPARENT), eval_args);
	while(more) {
		obj1 = eval_deref(eval_args[0], &es);
		if(obj1.tag == VAL_OBJ) {
			if(initialparent[obj1.value] < 0) {
				obj2 = eval_deref(eval_args[1], &es);
				if(obj2.tag != VAL_OBJ) {
					report(
						LVL_ERR,
						0,
						"Attempted to set the initial parent of #%s to a non-object.",
						prg->worldobjnames[obj1.value]->name);
					exit(1);
				}
				initialparent[obj1.value] = obj2.value;
				memset(seen, 0, prg->nworldobj);
				for(i = obj1.value; i >= 0; i = initialparent[i]) {
					if(seen[i]) {
						report(
							LVL_ERR,
							0,
							"Badly formed object tree! #%s is nested in itself.",
							prg->worldobjnames[i]->name);
						exit(1);
					}
					seen[i] = 1;
				}
				i = obj1.value;
				j = obj2.value;
				baseaddr = AA_N_INITREG + 1 + prg->nworldobj + n_glb_word;
				core[baseaddr + i * n_obj_word + OVAR_PARENT] = j + 1;
				addr = baseaddr + j * n_obj_word + OVAR_CHILD;
				while(core[addr]) {
					addr = baseaddr + (core[addr] - 1) * n_obj_word + OVAR_SIBLING;
				}
				core[addr] = i + 1;
			}
		} else {
			report(
				LVL_ERR,
				0,
				"Initial parent defined with a non-object as the first parameter.");
			exit(1);
		}
		more = eval_initial_next(&es);
	}

	free_evalstate(&es);

	return ltt;
}

static void setcorebyte(uint16_t *words, int index, uint8_t val) {
	if(index & 1) {
		words[index / 2] |= val;
	} else {
		words[index / 2] |= val << 8;
	}
}

static void chunk_init(FILE *f, struct program *prg, uint32_t *crc) {
	int core_sz = AA_N_INITREG + fixed_sz + longterm_sz + aux_sz + heap_sz;
	int ltt = fixed_sz;
	uint16_t core[core_sz];
	int trimmed;
	int pad;
	int i;

	report(LVL_DEBUG, 0, "Read-write memory: %d words (%d bytes)", core_sz, core_sz * 2);

	memset(core, 0, (AA_N_INITREG + ltt) * 2);
	memset(core + AA_N_INITREG + ltt, 0x3f, (core_sz - AA_N_INITREG - ltt) * 2);

	core[AA_N_INITREG] = 1 + prg->nworldobj;
	for(i = 0; i < prg->nworldobj; i++) {
		core[AA_N_INITREG + 1 + i] = 1 + prg->nworldobj + n_glb_word + i * n_obj_word;
	}

	for(i = 0; i < prg->nselect; i++) {
		if(initial_sel[i]) {
			setcorebyte(core + AA_N_INITREG + 1 + prg->nworldobj, first_sel_byte + i, initial_sel[i]);
		}
	}

	ltt = initial_values(prg, core, ltt);

	core[0] = prg->nworldobj;	// NOB
	core[1] = fixed_sz;		// LTB
	core[2] = ltt;			// LTT

	trimmed = core_sz;
	while(trimmed && core[trimmed - 1] == 0x3f3f) {
		trimmed--;
	}

	pad = chunkheader(f, "INIT", trimmed * 2);
	assert(!pad);
	for(i = 0; i < trimmed; i++) {
		putword_crc(core[i], f, crc);
	}
}

static aaoper_t encode_value(value_t v, struct program *prg) {
	if(v.tag == OPER_VAR) {
		assert(v.value < 64);
		return (aaoper_t) {AAO_VAR, v.value};
	} else if(v.tag == OPER_ARG) {
		return (aaoper_t) {AAO_REG, REG_A + v.value};
	} else if(v.tag == OPER_TEMP) {
		assert(v.value < AA_MAX_TEMP);
		return (aaoper_t) {AAO_REG, REG_X + v.value};
	} else if(v.tag == VAL_NIL) {
		return (aaoper_t) {AAO_REG, REG_NIL};
	} else {
		return (aaoper_t) {AAO_CONST, tag_eval_value(v, prg)};
	}
}

static aaoper_t encode_dest(value_t v, struct program *prg, int unify) {
	if(v.tag == OPER_VAR) {
		assert(v.value < 64);
		return (aaoper_t) {unify? AAO_VAR : AAO_STORE_VAR, v.value};
	} else if(v.tag == OPER_ARG) {
		return (aaoper_t) {unify? AAO_REG : AAO_STORE_REG, REG_A + v.value};
	} else if(v.tag == OPER_TEMP) {
		assert(v.value < AA_MAX_TEMP);
		return (aaoper_t) {unify? AAO_REG : AAO_STORE_REG, REG_X + v.value};
	} else if(v.tag == VAL_NIL && unify) {
		return (aaoper_t) {AAO_REG, REG_NIL};
	} else {
		printf("%d\n", v.tag);
		assert(0); exit(1);
	}
}

static void end_segment() {
	if(nsegment >= nalloc_segment) {
		nalloc_segment = nalloc_segment * 2 + 8;
		segment = realloc(segment, nalloc_segment * sizeof(struct segment));
	}

	segment[nsegment].ninstr = ninstr;
	segment[nsegment].instr = arena_alloc(&aa_arena, ninstr * sizeof(struct aainstr));
	memcpy(segment[nsegment].instr, aainstr, ninstr * sizeof(struct aainstr));
	segment[nsegment].visited = 0;
	nsegment++;
	memset(aainstr, 0, ninstr * sizeof(struct aainstr));
	ninstr = 0;
}

static void flatten_segments_sub(int s, int *segorder, int *sp, int *symbols) {
	int i, j, target;
	struct aainstr *ai;
	struct segment *seg = &segment[s];

	if(!seg->visited) {
		seg->visited = 1;

		for(i = 0; i < seg->ninstr; i++) {
			ai = &seg->instr[i];
			if(ai->op == AA_SET_CONT
			&& i == seg->ninstr - 2
			&& ai->oper[0].type == AAO_CODE
			&& ai->oper[0].value != AAFAIL
			&& symbols[ai->oper[0].value] >= 0
			&& (
				seg->instr[i + 1].op == AA_JMP_MULTI ||
				seg->instr[i + 1].op == AA_JMP_SIMPLE)
			&& seg->instr[i + 1].oper[0].type == AAO_CODE
			&& seg->instr[i + 1].oper[0].value != AAFAIL
			&& symbols[seg->instr[i + 1].oper[0].value] >= 0) {
				flatten_segments_sub(
					symbols[seg->instr[i + 1].oper[0].value],
					segorder,
					sp,
					symbols);
				flatten_segments_sub(
					symbols[ai->oper[0].value],
					segorder,
					sp,
					symbols);
				break;
			} else {
				for(j = 0; j < 4; j++) {
					if(ai->oper[j].type == AAO_CODE
					&& ai->oper[j].value != AAFAIL) {
						target = symbols[ai->oper[j].value];
						if(target >= 0) {
							flatten_segments_sub(
								target,
								segorder,
								sp,
								symbols);
						}
					}
				}
			}
		}

		segorder[--(*sp)] = s;
	}
}

static void flatten_segments(struct program *prg) {
	int segorder[nsegment];
	int sp;
	int i, j;
	struct segment *seg;
	int symbols[nextlabel];

	memset(symbols, 0xff, nextlabel * sizeof(int));
	for(i = 0; i < nsegment; i++) {
		for(j = 0; segment[i].instr[j].op == AA_LABEL; j++) {
			symbols[segment[i].instr[j].oper[0].value] = i;
		}
	}

	sp = nsegment;
	flatten_segments_sub(0, segorder, &sp, symbols);

	for(i = sp; i < nsegment - 1; i++) {
		seg = &segment[segorder[i]];
		if(seg->ninstr >= 2
		&& (seg->instr[seg->ninstr - 1].op == AA_FAIL || (
			seg->instr[seg->ninstr - 1].op == AA_JMP &&
			seg->instr[seg->ninstr - 1].oper[0].value == AAFAIL))
		&& (seg->instr[seg->ninstr - 2].op & 0x7f) >= 0x30
		&& (seg->instr[seg->ninstr - 2].op & 0x7f) <= 0x4f) {
			for(j = 0; j < 4; j++) {
				if(seg->instr[seg->ninstr - 2].oper[j].type == AAO_CODE
				&& seg->instr[seg->ninstr - 2].oper[j].value != AAFAIL
				&& symbols[seg->instr[seg->ninstr - 2].oper[j].value] == segorder[i + 1]) {
					seg->instr[seg->ninstr - 2].op ^= 0x30 ^ 0x40;
					seg->instr[seg->ninstr - 2].oper[j].value = AAFAIL;
					seg->instr[seg->ninstr - 1].op = AA_SKIP;
					seg->instr[seg->ninstr - 1].oper[0] = (aaoper_t) {AAO_NONE};
					break;
				}
			}
		}
	}

	nalloc_instr = 0;
	for(i = sp; i < nsegment; i++) {
		nalloc_instr += segment[segorder[i]].ninstr;
	}
	aainstr = realloc(aainstr, nalloc_instr * sizeof(struct aainstr));
	assert(ninstr == 0);
	for(i = sp; i < nsegment; i++) {
		seg = &segment[segorder[i]];
		memcpy(aainstr + ninstr, seg->instr, seg->ninstr * sizeof(struct aainstr));
		ninstr += seg->ninstr;
	}
}

static int generate_wordmap(struct wordmap *map) {
	int n, j, k, id, len;
	uint8_t data[2 * MAXWORDMAP + 1];
	uint16_t words[map->nmap * 2];

	n = map->nmap;
	if(map->map[n - 1].key == 0xffff) n--;
	assert(n > 0);
	for(j = 0; j < n; j++) {
		words[2 * j + 0] = map->map[j].key;
		if(map->map[j].count > MAXWORDMAP) {
			words[2 * j + 1] = 0;
		} else if(map->map[j].count == 1) {
			words[2 * j + 1] = 0xe001 + map->map[j].onumtable[0];
		} else {
			len = 0;
			for(k = 0; k < map->map[j].count; k++) {
				id = 1 + map->map[j].onumtable[k];
				if(id < 0xe0) {
					data[len++] = id;
				} else {
					assert(id <= 0x1fff);
					data[len++] = 0xe0 | (id >> 8);
					data[len++] = id & 0xff;
				}
			}
			data[len++] = 0;

			for(k = 0; k < ndatatable; k++) {
				if(datatable[k].length == len
				&& !memcmp(datatable[k].data, data, len)) {
					break;
				}
			}
			if(k == ndatatable) {
				datatable = realloc(datatable, ++ndatatable * sizeof(*datatable));
				datatable[k].address = datatable_org;
				datatable[k].length = len;
				datatable[k].data = malloc(len);
				memcpy(datatable[k].data, data, len);
				datatable_org += len;
			}
			words[2 * j + 1] = 1 + datatable[k].address;
		}
	}

	for(j = 0; j < nwordtable; j++) {
		if(wordtable[j].length == n * 2
		&& !memcmp(wordtable[j].words, words, n * 2 * sizeof(uint16_t))) {
			break;
		}
	}
	if(j == nwordtable) {
		wordtable = realloc(wordtable, ++nwordtable * sizeof(*wordtable));
		wordtable[j].length = n * 2;
		wordtable[j].words = malloc(n * 2 * sizeof(uint16_t));
		memcpy(wordtable[j].words, words, n * 2 * sizeof(uint16_t));
	}

	return j;
}

static int findstring(uint8_t *str) {
	int i, len;

	len = strlen((char *) str);
	for(i = 0; i < n_textstr; i++) {
		if(textstrings[i].length == len
		&& !memcmp(textstrings[i].chars, str, len)) {
			textstrings[i].occurrences++;
			return i;
		}
	}

	if(n_textstr >= nalloc_textstr) {
		nalloc_textstr = 2 * nalloc_textstr + 8;
		textstrings = realloc(textstrings, nalloc_textstr * sizeof(struct textstring));
	}

	textstrings[i].length = len;
	textstrings[i].occurrences = 1;
	textstrings[i].chars = (uint8_t *) arena_strdup(&aa_arena, (char *) str);
	textstrings[i].address = ~0;
	n_textstr++;

	return i;
}

struct index_slot {
	uint16_t	key;
	uint32_t	label;
};

static int cmp_index_slot(const void *a, const void *b) {
	const struct index_slot *aa = a;
	const struct index_slot *bb = b;

	return aa->key - bb->key;
}

static void binary_search(struct index_slot *table, int n, uint32_t endlab) {
	uint32_t ll;
	int i, j, pos;
	struct aainstr *ai;
	uint8_t handled[TWEAK_BINSEARCH];

	if(n >= TWEAK_BINSEARCH) {
		pos = n / 2;
		ll = nextlabel++;
		ai = add_instr(AA_CHECK_GT_EQ);
		if(table[pos].key < 0x100) {
			ai->op |= 0x80;
			ai->oper[0] = (aaoper_t) {AAO_VBYTE, table[pos].key};
		} else {
			ai->oper[0] = (aaoper_t) {AAO_WORD, table[pos].key};
		}
		ai->oper[1] = (aaoper_t) {AAO_CODE, ll};
		ai->oper[2] = (aaoper_t) {AAO_CODE, table[pos].label};
		binary_search(table, pos, endlab);
		add_label(ll);
		binary_search(table + pos + 1, n - pos - 1, endlab);
	} else {
		memset(handled, 0, TWEAK_BINSEARCH);
		for(i = 0; i < n; i++) {
			if(!handled[i]) {
				for(j = i + 1; j < n; j++) {
					if(table[i].label == table[j].label
					&& (table[i].key < 0x100) == (table[j].key < 0x100)) {
						break;
					}
				}
				if(j < n) {
					if(table[i].key < 0x100) {
						ai = add_instr(AA_CHECK_EQ_2B);
						ai->oper[0] = (aaoper_t) {AAO_VBYTE, table[i].key};
						ai->oper[1] = (aaoper_t) {AAO_VBYTE, table[j].key};
					} else {
						ai = add_instr(AA_CHECK_EQ_2A);
						ai->oper[0] = (aaoper_t) {AAO_WORD, table[i].key};
						ai->oper[1] = (aaoper_t) {AAO_WORD, table[j].key};
					}
					ai->oper[2] = (aaoper_t) {AAO_CODE, table[i].label};
					handled[j] = 1;
				} else {
					ai = add_instr(AA_CHECK_EQ);
					if(table[i].key < 0x100) {
						ai->op |= 0x80;
						ai->oper[0] = (aaoper_t) {AAO_VBYTE, table[i].key};
					} else {
						ai->oper[0] = (aaoper_t) {AAO_WORD, table[i].key};
					}
					ai->oper[1] = (aaoper_t) {AAO_CODE, table[i].label};
				}
			}
		}
		ai = add_instr(AA_JMP);
		ai->oper[0] = (aaoper_t) {AAO_CODE, endlab};
	}
}

static void compile_index(
	struct program *prg,
	struct cinstr *instr,
	int ninstr,
	uint32_t labelbase,
	uint32_t endlab)
{
	struct index_slot table[ninstr];
	int i;

	memset(table, 0, ninstr * sizeof(struct index_slot));

	for(i = 0; i < ninstr; i++) {
		table[i].key = tag_eval_value(instr[i].oper[0], prg);
		if(instr[i].oper[1].tag == OPER_FAIL) {
			table[i].label = AAFAIL;
		} else {
			assert(instr[i].oper[1].tag == OPER_RLAB);
			table[i].label = labelbase + instr[i].oper[1].value;
		}
	}

	qsort(table, ninstr, sizeof(struct index_slot), cmp_index_slot);

	binary_search(table, ninstr, endlab);
}

static void add_to_buf(uint8_t **buf, int *nalloc, int *pos, uint8_t ch) {
	if(*pos >= *nalloc) {
		*nalloc = (*pos) * 2 + 32;
		*buf = realloc(*buf, *nalloc);
	}
	(*buf)[(*pos)++] = ch;
}

static int decode_word_output(struct program *prg, uint8_t **bufptr, struct cinstr *instr, int *p_space, int include_ints) {
	int post_space = 0;
	int ninstr = 0;
	uint8_t last = 0;
	uint8_t *buf = 0;
	int i, j, len;
	int nalloc = 0, pos = 0;
	struct cinstr *ci;
	struct word *w;
	uint8_t numbuf[8];
	uint16_t uchar[2];

	for(;;) {
		ci = &instr[ninstr];
		if(ci->op == I_PRINT_WORDS) {
			for(i = 0; i < 3; i++) {
				if(ci->oper[i].tag == OPER_WORD) {
					w = prg->allwords[ci->oper[i].value];
					if(post_space == 2
					|| (post_space == 1 && !strchr(NO_SPACE_BEFORE, w->name[0]))) {
						add_to_buf(&buf, &nalloc, &pos, ' ');
					}
					len = strlen(w->name);
					j = 0;
					while(j < len) {
						j += utf8_to_unicode_n(uchar, 2, (uint8_t *) w->name + j, len - j);
						if(uchar[0]) {
							last = resolve_aachar(uchar[0]);
							add_to_buf(&buf, &nalloc, &pos, last);
						}
					}
					post_space = !strchr(NO_SPACE_AFTER, last);
				}
			}
			ninstr++;
		} else if(ci->op == I_PRINT_VAL && include_ints && ci->oper[0].tag == VAL_NUM) {
			snprintf((char *) numbuf, sizeof(numbuf), "%d", ci->oper[0].value);
			if(post_space) {
				add_to_buf(&buf, &nalloc, &pos, ' ');
			}
			for(i = 0; numbuf[i]; i++) {
				last = numbuf[i];
				add_to_buf(&buf, &nalloc, &pos, last);
			}
			post_space = 1;
			ninstr++;
		} else if(ci->op == I_BUILTIN && prg->predicates[ci->oper[2].value]->builtin == BI_NOSPACE) {
			if(post_space == 1) post_space = 0;
			ninstr++;
		} else if(ci->op == I_BUILTIN && prg->predicates[ci->oper[2].value]->builtin == BI_SPACE) {
			post_space = 2;
			ninstr++;
		} else {
			break;
		}
	}
	add_to_buf(&buf, &nalloc, &pos, 0);

	*bufptr = buf;
	if(p_space) *p_space = post_space;
	return ninstr;
}

static int generate_output(struct program *prg, struct cinstr *instr, int dry_run, int include_ints) {
	uint8_t *aastr;
	int pre_space;
	int post_space;
	uint8_t variant;
	struct aainstr *ai;
	int ninstr;

	pre_space = !strchr(NO_SPACE_BEFORE, prg->allwords[instr[0].oper[0].value]->name[0]);
	ninstr = decode_word_output(prg, &aastr, instr, &post_space, include_ints);

	if(!dry_run) {
		if(!pre_space && !post_space) {
			variant = AA_PRINT_N_STR_N;
		} else if(!pre_space && post_space) {
			variant = AA_PRINT_N_STR_A;
		} else if(pre_space && !post_space) {
			variant = AA_PRINT_A_STR_N;
		} else {
			variant = AA_PRINT_A_STR_A;
		}
		ai = add_instr(variant);
		ai->oper[0] = (aaoper_t) {AAO_STRING, findstring(aastr)};
		if(post_space == 2) {
			ai = add_instr(AA_SPACE);
		}
	}

	free(aastr);
	return ninstr;
}

static void visit_reachable_routines(int rnum, struct predicate *pred, uint8_t *reachable) {
	int i, j;
	struct comp_routine *cr = &pred->routines[rnum];

	if(reachable[rnum]) return;

	reachable[rnum] = 1;
	for(i = 0; i < cr->ninstr; i++) {
		for(j = 0; j < 3; j++) {
			if(cr->instr[i].oper[j].tag == OPER_RLAB) {
				visit_reachable_routines(cr->instr[i].oper[j].value, pred, reachable);
			}
		}
		if(cr->instr[i].implicit != 0xffff) {
			visit_reachable_routines(cr->instr[i].implicit, pred, reachable);
		}
	}
}

static int only_lowercase(char *str) {
	while(*str) {
		if(*str < 'a' || *str > 'z') {
			return 0;
		}
		str++;
	}

	return 1;
}

static void compile_routines(struct program *prg, struct predicate *pred, int first_r, uint32_t labelbase) {
	int i, j, k, m, sub_r_id;
	int sel, n, id, flag;
	struct aainstr *ai;
	struct comp_routine *cr;
	struct cinstr *ci;
	uint32_t ll, ll2, ll3, ll4;
	struct word *w;
	struct wordmap *map;
	aaoper_t aao;
	uint8_t reachable[pred->nroutine];

	memset(reachable, 0, sizeof(reachable));
	visit_reachable_routines(first_r, pred, reachable);

	for(sub_r_id = 0; sub_r_id < pred->nroutine; sub_r_id++) {
		if(!reachable[sub_r_id]) {
			continue;
		}
		assert(pred->routines[sub_r_id].reftrack != 0xffff);
		if(sub_r_id == first_r) {
			add_label(pred->predname->pred_id);
		}
		cr = &pred->routines[sub_r_id];
		add_label(labelbase + sub_r_id);
		for(i = 0; i < cr->ninstr; i++) {
			ci = &cr->instr[i];
			switch(ci->op) {
			case I_ALLOCATE:
				assert(ci->oper[0].tag == OPER_NUM);
				ai = add_instr(AA_PUSH_ENV);
				if(ci->oper[0].value) {
					ai->oper[0] = (aaoper_t) {AAO_BYTE, ci->oper[0].value};
				} else {
					ai->op |= 0x80;
					ai->oper[0] = (aaoper_t) {AAO_ZERO};
				}
				break;
			case I_ASSIGN:
				if(ci->oper[1].tag == VAL_OBJ && ci->oper[1].value < 0xff) {
					ai = add_instr(AA_ASSIGN | 0x80);
					ai->oper[0] = (aaoper_t) {AAO_VBYTE, 1 + ci->oper[1].value};
				} else {
					ai = add_instr(AA_ASSIGN);
					ai->oper[0] = encode_value(ci->oper[1], prg);
				}
				ai->oper[1] = encode_dest(ci->oper[0], prg, 0);
				break;
			case I_BEGIN_AREA:
			case I_BEGIN_AREA_OVERRIDE:
				assert(ci->oper[0].tag == OPER_BOX);
				if(ci->subop == AREA_TOP) {
					ai = add_instr(AA_ENTER_STATUS_0);
					ai->oper[0] = (aaoper_t) {AAO_ZERO};
					ai->oper[1] = (aaoper_t) {AAO_INDEX, ci->oper[0].value};
				} else {
					ai = add_instr(AA_ENTER_STATUS);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, 1};
					ai->oper[1] = (aaoper_t) {AAO_INDEX, ci->oper[0].value};
				}
				break;
			case I_BEGIN_BOX:
				assert(ci->oper[0].tag == OPER_BOX);
				if(ci->subop == BOX_SPAN) {
					ai = add_instr(AA_ENTER_SPAN);
					ai->oper[0] = (aaoper_t) {AAO_INDEX, ci->oper[0].value};
				} else {
					ai = add_instr(AA_ENTER_DIV);
					ai->oper[0] = (aaoper_t) {AAO_INDEX, ci->oper[0].value};
				}
				break;
			case I_BEGIN_LINK:
				ai = add_instr(AA_ENTER_LINK);
				ai->oper[0] = encode_value(ci->oper[0], prg);
				break;
			case I_BEGIN_LINK_RES:
				ai = add_instr(AA_ENTER_LINK_RES);
				ai->oper[0] = encode_value(ci->oper[0], prg);
				break;
			case I_BEGIN_SELF_LINK:
				ai = add_instr(AA_ENTER_SELF_LINK);
				break;
			case I_BUILTIN:
				assert(ci->oper[2].tag == OPER_PRED);
				id = prg->predicates[ci->oper[2].value]->builtin;
				switch(id) {
				case BI_BOLD:
					ai = add_instr(AA_SET_STYLE);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, AASTYLE_BOLD};
					break;
				case BI_CLEAR:
					ai = add_instr(AA_EXT0);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, AAEXT0_CLEAR};
					break;
				case BI_CLEAR_ALL:
					ai = add_instr(AA_EXT0);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, AAEXT0_CLEAR_ALL};
					break;
				case BI_CLEAR_LINKS:
					ai = add_instr(AA_EXT0);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, AAEXT0_CLEAR_LINKS};
					break;
				case BI_CLEAR_DIV:
					ai = add_instr(AA_EXT0);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, AAEXT0_CLEAR_DIV};
					break;
				case BI_CLEAR_OLD:
					ai = add_instr(AA_EXT0);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, AAEXT0_CLEAR_OLD};
					break;
				case BI_COMPILERVERSION:
					ai = add_instr(AA_PRINT_A_STR_A);
					ai->oper[0] = (aaoper_t) {AAO_STRING, findstring((uint8_t *) COMPILERVERSION)};
					break;
				case BI_FIXED:
					ai = add_instr(AA_SET_STYLE);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, AASTYLE_FIXED};
					break;
				case BI_ITALIC:
					ai = add_instr(AA_SET_STYLE);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, AASTYLE_ITALIC};
					break;
				case BI_LINE:
					ai = add_instr(AA_LINE);
					break;
				case BI_MEMSTATS:
					ai = add_instr(AA_LINE);
					ai = add_instr(AA_PRINT_A_STR_A);
					ai->oper[0] = (aaoper_t) {AAO_STRING, findstring((uint8_t *) "Peak dynamic memory usage:")};
					ai = add_instr(AA_VM_INFO);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, 0};
					ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
					ai = add_instr(AA_PRINT_VAL);
					ai->oper[0] = (aaoper_t) {AAO_REG, REG_TMP};
					ai = add_instr(AA_PRINT_A_STR_A);
					ai->oper[0] = (aaoper_t) {AAO_STRING, findstring((uint8_t *) "heap words,")};
					ai = add_instr(AA_VM_INFO);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, 1};
					ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
					ai = add_instr(AA_PRINT_VAL);
					ai->oper[0] = (aaoper_t) {AAO_REG, REG_TMP};
					ai = add_instr(AA_PRINT_A_STR_A);
					ai->oper[0] = (aaoper_t) {AAO_STRING, findstring((uint8_t *) "aux words, and")};
					ai = add_instr(AA_VM_INFO);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, 2};
					ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
					ai = add_instr(AA_PRINT_VAL);
					ai->oper[0] = (aaoper_t) {AAO_REG, REG_TMP};
					ai = add_instr(AA_PRINT_A_STR_A);
					ai->oper[0] = (aaoper_t) {AAO_STRING, findstring((uint8_t *) "long-term words.")};
					ai = add_instr(AA_LINE);
					break;
				case BI_NOSPACE:
					ai = add_instr(AA_NOSPACE);
					break;
				case BI_PAR:
					ai = add_instr(AA_PAR);
					break;
				case BI_PROGRESS_BAR:
					ai = add_instr(AA_PROGRESS);
					ai->oper[0] = encode_value(ci->oper[0], prg);
					ai->oper[1] = encode_value(ci->oper[1], prg);
					break;
				case BI_REVERSE:
					ai = add_instr(AA_SET_STYLE);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, AASTYLE_REVERSE};
					break;
				case BI_ROMAN:
					ai = add_instr(AA_RESET_STYLE);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, AASTYLE_REVERSE|AASTYLE_FIXED|AASTYLE_ITALIC|AASTYLE_BOLD};
					break;
				case BI_SCRIPT_OFF:
					ai = add_instr(AA_EXT0);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, AAEXT0_SCRIPT_OFF};
					break;
				case BI_SCRIPT_ON:
					ai = add_instr(AA_EXT0);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, AAEXT0_SCRIPT_ON};
					break;
				case BI_SERIALNUMBER:
					ai = add_instr(AA_EXT0);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, AAEXT0_PRINT_SERIAL};
					break;
				case BI_SPACE:
					ai = add_instr(AA_SPACE);
					break;
				case BI_SPACE_N:
					ai = add_instr(AA_SPACE_N);
					ai->oper[0] = encode_value(ci->oper[0], prg);
					break;
				case BI_TRACE_OFF:
					ai = add_instr(AA_EXT0);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, AAEXT0_TRACE_OFF};
					break;
				case BI_TRACE_ON:
					ai = add_instr(AA_EXT0);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, AAEXT0_TRACE_ON};
					break;
				case BI_UNSTYLE:
					ai = add_instr(AA_EXT0);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, AAEXT0_UNSTYLE};
					break;
				case BI_UPPER:
					ai = add_instr(AA_EXT0);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, AAEXT0_UPPERCASE};
					break;
				default:
					printf("unimplemented builtin %d %s\n", id, prg->predicates[ci->oper[2].value]->printed_name);
					assert(0); exit(1);
				}
				break;
			case I_CHECK_INDEX:
				n = 1;
				while(cr->instr[i + n].op == I_CHECK_INDEX) n++;
				if(cr->instr[i + n].op == I_JUMP && cr->instr[i + n].oper[0].tag == OPER_FAIL) {
					ll = AAFAIL;
				} else {
					ll = nextlabel++;
				}
				compile_index(prg, &cr->instr[i], n, labelbase, ll);
				if(ll == AAFAIL) {
					i++;
				} else {
					add_label(ll);
				}
				i += n - 1;
				break;
			case I_CHECK_WORDMAP:
				assert(ci->oper[0].tag == OPER_NUM);
				assert(ci->oper[2].tag == OPER_PRED);
				assert(ci->oper[0].value < prg->predicates[ci->oper[2].value]->pred->nwordmap);
				map = &prg->predicates[ci->oper[2].value]->pred->wordmaps[ci->oper[0].value];
				ai = add_instr(AA_CHECK_WORDMAP);
				ai->oper[0] = (aaoper_t) {AAO_INDEX, generate_wordmap(map)};
				if(ci->oper[1].tag == OPER_FAIL) {
					ai->oper[1] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					assert(ci->oper[1].tag == OPER_RLAB);
					ai->oper[1] = (aaoper_t) {AAO_CODE, labelbase + ci->oper[1].value};
				}
				break;
			case I_CLRALL_OFLAG:
				assert(ci->oper[0].tag == OPER_OFLAG);
				assert(prg->objflagpred[ci->oper[0].value]->pred->flags & PREDF_DYN_LINKAGE);
				ll = nextlabel++;
				ll2 = nextlabel++;
				ai = add_instr(AA_LOAD_WORD | 0x80);
				ai->oper[0] = (aaoper_t) {AAO_ZERO};
				ai->oper[1] = (aaoper_t) {AAO_INDEX, first_gvar + prg->nglobalvar + dynlink_id[ci->oper[0].value]};
				ai->oper[2] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
				ai = add_instr(AA_IF_RAW_EQ | 0x80);
				ai->oper[0] = (aaoper_t) {AAO_ZERO};
				ai->oper[1] = (aaoper_t) {AAO_REG, REG_TMP};
				ai->oper[2] = (aaoper_t) {AAO_CODE, ll2};
				add_label(ll);
				ai = add_instr(AA_RESET_FLAG);
				ai->oper[0] = (aaoper_t) {AAO_REG, REG_TMP};
				ai->oper[1] = (aaoper_t) {AAO_INDEX, 48 + ci->oper[0].value};
				ai = add_instr(AA_LOAD_WORD);
				ai->oper[0] = (aaoper_t) {AAO_REG, REG_TMP};
				ai->oper[1] = (aaoper_t) {AAO_INDEX, first_ovar + (prg->nobjvar - 1) + dynlink_id[ci->oper[0].value]};
				ai->oper[2] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
				ai = add_instr(AA_IFN_RAW_EQ | 0x80);
				ai->oper[0] = (aaoper_t) {AAO_ZERO};
				ai->oper[1] = (aaoper_t) {AAO_REG, REG_TMP};
				ai->oper[2] = (aaoper_t) {AAO_CODE, ll};
				ai = add_instr(AA_STORE_WORD | 0x80);
				ai->oper[0] = (aaoper_t) {AAO_ZERO};
				ai->oper[1] = (aaoper_t) {AAO_INDEX, first_gvar + prg->nglobalvar + dynlink_id[ci->oper[0].value]};
				ai->oper[2] = (aaoper_t) {AAO_CONST, 0};
				add_label(ll2);
				break;
			case I_CLRALL_OVAR:
				assert(ci->oper[0].tag == OPER_OVAR);
				assert(ci->oper[0].value != DYN_HASPARENT);
				if(prg->nworldobj) {
					ll = nextlabel++;
					ai = add_instr(AA_ASSIGN);
					ai->oper[0] = (aaoper_t) {AAO_CONST, prg->nworldobj};
					ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
					add_label(ll);
					ai = add_instr(AA_STORE_WORD);
					ai->oper[0] = (aaoper_t) {AAO_REG, REG_TMP};
					ai->oper[1] = (aaoper_t) {AAO_INDEX, first_ovar + (ci->oper[0].value - 1)};
					ai->oper[2] = (aaoper_t) {AAO_CONST, 0};
					ai = add_instr(AA_DEC_RAW);
					ai->oper[0] = (aaoper_t) {AAO_REG, REG_TMP};
					ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
					ai = add_instr(AA_IFN_RAW_EQ | 0x80);
					ai->oper[0] = (aaoper_t) {AAO_ZERO};
					ai->oper[1] = (aaoper_t) {AAO_REG, REG_TMP};
					ai->oper[2] = (aaoper_t) {AAO_CODE, ll};
				}
				break;
			case I_COLLECT_BEGIN:
				ai = add_instr(AA_AUX_PUSH_RAW_0);
				ai->oper[0] = (aaoper_t) {AAO_ZERO};
				if(ci->subop) {
					ai = add_instr(AA_AUX_PUSH_VAL);
					ai->oper[0] = (aaoper_t) {AAO_CONST, tag_eval_value((value_t) {VAL_NUM, 0}, prg)};
				}
				break;
			case I_COLLECT_CHECK:
				ai = add_instr(AA_AUX_POP_LIST_CHK);
				ai->oper[0] = encode_value(ci->oper[0], prg);
				break;
			case I_COLLECT_END_R:
				if(ci->subop) {
					ai = add_instr(AA_AUX_POP_LIST);
					ai->oper[0] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
					ai = add_instr(AA_MAKE_PAIR_D);
					ai->oper[0] = encode_dest(ci->oper[0], prg, 0);
					ai->oper[1] = (aaoper_t) {AAO_REG, REG_NIL};
					ai->oper[2] = (aaoper_t) {AAO_REG, REG_TMP};
				} else {
					ai = add_instr(AA_AUX_POP_LIST);
					ai->oper[0] = encode_dest(ci->oper[0], prg, 0);
				}
				break;
			case I_COLLECT_END_V:
				if(ci->subop) {
					ai = add_instr(AA_AUX_POP_LIST);
					ai->oper[0] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
					ai = add_instr(AA_MAKE_PAIR_D);
					ai->oper[0] = encode_dest(ci->oper[0], prg, 1);
					ai->oper[1] = (aaoper_t) {AAO_REG, REG_NIL};
					ai->oper[2] = (aaoper_t) {AAO_REG, REG_TMP};
				} else {
					ai = add_instr(AA_AUX_POP_LIST);
					if(ci->oper[0].tag == OPER_TEMP
					|| ci->oper[0].tag == OPER_VAR
					|| ci->oper[0].tag == OPER_ARG
					|| ci->oper[0].tag == VAL_NIL) {
						ai->oper[0] = encode_dest(ci->oper[0], prg, 1);
					} else {
						ai->oper[0] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
						ai = add_instr(AA_ASSIGN);
						ai->oper[0] = encode_value(ci->oper[0], prg);
						ai->oper[1] = (aaoper_t) {AAO_REG, REG_TMP};
					}
				}
				break;
			case I_COLLECT_MATCH_ALL:
				ai = add_instr(AA_AUX_POP_LIST_MATCH);
				ai->oper[0] = encode_value(ci->oper[0], prg);
				break;
			case I_COLLECT_PUSH:
				if(ci->subop) {
					ai = add_instr(AA_AUX_POP_LIST);
					ai->oper[0] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
					ai = add_instr(AA_AUX_PUSH_RAW_0);
					ai->oper[0] = (aaoper_t) {AAO_ZERO};
					ai = add_instr(AA_MAKE_PAIR_D);
					ai->oper[0] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
					ai->oper[1] = (aaoper_t) {AAO_REG, REG_NIL};
					ai->oper[2] = (aaoper_t) {AAO_REG, REG_TMP};
					if(ci->oper[0].tag == VAL_NUM && ci->oper[0].value == 1) {
						ai = add_instr(AA_INC_NUM);
						ai->oper[0] = (aaoper_t) {AAO_REG, REG_TMP};
						ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
					} else {
						ai = add_instr(AA_ADD_NUM);
						ai->oper[0] = (aaoper_t) {AAO_REG, REG_TMP};
						ai->oper[1] = encode_value(ci->oper[0], prg);
						ai->oper[2] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
					}
					ai = add_instr(AA_AUX_PUSH_VAL);
					ai->oper[0] = (aaoper_t) {AAO_REG, REG_TMP};
				} else if(ci->oper[0].tag == VAL_OBJ) {
					ai = add_instr(AA_AUX_PUSH_RAW);
					if(ci->oper[0].value < 0xff) {
						ai->op |= 0x80;
						ai->oper[0] = (aaoper_t) {AAO_VBYTE, 1 + ci->oper[0].value};
					} else {
						ai->oper[0] = (aaoper_t) {AAO_WORD, 1 + ci->oper[0].value};
					}
				} else if(ci->oper[0].tag == VAL_DICT) {
					ai = add_instr(AA_AUX_PUSH_RAW);
					ai->oper[0] = (aaoper_t) {AAO_WORD, tag_eval_value(ci->oper[0], prg)};
				} else {
					ai = add_instr(AA_AUX_PUSH_VAL);
					ai->oper[0] = encode_value(ci->oper[0], prg);
				}
				break;
			case I_COMPUTE_V:
			case I_COMPUTE_R:
				if(ci->subop == BI_DIV_WIDTH){ // This isn't supported by the Å-machine currently, so just fail without doing any further processing
					ai = add_instr(AA_FAIL);
				}
				if(ci->op == I_COMPUTE_V
				&& ci->oper[2].tag != OPER_VAR
				&& ci->oper[2].tag != OPER_ARG
				&& ci->oper[2].tag != OPER_TEMP
				&& ci->oper[2].tag != VAL_NIL) {
					ai = add_instr(AA_ASSIGN);
					ai->oper[0] = encode_value(ci->oper[2], prg);
					ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
					aao = (aaoper_t) {AAO_REG, REG_TMP};
				} else {
					aao = encode_dest(ci->oper[2], prg, ci->op == I_COMPUTE_V);
				}
				if(ci->subop == BI_PLUS && ci->oper[1].tag == VAL_NUM && ci->oper[1].value == 1) {
					ai = add_instr(AA_INC_NUM);
					ai->oper[0] = encode_value(ci->oper[0], prg);
					ai->oper[1] = aao;
				} else if(ci->subop == BI_PLUS && ci->oper[0].tag == VAL_NUM && ci->oper[0].value == 1) {
					ai = add_instr(AA_INC_NUM);
					ai->oper[0] = encode_value(ci->oper[1], prg);
					ai->oper[1] = aao;
				} else if(ci->subop == BI_MINUS && ci->oper[1].tag == VAL_NUM && ci->oper[1].value == 1) {
					ai = add_instr(AA_DEC_NUM);
					ai->oper[0] = encode_value(ci->oper[0], prg);
					ai->oper[1] = aao;
				} else {
					switch(ci->subop) {
					case BI_PLUS:
						ai = add_instr(AA_ADD_NUM);
						break;
					case BI_MINUS:
						ai = add_instr(AA_SUB_NUM);
						break;
					case BI_TIMES:
						ai = add_instr(AA_MUL_NUM);
						break;
					case BI_DIVIDED:
						ai = add_instr(AA_DIV_NUM);
						break;
					case BI_MODULO:
						ai = add_instr(AA_MOD_NUM);
						break;
					case BI_RANDOM:
						ai = add_instr(AA_RAND_NUM);
						break;
					default:
						assert(0); exit(1);
					}
					ai->oper[0] = encode_value(ci->oper[0], prg);
					ai->oper[1] = encode_value(ci->oper[1], prg);
					ai->oper[2] = aao;
				}
				break;
			case I_CUT_CHOICE:
				ai = add_instr(AA_CUT_CHOICE);
				break;
			case I_DEALLOCATE:
				if(cr->instr[i + 1].op == I_PROCEED) {
					ai = add_instr(AA_POP_ENV_PROCEED);
					i++;
				} else {
					ai = add_instr(AA_POP_ENV);
				}
				break;
			case I_EMBED_RES:
				ai = add_instr(AA_EMBED_RES);
				ai->oper[0] = encode_value(ci->oper[0], prg);
				break;
			case I_END_AREA:
				assert(ci->oper[0].tag == OPER_BOX);
				ai = add_instr(AA_LEAVE_STATUS);
				break;
			case I_END_BOX:
				assert(ci->oper[0].tag == OPER_BOX);
				if(ci->subop == BOX_SPAN) {
					ai = add_instr(AA_LEAVE_SPAN);
				} else {
					ai = add_instr(AA_LEAVE_DIV);
				}
				break;
			case I_END_LINK:
				ai = add_instr(AA_LEAVE_LINK);
				break;
			case I_END_LINK_RES:
				ai = add_instr(AA_LEAVE_LINK_RES);
				break;
			case I_END_SELF_LINK:
				ai = add_instr(AA_LEAVE_SELF_LINK);
				break;
			case I_FIRST_CHILD:
				ai = add_instr(AA_LOAD_VAL);
				ai->oper[0] = encode_value(ci->oper[0], prg);
				ai->oper[1] = (aaoper_t) {AAO_INDEX, OVAR_CHILD};
				ai->oper[2] = encode_dest(ci->oper[1], prg, 0);
				break;
			case I_FIRST_OFLAG:
				assert(ci->oper[0].tag == OPER_OFLAG);
				assert(prg->objflagpred[ci->oper[0].value]->pred->flags & PREDF_DYN_LINKAGE);
				ai = add_instr(AA_LOAD_VAL | 0x80);
				ai->oper[0] = (aaoper_t) {AAO_ZERO};
				ai->oper[1] = (aaoper_t) {AAO_INDEX, first_gvar + prg->nglobalvar + dynlink_id[ci->oper[0].value]};
				ai->oper[2] = encode_dest(ci->oper[1], prg, 0);
				break;
			case I_FOR_WORDS:
				ai = add_instr(AA_EXT0);
				if(ci->subop) {
					ai->oper[0] = (aaoper_t) {AAO_BYTE, AAEXT0_INC_CWL};
				} else {
					ai->oper[0] = (aaoper_t) {AAO_BYTE, AAEXT0_DEC_CWL};
				}
				break;
			case I_GET_OVAR_R:
			case I_GET_OVAR_V:
				assert(ci->oper[0].tag == OPER_OVAR);
				ai = add_instr(AA_LOAD_VAL);
				ai->oper[0] = encode_value(ci->oper[1], prg);
				if(ci->oper[0].value == DYN_HASPARENT) {
					ai->oper[1] = (aaoper_t) {AAO_INDEX, OVAR_PARENT};
				} else {
					ai->oper[1] = (aaoper_t) {AAO_INDEX, first_ovar + (ci->oper[0].value - 1)};
				}
				ai->oper[2] = encode_dest(ci->oper[2], prg, ci->op == I_GET_OVAR_V);
				break;
			case I_GET_GVAR_R:
			case I_GET_GVAR_V:
				assert(ci->oper[0].tag == OPER_GVAR);
				ai = add_instr(AA_LOAD_VAL | 0x80);
				ai->oper[0] = (aaoper_t) {AAO_ZERO};
				ai->oper[1] = (aaoper_t) {AAO_INDEX, first_gvar + ci->oper[0].value};
				ai->oper[2] = encode_dest(ci->oper[1], prg, ci->op == I_GET_GVAR_V);
				break;
			case I_GET_INPUT:
				ai = add_instr(AA_GET_INPUT);
				ai->oper[0] = (aaoper_t) {AAO_REG, REG_A + 0};
				ai = add_instr(AA_PROCEED);
				break;
			case I_GET_KEY:
				ai = add_instr(AA_GET_KEY);
				ai->oper[0] = (aaoper_t) {AAO_REG, REG_A + 0};
				ai = add_instr(AA_PROCEED);
				break;
			case I_GET_PAIR_RR:
			case I_MAKE_PAIR_RR:
				ai = add_instr(AA_MAKE_PAIR_D);
				ai->oper[0] = encode_dest(ci->oper[1], prg, 0);
				ai->oper[1] = encode_dest(ci->oper[2], prg, 0);
				ai->oper[2] = encode_dest(ci->oper[0], prg, ci->op == I_GET_PAIR_RR);
				break;
			case I_GET_PAIR_RV:
			case I_MAKE_PAIR_RV:
				ai = add_instr(AA_MAKE_PAIR_D);
				ai->oper[0] = encode_dest(ci->oper[1], prg, 0);
				ai->oper[1] = encode_dest(ci->oper[2], prg, 1);
				ai->oper[2] = encode_dest(ci->oper[0], prg, ci->op == I_GET_PAIR_RV);
				break;
			case I_GET_PAIR_VR:
			case I_MAKE_PAIR_VR:
				if(ci->oper[1].tag == OPER_VAR
				|| ci->oper[1].tag == OPER_TEMP
				|| ci->oper[1].tag == OPER_ARG
				|| ci->oper[1].tag == VAL_NIL) {
					ai = add_instr(AA_MAKE_PAIR_D);
					ai->oper[0] = encode_dest(ci->oper[1], prg, 1);
				} else {
					ai = add_instr(AA_MAKE_PAIR_WB);
					if(ci->oper[1].tag == VAL_OBJ && ci->oper[1].value < 0xff) {
						ai->op |= 0x80;
						ai->oper[0] = (aaoper_t) {AAO_VBYTE, 1 + ci->oper[1].value};
					} else {
						ai->oper[0] = (aaoper_t) {AAO_WORD, tag_eval_value(ci->oper[1], prg)};
					}
				}
				ai->oper[1] = encode_dest(ci->oper[2], prg, 0);
				ai->oper[2] = encode_dest(ci->oper[0], prg, ci->op == I_GET_PAIR_VR);
				break;
			case I_GET_PAIR_VV:
			case I_MAKE_PAIR_VV:
				if(ci->oper[1].tag == OPER_VAR
				|| ci->oper[1].tag == OPER_TEMP
				|| ci->oper[1].tag == OPER_ARG
				|| ci->oper[1].tag == VAL_NIL) {
					ai = add_instr(AA_MAKE_PAIR_D);
					ai->oper[0] = encode_dest(ci->oper[1], prg, 1);
				} else {
					ai = add_instr(AA_MAKE_PAIR_WB);
					if(ci->oper[1].tag == VAL_OBJ && ci->oper[1].value < 0xff) {
						ai->op |= 0x80;
						ai->oper[0] = (aaoper_t) {AAO_VBYTE, 1 + ci->oper[1].value};
					} else {
						ai->oper[0] = (aaoper_t) {AAO_WORD, tag_eval_value(ci->oper[1], prg)};
					}
				}
				ai->oper[1] = encode_dest(ci->oper[2], prg, 1);
				ai->oper[2] = encode_dest(ci->oper[0], prg, ci->op == I_GET_PAIR_VV);
				break;
			case I_IF_BOUND:
				ai = add_instr(AA_IF_BOUND);
				if(ci->subop) ai->op ^= AA_NEG_FLIP;
				ai->oper[0] = encode_value(ci->oper[0], prg);
				if(ci->implicit == 0xffff) {
					ai->oper[1] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					ai->oper[1] = (aaoper_t) {AAO_CODE, labelbase + ci->implicit};
				}
				break;
			case I_IF_CAN_EMBED:
				ai = add_instr(AA_CAN_EMBED_RES);
				ai->oper[0] = encode_value(ci->oper[0], prg);
				ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
				ai = add_instr(AA_IF_RAW_EQ | 0x80);
				if(!ci->subop) ai->op ^= AA_NEG_FLIP;
				ai->oper[0] = (aaoper_t) {AAO_ZERO};
				ai->oper[1] = (aaoper_t) {AAO_REG, REG_TMP};
				if(ci->implicit == 0xffff) {
					ai->oper[2] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					ai->oper[2] = (aaoper_t) {AAO_CODE, labelbase + ci->implicit};
				}
				break;
			case I_IF_GFLAG:
				assert(ci->oper[0].tag == OPER_GFLAG);
				ai = add_instr(AA_IF_FLAG | 0x80);
				if(ci->subop) ai->op ^= AA_NEG_FLIP;
				ai->oper[0] = (aaoper_t) {AAO_ZERO};
				ai->oper[1] = (aaoper_t) {AAO_INDEX, ci->oper[0].value};
				if(ci->implicit == 0xffff) {
					ai->oper[2] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					ai->oper[2] = (aaoper_t) {AAO_CODE, labelbase + ci->implicit};
				}
				break;
			case I_IF_GREATER:
				ai = add_instr(AA_IF_GT);
				if(ci->subop) ai->op ^= AA_NEG_FLIP;
				ai->oper[0] = encode_value(ci->oper[0], prg);
				ai->oper[1] = encode_value(ci->oper[1], prg);
				if(ci->implicit == 0xffff) {
					ai->oper[2] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					ai->oper[2] = (aaoper_t) {AAO_CODE, labelbase + ci->implicit};
				}
				break;
			case I_IF_GVAR_EQ:
				assert(ci->oper[0].tag == OPER_GVAR);
				if(ci->oper[1].tag == VAL_OBJ && ci->oper[1].value < 0xff) {
					ai = add_instr(AA_IF_MEM_EQ_2 | 0x80);
					ai->oper[2] = (aaoper_t) {AAO_VBYTE, 1 + ci->oper[1].value};
				} else {
					ai = add_instr(AA_IF_MEM_EQ_1 | 0x80);
					ai->oper[2] = encode_value(ci->oper[1], prg);
				}
				if(ci->subop) ai->op ^= AA_NEG_FLIP;
				ai->oper[0] = (aaoper_t) {AAO_ZERO};
				ai->oper[1] = (aaoper_t) {AAO_INDEX, first_gvar + ci->oper[0].value};
				if(ci->implicit == 0xffff) {
					ai->oper[3] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					ai->oper[3] = (aaoper_t) {AAO_CODE, labelbase + ci->implicit};
				}
				break;
			case I_IF_HAVE_LINK:
				ai = add_instr(AA_VM_INFO);
				ai->oper[0] = (aaoper_t) {AAO_BYTE, AAFEAT_LINKS};
				ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
				ai = add_instr(AA_IF_RAW_EQ | 0x80);
				if(!ci->subop) ai->op ^= AA_NEG_FLIP;
				ai->oper[0] = (aaoper_t) {AAO_ZERO};
				ai->oper[1] = (aaoper_t) {AAO_REG, REG_TMP};
				if(ci->implicit == 0xffff) {
					ai->oper[2] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					ai->oper[2] = (aaoper_t) {AAO_CODE, labelbase + ci->implicit};
				}
				break;
			case I_IF_HAVE_UNDO:
				ai = add_instr(AA_VM_INFO);
				ai->oper[0] = (aaoper_t) {AAO_BYTE, AAFEAT_UNDO};
				ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
				ai = add_instr(AA_IF_RAW_EQ | 0x80);
				if(!ci->subop) ai->op ^= AA_NEG_FLIP;
				ai->oper[0] = (aaoper_t) {AAO_ZERO};
				ai->oper[1] = (aaoper_t) {AAO_REG, REG_TMP};
				if(ci->implicit == 0xffff) {
					ai->oper[2] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					ai->oper[2] = (aaoper_t) {AAO_CODE, labelbase + ci->implicit};
				}
				break;
			case I_IF_HAVE_QUIT:
				ai = add_instr(AA_VM_INFO);
				ai->oper[0] = (aaoper_t) {AAO_BYTE, AAFEAT_QUIT};
				ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
				ai = add_instr(AA_IF_RAW_EQ | 0x80);
				if(!ci->subop) ai->op ^= AA_NEG_FLIP;
				ai->oper[0] = (aaoper_t) {AAO_ZERO};
				ai->oper[1] = (aaoper_t) {AAO_REG, REG_TMP};
				if(ci->implicit == 0xffff) {
					ai->oper[2] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					ai->oper[2] = (aaoper_t) {AAO_CODE, labelbase + ci->implicit};
				}
				break;
			case I_IF_HAVE_STATUS:
				assert(ci->oper[0].tag == VAL_RAW);
				ai = add_instr(AA_VM_INFO);
				ai->oper[0] = (aaoper_t) {AAO_BYTE, ci->oper[0].value? AAFEAT_INLINE_AREA : AAFEAT_TOP_AREA};
				ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
				ai = add_instr(AA_IF_RAW_EQ | 0x80);
				if(!ci->subop) ai->op ^= AA_NEG_FLIP;
				ai->oper[0] = (aaoper_t) {AAO_ZERO};
				ai->oper[1] = (aaoper_t) {AAO_REG, REG_TMP};
				if(ci->implicit == 0xffff) {
					ai->oper[2] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					ai->oper[2] = (aaoper_t) {AAO_CODE, labelbase + ci->implicit};
				}
				break;
			case I_IF_MATCH:
				if(ci->oper[1].tag == VAL_NIL) {
					ai = add_instr(AA_IF_EMPTY);
					if(ci->subop) ai->op ^= AA_NEG_FLIP;
					ai->oper[0] = encode_value(ci->oper[0], prg);
					if(ci->implicit == 0xffff) {
						ai->oper[1] = (aaoper_t) {AAO_CODE, AAFAIL};
					} else {
						ai->oper[1] = (aaoper_t) {AAO_CODE, labelbase + ci->implicit};
					}
				} else {
					ai = add_instr(AA_IF_EQ);
					if(ci->subop) ai->op ^= AA_NEG_FLIP;
					ai->oper[0] = (aaoper_t) {AAO_WORD, tag_eval_value(ci->oper[1], prg)};
					if(ai->oper[0].value < 0x100) {
						ai->op |= 0x80;
						ai->oper[0].type = AAO_VBYTE;
					}
					ai->oper[1] = encode_value(ci->oper[0], prg);
					if(ci->implicit == 0xffff) {
						ai->oper[2] = (aaoper_t) {AAO_CODE, AAFAIL};
					} else {
						ai->oper[2] = (aaoper_t) {AAO_CODE, labelbase + ci->implicit};
					}
				}
				break;
			case I_IF_NIL:
				ai = add_instr(AA_IF_EMPTY);
				if(ci->subop) ai->op ^= AA_NEG_FLIP;
				ai->oper[0] = encode_value(ci->oper[0], prg);
				if(ci->implicit == 0xffff) {
					ai->oper[1] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					ai->oper[1] = (aaoper_t) {AAO_CODE, labelbase + ci->implicit};
				}
				break;
			case I_IF_NUM:
				ai = add_instr(AA_IF_NUM);
				if(ci->subop) ai->op ^= AA_NEG_FLIP;
				ai->oper[0] = encode_value(ci->oper[0], prg);
				if(ci->implicit == 0xffff) {
					ai->oper[1] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					ai->oper[1] = (aaoper_t) {AAO_CODE, labelbase + ci->implicit};
				}
				break;
			case I_IF_OBJ:
				ai = add_instr(AA_IF_OBJ);
				if(ci->subop) ai->op ^= AA_NEG_FLIP;
				ai->oper[0] = encode_value(ci->oper[0], prg);
				if(ci->implicit == 0xffff) {
					ai->oper[1] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					ai->oper[1] = (aaoper_t) {AAO_CODE, labelbase + ci->implicit};
				}
				break;
			case I_IF_OFLAG:
				assert(ci->oper[0].tag == OPER_OFLAG);
				ai = add_instr(AA_IF_FLAG);
				if(ci->subop) ai->op ^= AA_NEG_FLIP;
				ai->oper[0] = encode_value(ci->oper[1], prg);
				ai->oper[1] = (aaoper_t) {AAO_INDEX, 48 + ci->oper[0].value};
				if(ci->implicit == 0xffff) {
					ai->oper[2] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					ai->oper[2] = (aaoper_t) {AAO_CODE, labelbase + ci->implicit};
				}
				break;
			case I_IF_OVAR_EQ:
				assert(ci->oper[0].tag == OPER_OVAR);
				if(ci->oper[2].tag == VAL_OBJ && ci->oper[2].value < 0xff) {
					ai = add_instr(AA_IF_MEM_EQ_2);
					ai->oper[2] = (aaoper_t) {AAO_VBYTE, 1 + ci->oper[2].value};
				} else {
					ai = add_instr(AA_IF_MEM_EQ_1);
					ai->oper[2] = encode_value(ci->oper[2], prg);
				}
				if(ci->subop) ai->op ^= AA_NEG_FLIP;
				ai->oper[0] = encode_value(ci->oper[1], prg);
				if(ci->oper[0].value == DYN_HASPARENT) {
					ai->oper[1] = (aaoper_t) {AAO_INDEX, OVAR_PARENT};
				} else {
					ai->oper[1] = (aaoper_t) {AAO_INDEX, first_ovar + (ci->oper[0].value - 1)};
				}
				if(ci->implicit == 0xffff) {
					ai->oper[3] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					ai->oper[3] = (aaoper_t) {AAO_CODE, labelbase + ci->implicit};
				}
				break;
			case I_IF_PAIR:
				ai = add_instr(AA_IF_PAIR);
				if(ci->subop) ai->op ^= AA_NEG_FLIP;
				ai->oper[0] = encode_value(ci->oper[0], prg);
				if(ci->implicit == 0xffff) {
					ai->oper[1] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					ai->oper[1] = (aaoper_t) {AAO_CODE, labelbase + ci->implicit};
				}
				break;
			case I_IF_UNIFY:
				ai = add_instr(AA_IF_UNIFY);
				if(ci->subop) ai->op ^= AA_NEG_FLIP;
				ai->oper[0] = encode_value(ci->oper[0], prg);
				ai->oper[1] = encode_value(ci->oper[1], prg);
				if(ci->implicit == 0xffff) {
					ai->oper[2] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					ai->oper[2] = (aaoper_t) {AAO_CODE, labelbase + ci->implicit};
				}
				break;
			case I_IF_WORD:
				ai = add_instr(AA_IF_WORD);
				if(ci->subop) ai->op ^= AA_NEG_FLIP;
				ai->oper[0] = encode_value(ci->oper[0], prg);
				if(ci->implicit == 0xffff) {
					ai->oper[1] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					ai->oper[1] = (aaoper_t) {AAO_CODE, labelbase + ci->implicit};
				}
				break;
			case I_IF_UNKNOWN_WORD:
				ai = add_instr(AA_IF_UWORD);
				if(ci->subop) ai->op ^= AA_NEG_FLIP;
				ai->oper[0] = encode_value(ci->oper[0], prg);
				if(ci->implicit == 0xffff) {
					ai->oper[1] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					ai->oper[1] = (aaoper_t) {AAO_CODE, labelbase + ci->implicit};
				}
				break;
			case I_INVOKE_MULTI:
				assert(ci->oper[0].tag == OPER_PRED);
				ai = add_instr(AA_JMP_MULTI);
				ai->oper[0] = (aaoper_t) {AAO_CODE, ci->oper[0].value};
				break;
			case I_INVOKE_ONCE:
				assert(ci->oper[0].tag == OPER_PRED);
				ai = add_instr(AA_JMP_SIMPLE);
				ai->oper[0] = (aaoper_t) {AAO_CODE, ci->oper[0].value};
				break;
			case I_INVOKE_TAIL_MULTI:
				assert(ci->oper[0].tag == OPER_PRED);
				ai = add_instr(AA_JMP);
				ai->oper[0] = (aaoper_t) {AAO_CODE, ci->oper[0].value};
				break;
			case I_INVOKE_TAIL_ONCE:
				assert(ci->oper[0].tag == OPER_PRED);
				ai = add_instr(AA_JMP_TAIL);
				ai->oper[0] = (aaoper_t) {AAO_CODE, ci->oper[0].value};
				break;
			case I_JOIN_WORDS:
				ai = add_instr(AA_JOIN_WORDS);
				ai->oper[0] = encode_value(ci->oper[0], prg);
				ai->oper[1] = encode_dest(ci->oper[1], prg, 0);
				break;
			case I_JUMP:
				if(ci->oper[0].tag == OPER_FAIL) {
					ai = add_instr(AA_FAIL);
				} else {
					assert(ci->oper[0].tag == OPER_RLAB);
					ai = add_instr(AA_JMP);
					ai->oper[0] = (aaoper_t) {AAO_CODE, labelbase + ci->oper[0].value};
				}
				break;
			case I_MAKE_VAR:
				ai = add_instr(AA_MAKE_VAR);
				ai->oper[0] = encode_dest(ci->oper[0], prg, 0);
				break;
			case I_NEXT_CHILD_PUSH:
				ll = nextlabel++;
				ai = add_instr(AA_LOAD_WORD);
				ai->oper[0] = encode_value(ci->oper[0], prg);
				ai->oper[1] = (aaoper_t) {AAO_INDEX, OVAR_SIBLING};
				ai->oper[2] = (aaoper_t) {AAO_STORE_REG, REG_A + 1};
				ai = add_instr(AA_IF_RAW_EQ | 0x80);
				ai->oper[0] = (aaoper_t) {AAO_ZERO};
				ai->oper[1] = (aaoper_t) {AAO_REG, REG_A + 1};
				ai->oper[2] = (aaoper_t) {AAO_CODE, ll};
				ai = add_instr(AA_PUSH_CHOICE);
				ai->oper[0] = (aaoper_t) {AAO_BYTE, 2};
				if(ci->oper[1].tag == OPER_FAIL) {
					ai->oper[1] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					assert(ci->oper[1].tag == OPER_RLAB);
					ai->oper[1] = (aaoper_t) {AAO_CODE, labelbase + ci->oper[1].value};
				}
				add_label(ll);
				break;
			case I_NEXT_OBJ_PUSH:
				ll = nextlabel++;
				if(ci->oper[0].tag == VAL_OBJ && ci->oper[0].value == 0) {
					ai = add_instr(AA_ASSIGN | 0x80);
					ai->oper[0] = (aaoper_t) {AAO_VBYTE, 2};
					ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_A + 1};
				} else {
					ai = add_instr(AA_INC_RAW);
					ai->oper[0] = encode_value(ci->oper[0], prg);
					ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_A + 1};
				}
				ai = add_instr(AA_IF_EQ);
				ai->oper[0] = (aaoper_t) {AAO_WORD, prg->nworldobj + 1};
				ai->oper[1] = (aaoper_t) {AAO_REG, REG_A + 1};
				ai->oper[2] = (aaoper_t) {AAO_CODE, ll};
				ai = add_instr(AA_PUSH_CHOICE);
				ai->oper[0] = (aaoper_t) {AAO_BYTE, 2};
				if(ci->oper[1].tag == OPER_FAIL) {
					ai->oper[1] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					assert(ci->oper[1].tag == OPER_RLAB);
					ai->oper[1] = (aaoper_t) {AAO_CODE, labelbase + ci->oper[1].value};
				}
				add_label(ll);
				break;
			case I_NEXT_OFLAG_PUSH:
				assert(ci->oper[0].tag == OPER_OFLAG);
				assert(prg->objflagpred[ci->oper[0].value]->pred->flags & PREDF_DYN_LINKAGE);
				ll = nextlabel++;
				ai = add_instr(AA_LOAD_WORD);
				ai->oper[0] = encode_value(ci->oper[1], prg);
				ai->oper[1] = (aaoper_t) {AAO_INDEX, first_ovar + (prg->nobjvar - 1) + dynlink_id[ci->oper[0].value]};
				ai->oper[2] = (aaoper_t) {AAO_STORE_REG, REG_A + 1};
				ai = add_instr(AA_IF_RAW_EQ | 0x80);
				ai->oper[0] = (aaoper_t) {AAO_ZERO};
				ai->oper[1] = (aaoper_t) {AAO_REG, REG_A + 1};
				ai->oper[2] = (aaoper_t) {AAO_CODE, ll};
				ai = add_instr(AA_PUSH_CHOICE);
				ai->oper[0] = (aaoper_t) {AAO_BYTE, 2};
				if(ci->oper[2].tag == OPER_FAIL) {
					ai->oper[1] = (aaoper_t) {AAO_CODE, AAFAIL};
				} else {
					assert(ci->oper[2].tag == OPER_RLAB);
					ai->oper[1] = (aaoper_t) {AAO_CODE, labelbase + ci->oper[2].value};
				}
				add_label(ll);
				break;
			case I_NOP:
			case I_NOP_DEBUG:
				break;
			case I_POP_CHOICE:
				assert(ci->oper[0].tag == OPER_NUM);
				ai = add_instr(AA_POP_CHOICE);
				if(ci->oper[0].value) {
					ai->oper[0] = (aaoper_t) {AAO_BYTE, ci->oper[0].value};
				} else {
					ai->op |= 0x80;
					ai->oper[0] = (aaoper_t) {AAO_ZERO};
				}
				if(cr->instr[i + 1].op == I_PUSH_CHOICE
				&& cr->instr[i + 1].oper[0].tag == OPER_NUM
				&& cr->instr[i + 1].oper[0].value <= ci->oper[0].value) {
					assert(cr->instr[i + 1].oper[1].tag == OPER_RLAB);
					ai->op = (ai->op & 0x80) | AA_POP_PUSH_CHOICE;
					ai->oper[1] = (aaoper_t) {AAO_CODE, labelbase + cr->instr[i + 1].oper[1].value};
					i++;
				}
				break;
			case I_POP_STOP:
				ai = add_instr(AA_POP_STOP);
				break;
			case I_PREPARE_INDEX:
				ai = add_instr(AA_SET_IDX);
				ai->oper[0] = encode_value(ci->oper[0], prg);
				break;
			case I_PRINT_VAL:
				ai = add_instr(AA_PRINT_VAL);
				ai->oper[0] = encode_value(ci->oper[0], prg);
				break;
			case I_PRINT_WORDS:
				if(ci->oper[1].tag == VAL_NONE
				&& ci->oper[0].tag == OPER_WORD
				&& (w = prg->allwords[ci->oper[0].value])
				&& (w->flags & WORDF_DICT)
				&& only_lowercase(w->name)) {
					ai = add_instr(AA_PRINT_VAL);
					ai->oper[0] = (aaoper_t) {AAO_CONST, prg->dictmap[w->dict_id]};
					n = 1;
				} else if(ci->subop == 1) {
					n = generate_output(prg, &cr->instr[i], 0, 1);
				} else if(ci->subop == 0) {
					// This can only happen for unreachable code.
					n = 1;
				} else {
					n = generate_output(prg, &cr->instr[i], 1, 0);
					if(ci->subop == 3) {
						flag = 0;
						for(j = 0; j < n && !flag; j++) {
							if(cr->instr[i + j].op != I_PRINT_WORDS) {
								flag = 1;
								break;
							}
							for(k = 0; k < 3; k++) {
								if(cr->instr[i + j].oper[k].tag == OPER_WORD) {
									w = prg->allwords[cr->instr[i + j].oper[k].value];
									assert(w->flags & WORDF_DICT);
									if(strchr(NO_SPACE_BEFORE, w->name[0])) flag = 1;
									if(strchr(NO_SPACE_AFTER, w->name[strlen(w->name) - 1])) flag = 1;
									for(m = 0; w->name[m] && !flag; m++) {
										if((w->name[m] >= 'A' && w->name[m] <= 'Z')
										|| (w->name[m] & 0x80)) {
											flag = 1;
											//printf("%s\n", w->name);
										}
									}
								}
							}
						}
						if(!flag) {
							for(j = 0; j < n; j++) {
								if(cr->instr[i + j].op == I_PRINT_WORDS) {
									for(k = 0; k < 3; k++) {
										if(cr->instr[i + j].oper[k].tag == OPER_WORD) {
											w = prg->allwords[cr->instr[i + j].oper[k].value];
											assert(w->flags & WORDF_DICT);
											ai = add_instr(AA_PRINT_VAL);
											ai->oper[0] = (aaoper_t) {AAO_CONST, prg->dictmap[w->dict_id]};
										}
									}
								}
							}
						} else {
							ll = nextlabel++;
							ll2 = nextlabel++;
							ai = add_instr(AA_IF_CWL);
							ai->oper[0] = (aaoper_t) {AAO_CODE, ll};
							n = generate_output(prg, &cr->instr[i], 0, 0);
							ai = add_instr(AA_JMP);
							ai->oper[0] = (aaoper_t) {AAO_CODE, ll2};
							add_label(ll);
							for(j = 0; j < n; j++) {
								if(cr->instr[i + j].op == I_PRINT_WORDS) {
									for(k = 0; k < 3; k++) {
										if(cr->instr[i + j].oper[k].tag == OPER_WORD) {
											w = prg->allwords[cr->instr[i + j].oper[k].value];
											assert(w->flags & WORDF_DICT);
											ai = add_instr(AA_AUX_PUSH_RAW);
											ai->oper[0] = (aaoper_t) {AAO_WORD, prg->dictmap[w->dict_id]};
										}
									}
								}
							}
							add_label(ll2);
						}
					} else {
						for(j = 0; j < n; j++) {
							if(cr->instr[i + j].op == I_PRINT_WORDS) {
								for(k = 0; k < 3; k++) {
									if(cr->instr[i + j].oper[k].tag == OPER_WORD) {
										w = prg->allwords[cr->instr[i + j].oper[k].value];
										assert(w->flags & WORDF_DICT);
										ai = add_instr(AA_AUX_PUSH_RAW);
										ai->oper[0] = (aaoper_t) {AAO_WORD, prg->dictmap[w->dict_id]};
									}
								}
							}
						}
					}
				}
				i += n - 1;
				break;
			case I_PROCEED:
				ai = add_instr(AA_PROCEED);
				break;
			case I_BREAKPOINT:
				ai = add_instr(AA_PROCEED);
				break;
			case I_PUSH_CHOICE:
				assert(ci->oper[0].tag == OPER_NUM);
				assert(ci->oper[1].tag == OPER_RLAB);
				ai = add_instr(AA_PUSH_CHOICE);
				if(ci->oper[0].value) {
					ai->oper[0] = (aaoper_t) {AAO_BYTE, ci->oper[0].value};
				} else {
					ai->op |= 0x80;
					ai->oper[0] = (aaoper_t) {AAO_ZERO};
				}
				ai->oper[1] = (aaoper_t) {AAO_CODE, labelbase + ci->oper[1].value};
				break;
			case I_PUSH_STOP:
				ai = add_instr(AA_PUSH_STOP);
				assert(ci->oper[0].tag == OPER_RLAB);
				ai->oper[0] = (aaoper_t) {AAO_CODE, labelbase + ci->oper[0].value};
				break;
			case I_QUIT:
				ai = add_instr(AA_EXT0);
				ai->oper[0] = (aaoper_t) {AAO_BYTE, AAEXT0_QUIT};
				break;
			case I_RESTART:
				ai = add_instr(AA_EXT0);
				ai->oper[0] = (aaoper_t) {AAO_BYTE, AAEXT0_RESTART};
				break;
			case I_RESTORE:
				ai = add_instr(AA_EXT0);
				ai->oper[0] = (aaoper_t) {AAO_BYTE, AAEXT0_RESTORE};
				break;
			case I_RESTORE_CHOICE:
				ai = add_instr(AA_SET_CHO);
				ai->oper[0] = encode_value(ci->oper[0], prg);
				break;
			case I_SAVE:
				ll = nextlabel++;
				ai = add_instr(AA_SAVE);
				ai->oper[0] = (aaoper_t) {AAO_CODE, ll};
				ai = add_instr(AA_ASSIGN);
				ai->oper[0] = (aaoper_t) {AAO_CONST, tag_eval_value((value_t) {VAL_NUM, 0}, prg)};
				ai->oper[1] = (aaoper_t) {AAO_REG, REG_A + 0};
				ai = add_instr(AA_PROCEED);
				add_label(ll);
				ai = add_instr(AA_ASSIGN);
				ai->oper[0] = (aaoper_t) {AAO_CONST, tag_eval_value((value_t) {VAL_NUM, 1}, prg)};
				ai->oper[1] = (aaoper_t) {AAO_REG, REG_A + 0};
				ai = add_instr(AA_PROCEED);
				break;
			case I_SAVE_CHOICE:
				ai = add_instr(AA_GET_CHO);
				ai->oper[0] = encode_dest(ci->oper[0], prg, 0);
				break;
			case I_SAVE_UNDO:
				ll = nextlabel++;
				ai = add_instr(AA_SAVE_UNDO);
				ai->oper[0] = (aaoper_t) {AAO_CODE, ll};
				ai = add_instr(AA_ASSIGN);
				ai->oper[0] = (aaoper_t) {AAO_CONST, tag_eval_value((value_t) {VAL_NUM, 0}, prg)};
				ai->oper[1] = (aaoper_t) {AAO_REG, REG_A + 0};
				ai = add_instr(AA_PROCEED);
				add_label(ll);
				ai = add_instr(AA_ASSIGN);
				ai->oper[0] = (aaoper_t) {AAO_CONST, tag_eval_value((value_t) {VAL_NUM, 1}, prg)};
				ai->oper[1] = (aaoper_t) {AAO_REG, REG_A + 0};
				ai = add_instr(AA_PROCEED);
				break;
			case I_SELECT:
				assert(ci->oper[0].tag == OPER_NUM);
				n = ci->oper[0].value;
				if(ci->subop == SEL_P_RANDOM) {
					ai = add_instr(AA_RAND_RAW);
					ai->oper[0] = (aaoper_t) {AAO_BYTE, n - 1};
					ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_IDX};
				} else {
					assert(ci->oper[1].tag == OPER_NUM);
					sel = ci->oper[1].value;
					switch(ci->subop) {
					case SEL_STOPPING:
						ll = nextlabel++;
						ai = add_instr(AA_LOAD_BYTE | 0x80);
						ai->oper[0] = (aaoper_t) {AAO_ZERO};
						ai->oper[1] = (aaoper_t) {AAO_INDEX, first_sel_byte + sel};
						ai->oper[2] = (aaoper_t) {AAO_STORE_REG, REG_IDX};
						ai = add_instr(AA_CHECK_EQ | 0x80);
						ai->oper[0] = (aaoper_t) {AAO_BYTE, n - 1};
						ai->oper[1] = (aaoper_t) {AAO_CODE, ll};
						ai = add_instr(AA_INC_RAW);
						ai->oper[0] = (aaoper_t) {AAO_REG, REG_IDX};
						ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
						ai = add_instr(AA_STORE_BYTE | 0x80);
						ai->oper[0] = (aaoper_t) {AAO_ZERO};
						ai->oper[1] = (aaoper_t) {AAO_INDEX, first_sel_byte + sel};
						ai->oper[2] = (aaoper_t) {AAO_REG, REG_TMP};
						add_label(ll);
						break;
					case SEL_RANDOM:
						ll = nextlabel++;
						ll2 = nextlabel++;
						ll3 = nextlabel++;
						initial_sel[sel] = 0xff;
						ai = add_instr(AA_LOAD_BYTE | 0x80);
						ai->oper[0] = (aaoper_t) {AAO_ZERO};
						ai->oper[1] = (aaoper_t) {AAO_INDEX, first_sel_byte + sel};
						ai->oper[2] = (aaoper_t) {AAO_STORE_REG, REG_IDX};
						ai = add_instr(AA_CHECK_EQ | 0x80);
						ai->oper[0] = (aaoper_t) {AAO_BYTE, 0xff};
						ai->oper[1] = (aaoper_t) {AAO_CODE, ll};

						ai = add_instr(AA_RAND_RAW);
						ai->oper[0] = (aaoper_t) {AAO_BYTE, n - 2};
						ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
						ai = add_instr(AA_CHECK_GT);
						ai->oper[0] = (aaoper_t) {AAO_REG, REG_TMP};
						ai->oper[1] = (aaoper_t) {AAO_CODE, ll2};
						ai = add_instr(AA_INC_RAW);
						ai->oper[0] = (aaoper_t) {AAO_REG, REG_TMP};
						ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};

						add_label(ll2);
						ai = add_instr(AA_ASSIGN);
						ai->oper[0] = (aaoper_t) {AAO_REG, REG_TMP};
						ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_IDX};
						ai = add_instr(AA_JMP);
						ai->oper[0] = (aaoper_t) {AAO_CODE, ll3};

						add_label(ll);
						ai = add_instr(AA_RAND_RAW);
						ai->oper[0] = (aaoper_t) {AAO_BYTE, n - 1};
						ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_IDX};

						add_label(ll3);
						ai = add_instr(AA_STORE_BYTE | 0x80);
						ai->oper[0] = (aaoper_t) {AAO_ZERO};
						ai->oper[1] = (aaoper_t) {AAO_INDEX, first_sel_byte + sel};
						ai->oper[2] = (aaoper_t) {AAO_REG, REG_IDX};
						break;
					case SEL_T_RANDOM:
						ll = nextlabel++;
						ll2 = nextlabel++;
						ll3 = nextlabel++;
						ll4 = nextlabel++;
						ai = add_instr(AA_LOAD_BYTE | 0x80);
						ai->oper[0] = (aaoper_t) {AAO_ZERO};
						ai->oper[1] = (aaoper_t) {AAO_INDEX, first_sel_byte + sel};
						ai->oper[2] = (aaoper_t) {AAO_STORE_REG, REG_IDX};
						ai = add_instr(AA_CHECK_GT_EQ | 0x80);
						ai->oper[0] = (aaoper_t) {AAO_VBYTE, n - 1};
						ai->oper[1] = (aaoper_t) {AAO_CODE, ll};
						ai->oper[2] = (aaoper_t) {AAO_CODE, ll2};

						ai = add_instr(AA_INC_RAW);
						ai->oper[0] = (aaoper_t) {AAO_REG, REG_IDX};
						ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
						ai = add_instr(AA_JMP);
						ai->oper[0] = (aaoper_t) {AAO_CODE, ll3};

						add_label(ll2);
						ai = add_instr(AA_ASSIGN | 0x80);
						ai->oper[0] = (aaoper_t) {AAO_VBYTE, n + (n - 1)};
						ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
						ai = add_instr(AA_JMP);
						ai->oper[0] = (aaoper_t) {AAO_CODE, ll3};

						add_label(ll);
						ai = add_instr(AA_SUB_RAW);
						ai->oper[0] = (aaoper_t) {AAO_REG, REG_IDX};
						ai->oper[1] = (aaoper_t) {AAO_CONST, n};
						ai->oper[2] = (aaoper_t) {AAO_STORE_REG, REG_IDX};
						ai = add_instr(AA_RAND_RAW);
						ai->oper[0] = (aaoper_t) {AAO_BYTE, n - 2};
						ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
						ai = add_instr(AA_CHECK_GT);
						ai->oper[0] = (aaoper_t) {AAO_REG, REG_TMP};
						ai->oper[1] = (aaoper_t) {AAO_CODE, ll4};
						ai = add_instr(AA_INC_RAW);
						ai->oper[0] = (aaoper_t) {AAO_REG, REG_TMP};
						ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};

						add_label(ll4);
						ai = add_instr(AA_ASSIGN);
						ai->oper[0] = (aaoper_t) {AAO_REG, REG_TMP};
						ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_IDX};
						ai = add_instr(AA_ADD_RAW);
						ai->oper[0] = (aaoper_t) {AAO_REG, REG_IDX};
						ai->oper[1] = (aaoper_t) {AAO_CONST, n};
						ai->oper[2] = (aaoper_t) {AAO_STORE_REG, REG_TMP};

						add_label(ll3);
						ai = add_instr(AA_STORE_BYTE | 0x80);
						ai->oper[0] = (aaoper_t) {AAO_ZERO};
						ai->oper[1] = (aaoper_t) {AAO_INDEX, first_sel_byte + sel};
						ai->oper[2] = (aaoper_t) {AAO_REG, REG_TMP};
						break;
					case SEL_T_P_RANDOM:
						ll = nextlabel++;
						ll2 = nextlabel++;
						ai = add_instr(AA_LOAD_BYTE | 0x80);
						ai->oper[0] = (aaoper_t) {AAO_ZERO};
						ai->oper[1] = (aaoper_t) {AAO_INDEX, first_sel_byte + sel};
						ai->oper[2] = (aaoper_t) {AAO_STORE_REG, REG_IDX};
						ai = add_instr(AA_CHECK_EQ | 0x80);
						ai->oper[0] = (aaoper_t) {AAO_BYTE, n};
						ai->oper[1] = (aaoper_t) {AAO_CODE, ll};
						ai = add_instr(AA_INC_RAW);
						ai->oper[0] = (aaoper_t) {AAO_REG, REG_IDX};
						ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
						ai = add_instr(AA_STORE_BYTE | 0x80);
						ai->oper[0] = (aaoper_t) {AAO_ZERO};
						ai->oper[1] = (aaoper_t) {AAO_INDEX, first_sel_byte + sel};
						ai->oper[2] = (aaoper_t) {AAO_REG, REG_TMP};
						ai = add_instr(AA_JMP);
						ai->oper[0] = (aaoper_t) {AAO_CODE, ll2};

						add_label(ll);
						ai = add_instr(AA_RAND_RAW);
						ai->oper[0] = (aaoper_t) {AAO_BYTE, n - 1};
						ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_IDX};

						add_label(ll2);
						break;
					case SEL_CYCLING:
						ll = nextlabel++;
						ai = add_instr(AA_LOAD_BYTE | 0x80);
						ai->oper[0] = (aaoper_t) {AAO_ZERO};
						ai->oper[1] = (aaoper_t) {AAO_INDEX, first_sel_byte + sel};
						ai->oper[2] = (aaoper_t) {AAO_STORE_REG, REG_IDX};
						ai = add_instr(AA_INC_RAW);
						ai->oper[0] = (aaoper_t) {AAO_REG, REG_IDX};
						ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
						ai = add_instr(AA_IFN_EQ | 0x80);
						ai->oper[0] = (aaoper_t) {AAO_VBYTE, n};
						ai->oper[1] = (aaoper_t) {AAO_REG, REG_TMP};
						ai->oper[2] = (aaoper_t) {AAO_CODE, ll};
						ai = add_instr(AA_ASSIGN | 0x80);
						ai->oper[0] = (aaoper_t) {AAO_VBYTE, 0};
						ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_TMP};

						add_label(ll);
						ai = add_instr(AA_STORE_BYTE | 0x80);
						ai->oper[0] = (aaoper_t) {AAO_ZERO};
						ai->oper[1] = (aaoper_t) {AAO_INDEX, first_sel_byte + sel};
						ai->oper[2] = (aaoper_t) {AAO_REG, REG_TMP};
						break;
					default:
						assert(0); exit(1);
					}
				}
				break;
			case I_SET_CONT:
				ai = add_instr(AA_SET_CONT);
				if(ci->oper[0].tag == OPER_RLAB) {
					ai->oper[0] = (aaoper_t) {AAO_CODE, labelbase + ci->oper[0].value};
				} else {
					assert(ci->oper[0].tag == OPER_FAIL);
					ai->oper[0] = (aaoper_t) {AAO_CODE, AAFAIL};
				}
				break;
			case I_SET_GFLAG:
				assert(ci->oper[0].tag == OPER_GFLAG);
				if(ci->subop) {
					ai = add_instr(AA_SET_FLAG | 0x80);
				} else {
					ai = add_instr(AA_RESET_FLAG | 0x80);
				}
				ai->oper[0] = (aaoper_t) {AAO_ZERO};
				ai->oper[1] = (aaoper_t) {AAO_INDEX, ci->oper[0].value};
				break;
			case I_SET_GVAR:
				assert(ci->oper[0].tag == OPER_GVAR);
				ai = add_instr(AA_STORE_VAL | 0x80);
				ai->oper[0] = (aaoper_t) {AAO_ZERO};
				ai->oper[1] = (aaoper_t) {AAO_INDEX, first_gvar + ci->oper[0].value};
				ai->oper[2] = encode_value(ci->oper[1], prg);
				break;
			case I_SET_OFLAG:
				assert(ci->oper[0].tag == OPER_OFLAG);
				if(prg->objflagpred[ci->oper[0].value]->pred->flags & PREDF_DYN_LINKAGE) {
					if(ci->subop) {
						ll = nextlabel++;
						ai = add_instr(AA_IF_FLAG);
						ai->oper[0] = encode_value(ci->oper[1], prg);
						ai->oper[1] = (aaoper_t) {AAO_INDEX, 48 + ci->oper[0].value};
						ai->oper[2] = (aaoper_t) {AAO_CODE, ll};
						ai = add_instr(AA_SET_FLAG);
						ai->oper[0] = encode_value(ci->oper[1], prg);
						ai->oper[1] = (aaoper_t) {AAO_INDEX, 48 + ci->oper[0].value};
						ai = add_instr(AA_LOAD_WORD | 0x80);
						ai->oper[0] = (aaoper_t) {AAO_ZERO};
						ai->oper[1] = (aaoper_t) {AAO_INDEX, first_gvar + prg->nglobalvar + dynlink_id[ci->oper[0].value]};
						ai->oper[2] = (aaoper_t) {AAO_STORE_REG, REG_TMP};
						ai = add_instr(AA_STORE_WORD);
						ai->oper[0] = encode_value(ci->oper[1], prg);
						ai->oper[1] = (aaoper_t) {AAO_INDEX, first_ovar + (prg->nobjvar - 1) + dynlink_id[ci->oper[0].value]};
						ai->oper[2] = (aaoper_t) {AAO_REG, REG_TMP};
						ai = add_instr(AA_STORE_VAL | 0x80);
						ai->oper[0] = (aaoper_t) {AAO_ZERO};
						ai->oper[1] = (aaoper_t) {AAO_INDEX, first_gvar + prg->nglobalvar + dynlink_id[ci->oper[0].value]};
						ai->oper[2] = encode_value(ci->oper[1], prg);
						add_label(ll);
					} else {
						ll = nextlabel++;
						ai = add_instr(AA_IFN_FLAG);
						ai->oper[0] = encode_value(ci->oper[1], prg);
						ai->oper[1] = (aaoper_t) {AAO_INDEX, 48 + ci->oper[0].value};
						ai->oper[2] = (aaoper_t) {AAO_CODE, ll};
						ai = add_instr(AA_RESET_FLAG);
						ai->oper[0] = encode_value(ci->oper[1], prg);
						ai->oper[1] = (aaoper_t) {AAO_INDEX, 48 + ci->oper[0].value};
						ai = add_instr(AA_UNLINK | 0x80);
						ai->oper[0] = (aaoper_t) {AAO_ZERO};
						ai->oper[1] = (aaoper_t) {AAO_INDEX, first_gvar + prg->nglobalvar + dynlink_id[ci->oper[0].value]};
						ai->oper[2] = (aaoper_t) {AAO_INDEX, first_ovar + (prg->nobjvar - 1) + dynlink_id[ci->oper[0].value]};
						ai->oper[3] = encode_value(ci->oper[1], prg);
						add_label(ll);
					}
				} else {
					if(ci->subop) {
						ai = add_instr(AA_SET_FLAG);
					} else {
						ai = add_instr(AA_RESET_FLAG);
					}
					ai->oper[0] = encode_value(ci->oper[1], prg);
					ai->oper[1] = (aaoper_t) {AAO_INDEX, 48 + ci->oper[0].value};
				}
				break;
			case I_SET_OVAR:
				assert(ci->oper[0].tag == OPER_OVAR);
				if(ci->oper[0].value == DYN_HASPARENT) {
					if(ci->oper[2].tag == VAL_NONE) {
						ai = add_instr(AA_SET_PARENT_B);
						ai->oper[0] = encode_value(ci->oper[1], prg);
						ai->oper[1] = (aaoper_t) {AAO_VBYTE, 0};
					} else if(ci->oper[2].tag == VAL_OBJ && ci->oper[2].value < 255) {
						ai = add_instr(AA_SET_PARENT_B);
						ai->oper[0] = encode_value(ci->oper[1], prg);
						ai->oper[1] = (aaoper_t) {AAO_VBYTE, ci->oper[2].value + 1};
					} else {
						ai = add_instr(AA_SET_PARENT_V);
						ai->oper[0] = encode_value(ci->oper[1], prg);
						ai->oper[1] = encode_value(ci->oper[2], prg);
					}
					if(ci->oper[1].tag == VAL_OBJ && ci->oper[1].value < 255) {
						ai->op |= 0x80;
						ai->oper[0] = (aaoper_t) {AAO_VBYTE, ci->oper[1].value + 1};
					}
				} else {
					ai = add_instr(AA_STORE_VAL);
					ai->oper[0] = encode_value(ci->oper[1], prg);
					ai->oper[1] = (aaoper_t) {AAO_INDEX, first_ovar + (ci->oper[0].value - 1)};
					ai->oper[2] = encode_value(ci->oper[2], prg);
				}
				break;
			case I_SPLIT_LIST:
				ai = add_instr(AA_SPLIT_LIST);
				ai->oper[0] = encode_value(ci->oper[0], prg);
				ai->oper[1] = encode_value(ci->oper[1], prg);
				ai->oper[2] = encode_dest(ci->oper[2], prg, 1);
				break;
			case I_SPLIT_WORD:
				ai = add_instr(AA_SPLIT_WORD);
				ai->oper[0] = encode_value(ci->oper[0], prg);
				ai->oper[1] = encode_dest(ci->oper[1], prg, 0);
				break;
			case I_STOP:
				ai = add_instr(AA_STOP);
				break;
			case I_TRACEPOINT:
				if(tracelabels[ci->subop]) {
					assert(ci->oper[0].tag == OPER_FILE);
					assert(ci->oper[1].tag == OPER_NUM);
					assert(ci->oper[2].tag == OPER_PRED);
					if(prg->predicates[ci->oper[2].value]->builtin != BI_GETINPUT
					&& prg->predicates[ci->oper[2].value]->builtin != BI_GETKEY) {
						char buf[128];
						snprintf(buf, sizeof(buf), prg->predicates[ci->oper[2].value]->printed_name + 1);
						assert(strlen(buf));
						buf[strlen(buf) - 1] = 0;
						ai = add_instr(AA_TRACEPOINT);
						ai->oper[0] = (aaoper_t) {AAO_STRING, findstring((uint8_t *) tracelabels[ci->subop])};
						ai->oper[1] = (aaoper_t) {AAO_STRING, findstring((uint8_t *) buf)};
						ai->oper[2] = (aaoper_t) {AAO_STRING, findstring((uint8_t *) sourcefile[ci->oper[0].value])};
						ai->oper[3] = (aaoper_t) {AAO_WORD, ci->oper[1].value};
					}
				}
				break;
			case I_TRANSCRIPT:
				ai = add_instr(AA_EXT0);
				if(ci->subop) {
					ai->oper[0] = (aaoper_t) {AAO_BYTE, AAEXT0_SCRIPT_ON};
				} else {
					ai->oper[0] = (aaoper_t) {AAO_BYTE, AAEXT0_SCRIPT_OFF};
				}
				break;
			case I_UNDO:
				ai = add_instr(AA_EXT0);
				ai->oper[0] = (aaoper_t) {AAO_BYTE, AAEXT0_UNDO};
				break;
			case I_UNIFY:
				ai = add_instr(AA_ASSIGN);
				if(ci->oper[0].tag == OPER_VAR
				|| ci->oper[0].tag == OPER_ARG
				|| ci->oper[0].tag == OPER_TEMP) {
					if(ci->oper[1].tag == VAL_OBJ && ci->oper[1].value < 0xff) {
						ai->op |= 0x80;
						ai->oper[0] = (aaoper_t) {AAO_VBYTE, 1 + ci->oper[1].value};
					} else {
						ai->oper[0] = encode_value(ci->oper[1], prg);
					}
					ai->oper[1] = encode_dest(ci->oper[0], prg, 1);
				} else if(ci->oper[1].tag == OPER_VAR
				|| ci->oper[1].tag == OPER_ARG
				|| ci->oper[1].tag == OPER_TEMP) {
					if(ci->oper[0].tag == VAL_OBJ && ci->oper[0].value < 0xff) {
						ai->op |= 0x80;
						ai->oper[0] = (aaoper_t) {AAO_VBYTE, 1 + ci->oper[0].value};
					} else {
						ai->oper[0] = encode_value(ci->oper[0], prg);
					}
					ai->oper[1] = encode_dest(ci->oper[1], prg, 1);
				} else {
					assert(0);
				}
				break;
			default:
				printf("unimplemented op %s in R%d of %s\n",
					opinfo[ci->op].name,
					sub_r_id,
					pred->predname->printed_name);
				assert(0); exit(1);
			}
		}
		end_segment();
	}
}

static void compile_predicate(struct predname *predname, struct program *prg) {
	struct predicate *pred = predname->pred;
	int labelbase;

	if(((pred->flags & PREDF_INVOKED) && !(pred->flags & PREDF_DYNAMIC))
	|| (pred->flags & (PREDF_DYN_LINKAGE | PREDF_NEEDS_LABEL))
	|| predname->builtin == BI_HASPARENT
	|| predname->builtin == BI_OBJECT) {
		if(pred->normal_entry >= 0) {
			labelbase = nextlabel;
			nextlabel += pred->nroutine;
			compile_routines(prg, pred, pred->normal_entry, labelbase);
		}
	}
}

static void visit_reachable_instr(int inum, int *symbols) {
	uint8_t op;
	int i, j;

	for(;;) {
		aainstr[inum].flags |= AAIF_REACHABLE;
		op = aainstr[inum].op;
		for(i = 0; i < 4; i++) {
			if(aainstr[inum].oper[i].type == AAO_CODE
			&& aainstr[inum].oper[i].value != AAFAIL) {
				j = symbols[aainstr[inum].oper[i].value];
				assert(j);
				if(!(aainstr[j].flags & AAIF_REACHABLE)) {
					visit_reachable_instr(j, symbols);
				}
			}
		}
		if(op == AA_FAIL
		|| op == AA_PROCEED
		|| op == AA_JMP
		|| op == AA_JMP_MULTI
		|| op == AA_JMP_SIMPLE
		|| op == AA_JMP_TAIL
		|| op == AA_POP_ENV_PROCEED
		|| op == AA_STOP
		|| (op == AA_EXT0 &&
			aainstr[inum].oper[0].value == AAEXT0_QUIT))
		{
			break;
		}
		inum++;
	}
}

static void compile_program(struct program *prg) {
	struct aainstr *ai;
	int i, j;
	uint32_t ll;
	int *symbols;

	// the first prg->npredicate labels are predicate entry points
	nextlabel = prg->npredicate;

	// address 0: fail
	ai = add_instr(AA_FAIL);

	// address 1: bootstrap code

	ai = add_instr(AA_ASSIGN);
	ai->oper[0] = (aaoper_t) {AAO_CONST, 0x3f00};
	ai->oper[1] = (aaoper_t) {AAO_STORE_REG, REG_NIL};

	ll = nextlabel++;

	ai = add_instr(AA_SET_CONT);
	ai->oper[0] = (aaoper_t) {AAO_CODE, ll};
	ai = add_instr(AA_PUSH_STOP);
	ai->oper[0] = (aaoper_t) {AAO_CODE, ll};

	ai = add_instr(AA_LINE);

	ai = add_instr(AA_IF_RAW_EQ | 0x80);
	ai->oper[0] = (aaoper_t) {AAO_ZERO};
	ai->oper[1] = (aaoper_t) {AAO_REG, 0};
	ai->oper[2] = (aaoper_t) {AAO_CODE, find_builtin(prg, BI_PROGRAM_ENTRY)->pred_id};
	ai = add_instr(AA_JMP);
	ai->oper[0] = (aaoper_t) {AAO_CODE, find_builtin(prg, BI_ERROR_ENTRY)->pred_id};

	add_label(ll);
	ai = add_instr(AA_EXT0);
	ai->oper[0] = (aaoper_t) {AAO_BYTE, AAEXT0_QUIT};

	end_segment();

	// predicate code

	for(i = 0; i < prg->npredicate; i++) {
		compile_predicate(prg->predicates[i], prg);
	}

	flatten_segments(prg);

	// eliminate jumps to single, small instructions
	symbols = calloc(nextlabel, sizeof(int));
	for(i = 0; i < ninstr; i++) {
		if(aainstr[i].op == AA_LABEL) {
			j = i + 1;
			while(aainstr[j].op == AA_LABEL) j++;
			symbols[aainstr[i].oper[0].value] = j;
		}
	}
	for(i = ninstr - 1; i >= 0; i--) {
		if(aainstr[i].op == AA_JMP
		&& aainstr[i].oper[0].value != AAFAIL) {
			j = symbols[aainstr[i].oper[0].value];
			assert(j);
			if(aainstr[j].op == AA_FAIL
			|| aainstr[j].op == AA_PROCEED
			|| aainstr[j].op == AA_POP_ENV_PROCEED
			|| aainstr[j].op == AA_STOP
			|| (aainstr[j].op == AA_EXT0 && (
				aainstr[j].oper[0].value == AAEXT0_QUIT ||
				aainstr[j].oper[0].value == AAEXT0_RESTART)))
			{
				memcpy(&aainstr[i], &aainstr[j], sizeof(struct aainstr));
			}
		}
	}

	// mark live code
	visit_reachable_instr(0, symbols);
	visit_reachable_instr(1, symbols);

	free(symbols);

	// transform jmp 0 to fail
	// transform set_cont + jmp to jmpl etc.
	// eliminate dead instructions
	for(i = 0; i < ninstr - 2; i++) {
		if(aainstr[i].op != AA_LABEL
		&& !(aainstr[i].flags & AAIF_REACHABLE)) {
			aainstr[i].op = AA_SKIP;
			memset(aainstr[i].oper, 0, sizeof(aainstr[i].oper));
		}
		if(aainstr[i].op == AA_JMP
		&& aainstr[i].oper[0].value == AAFAIL) {
			aainstr[i].op = AA_FAIL;
			aainstr[i].oper[0] = (aaoper_t) {AAO_NONE};
		}
		if(aainstr[i].op == AA_JMP
		&& aainstr[i + 1].op == AA_LABEL
		&& aainstr[i].oper[0].value == aainstr[i + 1].oper[0].value) {
			aainstr[i].op = AA_SKIP;
		}
		if(aainstr[i].op == AA_JMP_TAIL
		&& aainstr[i + 1].op == AA_LABEL
		&& aainstr[i].oper[0].value == aainstr[i + 1].oper[0].value) {
			aainstr[i].op = AA_TAIL;
			aainstr[i].oper[0] = (aaoper_t) {AAO_NONE};
		}
		if(aainstr[i].op == AA_SET_CONT
		&& aainstr[i + 1].op == AA_JMP_MULTI
		&& aainstr[i + 2].op == AA_LABEL
		&& aainstr[i].oper[0].value == aainstr[i + 2].oper[0].value) {
			aainstr[i].op = AA_SKIP;
			aainstr[i + 1].op = AA_JMPL_MULTI;
		}
		if(aainstr[i].op == AA_SET_CONT
		&& aainstr[i + 1].op == AA_JMP_SIMPLE
		&& aainstr[i + 2].op == AA_LABEL
		&& aainstr[i].oper[0].value == aainstr[i + 2].oper[0].value) {
			aainstr[i].op = AA_SKIP;
			aainstr[i + 1].op = AA_JMPL_SIMPLE;
		}
	}
}

static void analyze_resources(struct program *prg) {
	int i, pos, bytes;
	uint16_t ubuf[2];
	int nalloc = 0;
	uint8_t *aabuf = 0, *str;

	altstring = malloc(prg->nresource * sizeof(int));
	for(i = 0; i < prg->nresource; i++) {
		str = (uint8_t *) prg->resources[i].alt;
		pos = 0;
		while((bytes = utf8_to_unicode(ubuf, 2, str))) {
			if(pos + 1 >= nalloc) {
				nalloc = 2 * (pos + 4);
				aabuf = realloc(aabuf, nalloc);
			}
			aabuf[pos++] = resolve_aachar(ubuf[0]);
			str += bytes;
		}
		assert(pos);
		assert(nalloc);
		assert(pos < nalloc);
		aabuf[pos] = 0;
		altstring[i] = findstring(aabuf);
	}

	free(aabuf);
}

struct decodernode {
	uint32_t	occurrences;
	uint8_t		aachar;
	uint8_t		children[2];
	uint8_t		tablepos;
};

static void siftdown(struct decodernode *node, uint16_t *heap, int n, int i) {
	int min;

	while(i * 2 + 0 <= n) {
		min = i;
		if(node[heap[min]].occurrences > node[heap[i * 2 + 0]].occurrences) {
			min = i * 2 + 0;
		}
		if(i * 2 + 1 <= n
		&& node[heap[min]].occurrences > node[heap[i * 2 + 1]].occurrences) {
			min = i * 2 + 1;
		}
		if(min == i) break;
		heap[i] ^= heap[min];
		heap[min] ^= heap[i];
		heap[i] ^= heap[min];
		i = min;
	}
}

static void build_decoder_tree(struct decodernode *node, int i, uint32_t prefix, int nextbit, uint8_t *tableref) {
	int j, t;

	if(i < 129) {
		*tableref = i;
		prefix |= 1 << nextbit;
		charbits[i] = prefix;
		if(verbose >= 4) {
			for(j = 0; j < 16; j++) {
				if(prefix == 1) {
					printf(" ");
				} else {
					if(prefix & 1) {
						printf("1");
					} else {
						printf("0");
					}
					prefix >>= 1;
				}
			}
			printf(" %8d ", node[i].occurrences);
			if(i == 128) {
				printf("END\n");
			} else if(i == 0x5f) {
				printf("EXTENDED\n");
			} else if(node[i].aachar >= 0x80) {
				printf("0x%02x\n", node[i].aachar);
			} else {
				printf("'%c'\n", node[i].aachar);
			}
		}
	} else {
		t = n_decodetable++;
		*tableref = 0x80 | t;
		build_decoder_tree(node, node[i].children[0], prefix, nextbit + 1, &decodetable[t][0]);
		build_decoder_tree(node, node[i].children[1], prefix | (1 << nextbit), nextbit + 1, &decodetable[t][1]);
	}
}

static int find_dict_prefix(uint8_t *aastr) {
	int i, best = 2, argbest = -1;
	int begin, end, mid;
	struct dictentry *de;

	if(*aastr++ != ' ') return -1;
	begin = 0;
	end = ndict;
	while(begin < end) {
		mid = (begin + end) / 2;
		de = &dictionary[mid];
		for(i = 0; de->chars[i] && aastr[i] == de->chars[i]; i++);
		if(!de->chars[i]) {
			if(i > best) {
				best = i;
				argbest = mid;
			}
			begin = mid + 1;
		} else if(de->chars[i] > aastr[i]) {
			end = mid - 1;
		} else {
			begin = mid + 1;
		}
	}

	return argbest;
}

static void analyze_chars() {
	int i, j, dnum, nnode, nheap = 0;
	struct decodernode node[257];
	uint16_t heap[130];
	uint8_t rootref;

	for(i = 0; i < ncharmap; i++) {
		charmap[i].tolower = resolve_aachar(unicode_to_lower(charmap[i].glyph));
		charmap[i].toupper = resolve_aachar(unicode_to_upper(charmap[i].glyph));
	}

	memset(node, 0, sizeof(node));
	for(i = 0; i < 0x80; i++) {
		node[i].aachar = 0x20 + i;	// char in range 20..9f
	}
	node[0x80].aachar = 0;		// end
	node[0x5f].aachar = 0xff;	// extended
	nnode = 129;

	for(i = 0; i < n_textstr; i++) {
#if 0
		printf("%7d \"%s\"\n", i, textstrings[i].chars);
#endif
		for(j = 0; j < textstrings[i].length; ) {
			dnum = find_dict_prefix(textstrings[i].chars + j);
			if(dnum >= 0) {
#if 0
				printf("    %7d \"%s\"\n", j, dictionary[dnum].word->name);
#endif
				node[0x5f].occurrences++;
				j += 1 + strlen((char *) dictionary[dnum].chars);
			} else {
				if(textstrings[i].chars[j] >= 0x20
				&& textstrings[i].chars[j] < 0xa0) {
					node[textstrings[i].chars[j] - 0x20].occurrences++;
				} else {
					node[0x5f].occurrences++;
				}
				j++;
			}
		}
		node[0x80].occurrences++;
	}

	// The decoder table must have at least two nodes, since the root must be a choice.
	// Every string has at least one character and one end-of-string marker.
	if(!n_textstr) {
		node[0x20].occurrences++;
		node[0x80].occurrences++;
	}

	for(i = 0; i < nnode; i++) {
		heap[1 + nheap++] = i;
	}

	for(i = nheap / 2; i >= 1; i--) {
		siftdown(node, heap, nheap, i);
	}

#if 0
	for(i = 1; i <= nheap; i++) {
		printf("%3d %3d %8d %02x\n", i, heap[i], node[heap[i]].occurrences, node[heap[i]].aachar);
	}
#endif

	while(nheap > 1) {
		i = heap[1];
		heap[1] = heap[nheap--];
		siftdown(node, heap, nheap, 1);
		if(node[i].occurrences) {
			j = heap[1];
			node[nnode].occurrences = node[i].occurrences + node[j].occurrences;
			node[nnode].children[0] = i;
			node[nnode].children[1] = j;
			heap[1] = nnode++;
			siftdown(node, heap, nheap, 1);
		}
	}

	build_decoder_tree(node, heap[1], 0, 0, &rootref);
	assert(rootref == 0x80);

	decode_esc_boundary = ncharmap - 32;
	if(decode_esc_boundary < 0) decode_esc_boundary = 0;
	decode_esc_bits = 0;
	i = decode_esc_boundary + ndict - 1;;
	while(i > 0) {
		i >>= 1;
		decode_esc_bits++;
	}
#if 0
	printf("decode_esc_boundary: %d\n", decode_esc_boundary);
	printf("decode_esc_bits: %d\n", decode_esc_bits);
#endif
}

static int cmp_stringref(const void *a, const void *b) {
	const uint16_t *aa = a;
	const uint16_t *bb = b;
	int cost_a = (textstrings[*aa].bitlength + 7) / 8 - textstrings[*aa].occurrences;
	int cost_b = (textstrings[*bb].bitlength + 7) / 8 - textstrings[*bb].occurrences;
	return cost_a - cost_b;
}

static void analyze_strings() {
	int i, j, n, dnum, len;
	uint32_t bits;
	uint8_t charcost[129];
	uint8_t ch;
	uint16_t refs[n_textstr];
	struct textstring *ts;
	uint32_t org;

	for(i = 0; i < 129; i++) {
		bits = charbits[i];
		n = 0;
		while(bits > 1) {
			bits >>= 1;
			n++;
		}
		charcost[i] = n;
	}

	for(i = 0; i < n_textstr; i++) {
		len = charcost[0x80];
		for(j = 0; j < textstrings[i].length; ) {
			dnum = find_dict_prefix(textstrings[i].chars + j);
			if(dnum >= 0) {
				len += charcost[0x5f] + decode_esc_bits;
				j += 1 + strlen((char *) dictionary[dnum].chars);
			} else {
				ch = textstrings[i].chars[j];
				if(ch >= 0x20 && ch < 0xa0) {
					len += charcost[ch - 0x20];
				} else {
					len += charcost[0x5f] + decode_esc_bits;
				}
				j++;
			}
		}
		textstrings[i].bitlength = len;
		refs[i] = i;
	}

	qsort(refs, n_textstr, sizeof(uint16_t), cmp_stringref);

	org = 0;
	for(i = 0; i < n_textstr; i++) {
		ts = &textstrings[refs[i]];
		ts->address = org;
		//printf("%06x %4d %4d \"%s\"\n", org, (ts->bitlength + 7) / 8, ts->occurrences, ts->chars);
		org += (ts->bitlength + 7) / 8;
		if(org <= 253) {
			org = (org + 1) & ~1;
		}
	}
	writ_size = org;
}

static int compile_endings_check(uint8_t *dest, int org, struct endings_point *pt) {
	int i, nextfree;

	nextfree = org + pt->nway * 2 + 1;
	for(i = 0; i < pt->nway; i++) {
		dest[org++] = resolve_aachar(pt->ways[i]->letter);
		dest[org++] = nextfree;
		if(pt->ways[i]->final) {
			dest[nextfree++] = 0x01;
		}
		nextfree = compile_endings_check(dest, nextfree, &pt->ways[i]->more);
	}
	dest[org] = 0x00;
	return nextfree;
}

void chunk_lang(FILE *f, struct program *prg, uint32_t *crc) {
	int i, pad;
	uint32_t size;
	uint8_t endings[1024];
	int endsz, stopsz;
	char stopdata[3 * (NSTOPCHAR + 1)];

	endings[0] = 0x01;
	// the following call may increment n_decodetable
	endsz = compile_endings_check(endings, 1, &prg->endings_root);

	strcpy(stopdata, STOPCHARS);
	stopsz = strlen(stopdata) + 1;
	for(i = 0; STOPCHARS[i]; i++) {
		if(strchr(NO_SPACE_BEFORE, STOPCHARS[i])) {
			stopdata[stopsz++] = STOPCHARS[i];
		}
	}
	stopdata[stopsz++] = 0;
	for(i = 0; STOPCHARS[i]; i++) {
		if(strchr(NO_SPACE_AFTER, STOPCHARS[i])) {
			stopdata[stopsz++] = STOPCHARS[i];
		}
	}
	stopdata[stopsz++] = 0;

	size = (4 * 2) + (n_decodetable * 2) + (1 + ncharmap * 5) + endsz + stopsz;

	pad = chunkheader(f, "LANG", size);
	putword_crc(4 * 2, f, crc);
	putword_crc(4 * 2 + n_decodetable * 2, f, crc);
	putword_crc(4 * 2 + n_decodetable * 2 + 1 + ncharmap * 5, f, crc);
	putword_crc(4 * 2 + n_decodetable * 2 + 1 + ncharmap * 5 + endsz, f, crc);
	for(i = 0; i < n_decodetable; i++) {
		putbyte_crc(decodetable[i][0], f, crc);
		putbyte_crc(decodetable[i][1], f, crc);
	}
	putbyte_crc(ncharmap, f, crc);
	for(i = 0; i < ncharmap; i++) {
		putbyte_crc(charmap[i].tolower, f, crc);
		putbyte_crc(charmap[i].toupper, f, crc);
		putbyte_crc((charmap[i].glyph >> 16) & 0xff, f, crc);
		putbyte_crc((charmap[i].glyph >> 8) & 0xff, f, crc);
		putbyte_crc((charmap[i].glyph >> 0) & 0xff, f, crc);
	}
	for(i = 0; i < endsz; i++) {
		putbyte_crc(endings[i], f, crc);
	}
	for(i = 0; i < stopsz; i++) {
		putbyte_crc(stopdata[i], f, crc);
	}
	if(pad) fputc(0, f);
}

void chunk_writ(FILE *f, uint32_t *crc) {
	int i, j, k, pad, nprefix = 0, dnum;
	uint8_t data[writ_size];
	uint8_t ch;
	uint32_t code, bitaddr;

	code = charbits[0x5f];
	while(code > 1) {
		code >>= 1;
		nprefix++;
	}

	memset(data, 0, writ_size);
	for(i = 0; i < n_textstr; i++) {
		bitaddr = textstrings[i].address << 3;
		for(j = 0; j <= textstrings[i].length; ) {
			dnum = find_dict_prefix(textstrings[i].chars + j);
			if(dnum >= 0) {
				code = charbits[0x5f] ^ (1 << nprefix);
				for(k = 0; k < decode_esc_bits; k++) {
					if((decode_esc_boundary + dnum) & (1 << (decode_esc_bits - 1 - k))) {
						code |= 1 << (nprefix + k);
					}
				}
				code |= 1 << (nprefix + decode_esc_bits);
				j += 1 + strlen((char *) dictionary[dnum].chars);
			} else {
				ch = textstrings[i].chars[j];
				if(ch == 0) {
					code = charbits[0x80];
				} else if(ch >= 0x20 && ch < 0xa0) {
					code = charbits[ch - 0x20];
				} else {
					assert(ch >= 0xa0);
					code = charbits[0x5f] ^ (1 << nprefix);
					for(k = 0; k < decode_esc_bits; k++) {
						if((ch - 0xa0) & (1 << (decode_esc_bits - 1 - k))) {
							code |= 1 << (nprefix + k);
						}
					}
					code |= 1 << (nprefix + decode_esc_bits);
				}
				j++;
			}
			while(code > 1) {
				if(code & 1) {
					data[bitaddr >> 3] |= 0x80 >> (bitaddr & 7);
				}
				code >>= 1;
				bitaddr++;
			}
		}
	}

	pad = chunkheader(f, "WRIT", writ_size);
	for(i = 0; i < writ_size; i++) {
		putbyte_crc(data[i], f, crc);
	}
	if(pad) fputc(0, f);
}

static void chunk_maps(FILE *f, uint32_t *crc) {
	uint32_t size, datatable_start;
	uint16_t wordtable_org[nwordtable];
	int i, j, pad;
	uint16_t w;

	size = 2 + nwordtable * 2;
	for(i = 0; i < nwordtable; i++) {
		wordtable_org[i] = size;
		size += 2 + wordtable[i].length * 2;
	}
	datatable_start = size;
	size += datatable_org;

	pad = chunkheader(f, "MAPS", size);
	putword_crc(nwordtable, f, crc);
	for(i = 0; i < nwordtable; i++) {
		putword_crc(wordtable_org[i], f, crc);
	}
	for(i = 0; i < nwordtable; i++) {
		putword_crc(wordtable[i].length / 2, f, crc);
		for(j = 0; j < wordtable[i].length; j += 2) {
			putword_crc(wordtable[i].words[j + 0], f, crc);
			w = wordtable[i].words[j + 1];
			if(w && w < 0xe000) {
				w = w - 1 + datatable_start;
			}
			putword_crc(w, f, crc);
		}
	}
	for(i = 0; i < ndatatable; i++) {
		for(j = 0; j < datatable[i].length; j++) {
			putbyte_crc(datatable[i].data[j], f, crc);
		}
	}
	if(pad) fputc(0, f);
}

static void chunk_head(FILE *f, struct program *prg) {
	uint32_t size = 12 + 4 + 3 * 2;
	int pad;

	if(prg->meta_ifid) size += 10 + strlen(prg->meta_ifid);
	pad = chunkheader(f, "HEAD", size);
	fputc(AAVM_FORMAT_MAJOR, f);
	fputc(5, f); // minimum required minor version of interpreter
	fputc(2, f);
	fputc(0, f);
	putword(prg->meta_release, f);
	fwrite(prg->meta_serial, 1, 6, f);
	putword(0, f);
	putword(0, f);
	putword(heap_sz, f);
	putword(aux_sz, f);
	putword(fixed_sz + longterm_sz, f);
	if(prg->meta_ifid) {
		fprintf(f, "UUID://%s//", prg->meta_ifid);
		fputc(0, f);
	}
	if(pad) fputc(0, f);
}

static void countmeta(char *str, uint32_t *size, int *n) {
	uint16_t ubuf[2];
	int i;

	if(str) {
		(*size) += 2;
		while((i = utf8_to_unicode(ubuf, 2, (uint8_t *) str))) {
			(*size)++;
			str += i;
		}
		(*n)++;
	}
}

static void putmeta(FILE *f, int tag, char *str) {
	uint16_t ubuf[2];
	int i;
	if(str) {
		fputc(tag, f);
		while((i = utf8_to_unicode(ubuf, 2, (uint8_t *) str))) {
			fputc(resolve_aachar(ubuf[0]), f);
			str += i;
		}
		fputc(0, f);
	}
}

static void chunk_meta(FILE *f, struct program *prg) {
	uint32_t size = 1;
	int pad, n = 0;

	countmeta(prg->meta_title, &size, &n);
	countmeta(prg->meta_author, &size, &n);
	countmeta(prg->meta_noun, &size, &n);
	countmeta(prg->meta_blurb, &size, &n);
	countmeta(prg->meta_reldate, &size, &n);
	countmeta(COMPILERVERSION, &size, &n);

	pad = chunkheader(f, "META", size);
	fputc(n, f);
	putmeta(f, AAMETA_TITLE, prg->meta_title);
	putmeta(f, AAMETA_AUTHOR, prg->meta_author);
	putmeta(f, AAMETA_NOUN, prg->meta_noun);
	putmeta(f, AAMETA_BLURB, prg->meta_blurb);
	putmeta(f, AAMETA_RELDATE, prg->meta_reldate);
	putmeta(f, AAMETA_COMPVER, COMPILERVERSION);
	if(pad) fputc(0, f);
}

static void chunk_urls(FILE *f, struct program *prg) {
	uint32_t size;
	int pad, i, j;
	uint16_t offset[prg->nresource];
	uint32_t addr;

	if(prg->nresource) {
		size = 2 + prg->nresource * 2;
		for(i = 0; i < prg->nresource; i++) {
			offset[i] = size;
			size += 3 + strlen(prg->resources[i].url) + 2;
		}
		pad = chunkheader(f, "URLS", size);
		putword(prg->nresource, f);
		for(i = 0; i < prg->nresource; i++) {
			putword(offset[i], f);
		}
		for(i = 0; i < prg->nresource; i++) {
			assert(altstring[i] < n_textstr);
			addr = textstrings[altstring[i]].address;
			fputc((addr >> 16) & 0xff, f);
			fputc((addr >> 8) & 0xff, f);
			fputc((addr >> 0) & 0xff, f);
			for(j = 0; prg->resources[i].url[j]; j++) {
				fputc(prg->resources[i].url[j], f);
			}
			fputc(0, f);
			fputc(0, f);
		}
		if(pad) fputc(0, f);
	}
}

static void chunks_file(FILE *f, struct program *prg, char *resdir) {
	int pad, i, len;
	FILE *datafile;
	struct stat st;
	uint32_t size;
	uint8_t *buf;
	char *path, *pathbuf = 0;

	for(i = 0; i < prg->nresource; i++) {
		if(prg->resources[i].path) {
			if(resdir
			&& prg->resources[i].path[0] != '/'
			&& prg->resources[i].path[0] != '\\'
			&& !strchr(prg->resources[i].path, ':')) {
				len = strlen(resdir) + 1 + strlen(prg->resources[i].path) + 1;
				pathbuf = malloc(len);
				snprintf(pathbuf, len, "%s/%s", resdir, prg->resources[i].path);
				path = pathbuf;
			} else {
				path = prg->resources[i].path;
			}
			datafile = fopen(path, "rb");
			if(!datafile) {
				report(
					LVL_ERR,
					prg->resources[i].line,
					"Failed to open \"%s\": %s",
					path,
					strerror(errno));
				exit(1);
			}
			if(fstat(fileno(datafile), &st)) {
				report(
					LVL_ERR,
					prg->resources[i].line,
					"Failed to obtain file size for \"%s\": %s",
					path,
					strerror(errno));
				exit(1);
			}
			size = (uint32_t) st.st_size;
			buf = malloc(size);
			if(1 != fread(buf, size, 1, datafile)) {
				report(
					LVL_ERR,
					prg->resources[i].line,
					"Failed to read all of \"%s\": %s",
					path,
					strerror(errno));
				exit(1);
			}
			fclose(datafile);
			pad = chunkheader(f, "FILE", strlen(prg->resources[i].stem) + 1 + size);
			fwrite(prg->resources[i].stem, strlen(prg->resources[i].stem), 1, f);
			fputc(0, f);
			fwrite(buf, size, 1, f);
			if(pad) fputc(0, f);
			free(buf);
			free(pathbuf);
		}
	}
}

static uint32_t opersize(aaoper_t aao) {
	uint32_t addr;

	switch(aao.type) {
	case AAO_NONE:
	case AAO_ZERO:
		return 0;
	case AAO_BYTE:
	case AAO_VBYTE:
		assert(aao.value <= 0xff);
		return 1;
	case AAO_WORD:
		assert(aao.value <= 0xffff);
		return 2;
	case AAO_INDEX:
		assert(aao.value <= 0x3fff);
		return 1 + (aao.value >= 0xc0);
	case AAO_CONST:
		assert(aao.value <= 0x7fff);
		return 2;
	case AAO_REG:
	case AAO_VAR:
	case AAO_STORE_REG:
	case AAO_STORE_VAR:
		assert(aao.value <= 0x3f);
		return 1;
	case AAO_CODE:
		if(aao.value == AAFAIL) {
			return 1;
		} else {
			return 3;
		}
	case AAO_CODE2:
		return 2;
	case AAO_CODE1:
		return 1;
	case AAO_STRING:
		assert(aao.value < n_textstr);
		addr = textstrings[aao.value].address;
		if(addr <= 0xfe && !(addr & 1)) {
			return 1;
		} else if(addr <= 0x3fff) {
			return 2;
		} else {
			assert(addr < 0x3fffff);
			return 3;
		}
	default:
		assert(0); exit(1);
	}
}

static void putoper(aaoper_t aao, FILE *f, uint8_t op, uint32_t *org, struct program *prg, uint32_t *crc) {
	uint32_t addr;
	int32_t diff;

	switch(aao.type) {
	case AAO_NONE:
	case AAO_ZERO:
		break;
	case AAO_BYTE:
	case AAO_VBYTE:
		putbyte_crc(aao.value, f, crc);
		(*org)++;
		break;
	case AAO_WORD:
		putword_crc(aao.value, f, crc);
		(*org) += 2;
		break;
	case AAO_INDEX:
		if(aao.value < 0xc0) {
			putbyte_crc(aao.value, f, crc);
			(*org)++;
		} else {
			putbyte_crc(0xc0 | (aao.value >> 8), f, crc);
			putbyte_crc(aao.value & 0xff, f, crc);
			(*org) += 2;
		}
		break;
	case AAO_CONST:
		putword_crc(aao.value, f, crc);
		(*org) += 2;
		break;
	case AAO_REG:
		putbyte_crc(0x80 | aao.value, f, crc);
		(*org)++;
		break;
	case AAO_VAR:
		putbyte_crc(0xc0 | aao.value, f, crc);
		(*org)++;
		break;
	case AAO_STORE_REG:
		putbyte_crc(0x00 | aao.value, f, crc);
		(*org)++;
		break;
	case AAO_STORE_VAR:
		putbyte_crc(0x40 | aao.value, f, crc);
		(*org)++;
		break;
	case AAO_CODE:
		if(aao.value == AAFAIL) {
			putbyte_crc(0, f, crc);
			(*org)++;
		} else {
			assert(aao.value < nextlabel);
			addr = symbols[aao.value];
			if(addr >= code_sz) {
				printf("%x %02x %d\n", addr, op, aao.value);
				if(aao.value < prg->npredicate) {
					printf("%s\n", prg->predicates[aao.value]->printed_name);
				}
				assert(0); exit(1);
			}
			putbyte_crc(0x80 | (addr >> 16), f, crc);
			putbyte_crc((addr >> 8) & 0xff, f, crc);
			putbyte_crc(addr & 0xff, f, crc);
			(*org) += 3;
		}
		break;
	case AAO_CODE2:
		assert(aao.value < nextlabel);
		diff = symbols[aao.value] - *org - 2;
		assert(diff >= -0x2000 && diff <= 0x1fff);
		putbyte_crc(0x40 | ((diff >> 8) & 0x3f), f, crc);
		putbyte_crc(diff & 0xff, f, crc);
		(*org) += 2;
		break;
	case AAO_CODE1:
		assert(aao.value < nextlabel);
		diff = symbols[aao.value] - *org - 1;
		assert(diff >= 0x01 && diff <= 0x3f);
		putbyte_crc(diff, f, crc);
		(*org)++;
		break;
	case AAO_STRING:
		assert(aao.value < n_textstr);
		addr = textstrings[aao.value].address;
		if(addr <= 0xfe && !(addr & 1)) {
			putbyte_crc(addr >> 1, f, crc);
			(*org)++;
		} else if(addr <= 0x3fff) {
			putbyte_crc(0x80 | (addr >> 8), f, crc);
			putbyte_crc(addr & 0xff, f, crc);
			(*org) += 2;
		} else {
			putbyte_crc(0xc0 | (addr >> 16), f, crc);
			putbyte_crc((addr >> 8) & 0xff, f, crc);
			putbyte_crc(addr & 0xff, f, crc);
			(*org) += 3;
		}
		break;
	default:
		assert(0); exit(1);
	}
}

static void analyze_code() {
	int i, j, k, flag;
	struct aainstr *ai;
	uint32_t org;
	uint8_t actual, expected;

	symbols = arena_alloc(&aa_arena, nextlabel * sizeof(uint32_t));
	memset(symbols, 0xff, nextlabel * sizeof(uint32_t));
	do {
		flag = 0;
		org = 0;
		for(i = 0; i < ninstr; i++) {
			ai = &aainstr[i];
			if(ai->op == AA_LABEL) {
				assert(ai->oper[0].type == AAO_LABEL);
				symbols[ai->oper[0].value] = org;
			} else if(ai->op != AA_SKIP) {
				org++;
				for(j = 0; j < 4; j++) {
					actual = ai->oper[j].type;
					expected = aaopinfo[ai->op].oper[j];
					if(actual != expected
					&& !(expected == AAO_VALUE &&
						(actual == AAO_CONST ||
						actual == AAO_REG ||
						actual == AAO_VAR))
					&& !(expected == AAO_DEST &&
						(actual == AAO_STORE_REG ||
						actual == AAO_STORE_VAR ||
						actual == AAO_REG ||
						actual == AAO_VAR))
					&& !(expected == AAO_CODE &&
						(actual == AAO_CODE2 ||
						actual == AAO_CODE1))
					&& !(ai->op == (AA_CHECK_EQ | 0x80) && actual == AAO_BYTE))
					{
						printf("%02x", ai->op);
						for(j = 0; j < 4; j++) {
							printf(" %02x:%06x (%02x)",
								ai->oper[j].type,
								ai->oper[j].value,
								aaopinfo[ai->op].oper[j]);
						}
						printf("\n");
						assert(0); exit(1);
					}
					if(ai->oper[j].type == AAO_CODE && ai->oper[j].value != AAFAIL) {
						if(symbols[ai->oper[j].value] == 0xffffffff) {
							flag = 1;
						} else {
							if((int32_t) (symbols[ai->oper[j].value] - org - 3) <= 0x1fff
							&& (int32_t) (symbols[ai->oper[j].value] - org - 3) >= -0x2000) {
								ai->oper[j].type = AAO_CODE2;
								for(k = 0; k < nextlabel; k++) {
									if(symbols[k] != 0xffffffff
									&& symbols[k] > org) {
										symbols[k]--;
									}
								}
								flag = 1;
							}
						}
					}
					if(ai->oper[j].type == AAO_CODE2) {
						if((int32_t) (symbols[ai->oper[j].value] - org - 2) <= 0x3f
						&& (int32_t) (symbols[ai->oper[j].value] - org - 2) >= 0x01) {
							ai->oper[j].type = AAO_CODE1;
							for(k = 0; k < nextlabel; k++) {
								if(symbols[k] != 0xffffffff
								&& symbols[k] > org) {
									symbols[k]--;
								}
							}
							flag = 1;
						}
					}
					org += opersize(ai->oper[j]);
				}
			}
		}
	} while(flag);
	code_sz = org;
}

static void chunk_code(FILE *f, struct program *prg, uint32_t *crc) {
	int pad;
	int i, j;
	struct aainstr *ai;
	uint32_t org = 0;

	pad = chunkheader(f, "CODE", code_sz);
	for(i = 0; i < ninstr; i++) {
		ai = &aainstr[i];
		if(ai->op != AA_LABEL && ai->op != AA_SKIP) {
			putbyte_crc(ai->op, f, crc);
			org++;
			for(j = 0; j < 4; j++) {
				putoper(ai->oper[j], f, ai->op, &org, prg, crc);
			}
		}
	}
	if(pad) fputc(0, f);
}

static void chunk_look(FILE *f, struct program *prg, uint32_t *crc) {
	int i, j, pad;
	uint32_t org;
	struct boxclassline *bcl;
	uint16_t offset[prg->nboxclass];

	org = 2 + prg->nboxclass * 2;
	for(i = 0; i < prg->nboxclass; i++) {
		offset[i] = org;
		for(bcl = prg->boxclasses[i].css_lines; bcl; bcl = bcl->next) {
			org += strlen(bcl->data) + 1;
		}
		org++;
	}

	pad = chunkheader(f, "LOOK", org);

	putword_crc(prg->nboxclass, f, crc);
	for(i = 0; i < prg->nboxclass; i++) {
		putword_crc(offset[i], f, crc);
	}
	for(i = 0; i < prg->nboxclass; i++) {
		for(bcl = prg->boxclasses[i].css_lines; bcl; bcl = bcl->next) {
			for(j = 0; bcl->data[j]; j++) {
				if(bcl->data[j] & 0x80) {
					report(LVL_ERR, 0, "Extended characters not supported in style definitions.");
					exit(1);
				}
				putbyte_crc(bcl->data[j], f, crc);
			}
			putbyte_crc(0, f, crc);
		}
		putbyte_crc(0, f, crc);
	}

	if(pad) fputc(0, f);
}

void backend_aa(
	char *filename,
	char *format,
	char *coverfname,
	char *coveralt,
	int heapsize,
	int auxsize,
	int ltssize,
	int strip,
	struct program *prg,
	struct arena *arena,
	char *resdir)
{
	FILE *f;
	int i;
	uint32_t crc;

	aavm_init();

	arena_init(&aa_arena, 1024);

	tracelabels[TR_ENTER] = "ENTER ";
	tracelabels[TR_QUERY] = "QUERY ";
	tracelabels[TR_MQUERY] = "QUERY *";
	//tracelabels[TR_QDONE] = "FOUND ";

	dynlink_id = malloc(prg->nobjflag * sizeof(uint16_t));
	for(i = 0; i < prg->nobjflag; i++) {
		if(prg->objflagpred[i]->pred->flags & PREDF_DYN_LINKAGE) {
			dynlink_id[i] = n_dynlink++;
			prg->objflagpred[i]->pred->flags |= PREDF_INVOKED_SIMPLE | PREDF_INVOKED_MULTI;
		} else {
			dynlink_id[i] = ~0;
		}
	}

	initial_sel = calloc(prg->nselect, 1);

	first_sel_byte = (prg->nglobalflag + 7) / 8;
	first_gvar = (first_sel_byte + prg->nselect + 1) / 2;
	first_ovar = 3 + (prg->nobjflag + 15) / 16;
	n_glb_word = first_gvar + prg->nglobalvar + n_dynlink;
	n_obj_word = first_ovar + (prg->nobjvar - 1) + n_dynlink;

	heap_sz = heapsize;
	aux_sz = auxsize;
	fixed_sz = 1 + prg->nworldobj + n_glb_word + prg->nworldobj * n_obj_word;
	longterm_sz = ltssize;

	compile_program(prg);
	analyze_resources(prg);
	analyze_chars();
	analyze_strings();
	analyze_code();

	f = fopen(filename, "wb");
	if(!f) {
		report(LVL_ERR, 0, "Error opening \"%s\" for output: %s", filename, strerror(errno));
		exit(1);
	}

	fprintf(f, "FORMxxxxAAVM");
	aatotalsize = 4;

	crc = 0xffffffff;

	chunk_head(f, prg);
	chunk_meta(f, prg);
	chunk_look(f, prg, &crc);

	if(!strip) {
		chunk_tags(f, prg);
	}

	chunk_lang(f, prg, &crc);
	chunk_maps(f, &crc);
	chunk_dict(f, &crc);

	chunk_init(f, prg, &crc);

	chunk_code(f, prg, &crc);
	chunk_writ(f, &crc);

	chunk_urls(f, prg);
	chunks_file(f, prg, resdir);

	crc ^= 0xffffffff;

	fseek(f, 4, SEEK_SET);
	fputc((aatotalsize >> 24) & 0xff, f);
	fputc((aatotalsize >> 16) & 0xff, f);
	fputc((aatotalsize >> 8) & 0xff, f);
	fputc((aatotalsize >> 0) & 0xff, f);

	fseek(f, 0x20, SEEK_SET);
	fputc((crc >> 24) & 0xff, f);
	fputc((crc >> 16) & 0xff, f);
	fputc((crc >> 8) & 0xff, f);
	fputc((crc >> 0) & 0xff, f);

	fclose(f);

	arena_free(&aa_arena);

	if(!(prg->optflags & OPTF_NO_TRACE)) {
		report(LVL_NOTE, 0, "In this build, the code has been instrumented to allow tracing.");
	}
}

int aa_get_max_temp() {
	return AA_MAX_TEMP;
}
