#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rules.h"
#include "report.h"

#define MAXWORDLENGTH 256
#define MAXRULEWORDS 32
#define MAXNESTEDEXPR 32
#define BUCKETS 1024

struct evalvar {
	struct word	*name;
	struct astnode	**pointer;
	struct evalvar	*next;
};

struct tracevar {
	struct tracevar	*next;
	struct word	*name;
};

static struct word *wordhash[BUCKETS];

struct predicate **predicates;
int npredicate;

static line_t line = 0;
static int column = 0, wordcount;

int verbose = 0;

static struct token tok;
static struct word *starword = 0;
static int status;

char **sourcefile;
int nsourcefile;

struct worldobj **worldobjs;
int nworldobj;

static int macro_instance;

static FILE *f;

static struct predicate *failpred, *stoppred;

static struct predicate **tracequeue;
static int tracequeue_r, tracequeue_w;

void pp_predname(struct predicate *pred);

struct builtinspec {
	int id;
	int predflags;
	int nword;
	char *word[8];
} builtinspec[] = {
	{BI_LESSTHAN,		0,				3,	{0, "<", 0}},
	{BI_GREATERTHAN,	0,				3,	{0, ">", 0}},
	{BI_PLUS,		0,				5,	{0, "plus", 0, "into", 0}},
	{BI_MINUS,		0,				5,	{0, "minus", 0, "into", 0}},
	{BI_TIMES,		0,				5,	{0, "times", 0, "into", 0}},
	{BI_DIVIDED,		0,				6,	{0, "divided", "by", 0, "into", 0}},
	{BI_MODULO,		0,				5,	{0, "modulo", 0, "into", 0}},
	{BI_RANDOM,		0,				7,	{"random", "from", 0, "to", 0, "into", 0}},
	{BI_FAIL,		PREDF_FAIL,			1,	{"fail"}},
	{BI_STOP,		PREDF_SUCCEEDS,			1,	{"stop"}},
	{BI_REPEAT,		PREDF_SUCCEEDS,			2,	{"repeat", "forever"}},
	{BI_NUMBER,		0,				2,	{"number", 0}},
	{BI_LIST,		0,				2,	{"list", 0}},
	{BI_EMPTY,		0,				2,	{"empty", 0}},
	{BI_NONEMPTY,		0,				2,	{"nonempty", 0}},
	{BI_WORD,		0,				2,	{"word", 0}},
	{BI_OBJECT,		0,				2,	{"object", 0}},
	{BI_UNBOUND,		0,				2,	{"unbound", 0}},
	{BI_QUIT,		PREDF_SUCCEEDS,			1,	{"quit"}},
	{BI_RESTART,		PREDF_SUCCEEDS,			1,	{"restart"}},
	{BI_SAVE,		0,				2,	{"save", 0}},
	{BI_RESTORE,		PREDF_SUCCEEDS,			1,	{"restore"}},
	{BI_SAVE_UNDO,		0,				3,	{"save", "undo", 0}},
	{BI_UNDO,		0,				1,	{"undo"}},
	{BI_SCRIPT_ON,		0,				2,	{"transcript", "on"}},
	{BI_SCRIPT_OFF,		PREDF_SUCCEEDS,			2,	{"transcript", "off"}},
	{BI_TRACE_ON,		PREDF_SUCCEEDS,			2,	{"trace", "on"}},
	{BI_TRACE_OFF,		PREDF_SUCCEEDS,			2,	{"trace", "off"}},
	{BI_NOSPACE,		PREDF_SUCCEEDS|PREDF_OUTPUT,	2,	{"no", "space"}},
	{BI_SPACE,		PREDF_SUCCEEDS|PREDF_OUTPUT,	1,	{"space"}},
	{BI_SPACE_N,		PREDF_SUCCEEDS|PREDF_OUTPUT,	2,	{"space", 0}},
	{BI_LINE,		PREDF_SUCCEEDS|PREDF_OUTPUT,	1,	{"line"}},
	{BI_PAR,		PREDF_SUCCEEDS|PREDF_OUTPUT,	1,	{"par"}},
	{BI_PAR_N,		PREDF_SUCCEEDS|PREDF_OUTPUT,	2,	{"par", 0}},
	{BI_ROMAN,		PREDF_SUCCEEDS,			1,	{"roman"}},
	{BI_BOLD,		PREDF_SUCCEEDS,			1,	{"bold"}},
	{BI_ITALIC,		PREDF_SUCCEEDS,			1,	{"italic"}},
	{BI_REVERSE,		PREDF_SUCCEEDS,			1,	{"reverse"}},
	{BI_FIXED,		PREDF_SUCCEEDS,			2,	{"fixed", "pitch"}},
	{BI_UPPER,		PREDF_SUCCEEDS,			1,	{"uppercase"}},
	{BI_CLEAR,		PREDF_SUCCEEDS,			1,	{"clear"}},
	{BI_CLEAR_ALL,		PREDF_SUCCEEDS,			2,	{"clear", "all"}},
	{BI_WINDOWWIDTH,	0,				4,	{"status", "bar", "width", 0}},
	{BI_CURSORTO,		PREDF_SUCCEEDS,			6,	{"cursor", "to", "row", 0, "column", 0}},
	{BI_GETINPUT,		0,				3,	{"get", "input", 0}},
	{BI_GETRAWINPUT,	0,				4,	{"get", "raw", "input", 0}},
	{BI_GETKEY,		0,				3,	{"get", "key", 0}},
	{BI_SERIALNUMBER,	PREDF_SUCCEEDS,			2,	{"serial", "number"}},
	{BI_COMPILERVERSION,	PREDF_SUCCEEDS,			2,	{"compiler", "version"}},
	//{BI_MEMINFO,		PREDF_SUCCEEDS,			3,	{"display", "memory", "info"}},
	{BI_MEMSTATS,		PREDF_SUCCEEDS,			3,	{"display", "memory", "statistics"}},
	{BI_HASPARENT,		PREDF_DYNAMIC,			4,	{0, "has", "parent", 0}},
	{BI_UNIFY,		0,				3,	{0, "=", 0}},
	{BI_IS_ONE_OF,		0,				5,	{0, "is", "one", "of", 0}},
	{BI_SPLIT,		0,				8,	{"split", 0, "by", 0, "into", 0, "and", 0}},
	{BI_WORDREP_RETURN,	0,				4,	{"word", "representing", "return", 0}},
	{BI_WORDREP_SPACE,	0,				4,	{"word", "representing", "space", 0}},
	{BI_WORDREP_BACKSPACE,	0,				4,	{"word", "representing", "backspace", 0}},
	{BI_WORDREP_UP,		0,				4,	{"word", "representing", "up", 0}},
	{BI_WORDREP_DOWN,	0,				4,	{"word", "representing", "down", 0}},
	{BI_WORDREP_LEFT,	0,				4,	{"word", "representing", "left", 0}},
	{BI_WORDREP_RIGHT,	0,				4,	{"word", "representing", "right", 0}},
	{BI_HAVE_UNDO,		0,				3,	{"interpreter", "supports", "undo"}},
	{BI_CONSTRUCTORS,	PREDF_DEFINABLE_BI,		1,	{""}}, // cannot appear in source code
	{BI_PROGRAM_ENTRY,	PREDF_DEFINABLE_BI,		3,	{"program", "entry", "point"}},
	{BI_ERROR_ENTRY,	PREDF_DEFINABLE_BI,		4,	{"error", 0, "entry", "point"}},
	{BI_STORY_IFID,		PREDF_DEFINABLE_BI,		2,	{"story", "ifid"}},
	{BI_STORY_TITLE,	PREDF_DEFINABLE_BI,		2,	{"story", "title"}},
	{BI_STORY_AUTHOR,	PREDF_DEFINABLE_BI,		2,	{"story", "author"}},
	{BI_STORY_NOUN,		PREDF_DEFINABLE_BI,		2,	{"story", "noun"}},
	{BI_STORY_BLURB,	PREDF_DEFINABLE_BI,		2,	{"story", "blurb"}},
	{BI_STORY_RELEASE,	PREDF_DEFINABLE_BI,		3,	{"story", "release", 0}},
	{BI_ENDINGS,		PREDF_DEFINABLE_BI,		3,	{"removable", "word", "endings"}},
};

int hashfunc(char *name) {
	int i;
	unsigned int h = 0;

	for(i = 0; name[i]; i++) {
		h += (unsigned) name[i] * (1 + i);
	}
	h %= BUCKETS;
	return h;
}

struct word *find_word(char *name) {
	int h = hashfunc(name);
	struct word *w;

	for(w = wordhash[h]; w; w = w->next_in_hash) {
		if(!strcmp(w->name, name)) return w;
	}
	w = calloc(1, sizeof(*w));
	w->name = strdup(name);
	w->next_in_hash = wordhash[h];
	wordhash[h] = w;

	return w;
}

struct word *fresh_word() {
	char buf[16];
	static int counter = 1;

	snprintf(buf, sizeof(buf), "*%d", counter++);
	return find_word(buf);
}

void create_worldobj(struct word *w) {
	if(!(w->flags & WORDF_TAG)) {
		worldobjs = realloc(worldobjs, (nworldobj + 1) * sizeof(struct worldobj *));
		worldobjs[nworldobj] = calloc(1, sizeof(struct worldobj));
		worldobjs[nworldobj]->astnode = mkast(AN_TAG);
		worldobjs[nworldobj]->astnode->word = w;
		w->flags |= WORDF_TAG;
		w->obj_id = nworldobj;
		nworldobj++;
	}
}

struct predicate *find_predicate(int nword, struct word **words) {
	int i, j, len;
	struct predicate *pred;

	assert(nword);

	for(i = 0; i < npredicate; i++) {
		if(predicates[i]->nword == nword) {
			for(j = 0; j < nword; j++) {
				if(predicates[i]->words[j] != words[j]) break;
			}
			if(j == nword) {
				return predicates[i];
			}
		}
	}

	pred = calloc(1, sizeof(*pred));
	pred->nword = nword;
	pred->words = malloc(nword * sizeof(struct word *));
	len = 2;
	for(j = 0; j < nword; j++) {
		if(j) len++;
		pred->words[j] = words[j];
		if(words[j]) {
			len += strlen(words[j]->name);
		} else {
			pred->arity++;
			len++;
		}
	}
	pred->printed_name = malloc(len + 1);
	i = 0;
	pred->printed_name[i++] = '(';
	for(j = 0; j < nword; j++) {
		if(j) pred->printed_name[i++] = ' ';
		if(words[j]) {
			strcpy(pred->printed_name + i, words[j]->name);
			i += strlen(words[j]->name);
		} else {
			pred->printed_name[i++] = '$';
		}
	}
	pred->printed_name[i++] = ')';
	pred->printed_name[i] = 0;
	assert(i == len);
	pred->unbound_in_due_to = calloc(pred->arity, sizeof(struct clause *));

	predicates = realloc(predicates, (npredicate + 1) * sizeof(pred));
	predicates[npredicate++] = pred;
	return pred;
}

struct predicate *find_builtin(int id) {
	int i;

	for(i = 0; i < npredicate; i++) {
		if(predicates[i]->builtin == id) {
			return predicates[i];
		}
	}
	assert(0);
	exit(1);
}

struct astnode *mkast(int kind) {
	struct astnode *an = calloc(1, sizeof(*an));
	an->kind = kind;
	an->line = line;
	return an;
}

int valid_varname_char(uint8_t ch) {
	return
		(ch >= 'a' && ch <= 'z')
		|| (ch >= 'A' && ch <= 'Z')
		|| (ch >= '0' && ch <= '9')
		|| (ch >= 128 && ch <= 255)
		|| ch == '_'
		|| ch == '-';
}

enum {
	PMODE_BODY,
	PMODE_RULE,
	PMODE_VALUE
};

int next_token(struct token *t, FILE *f, int parsemode) {
	int ch;
	char buf[MAXWORDLENGTH + 1];
	int pos = 0, i;
	int at_start;

	for(;;) {
		ch = fgetc(f);
		column++;

		if(ch == EOF) {
			return 0;
		} else if(ch == '\n') {
			column = 0;
			line++;
		} else if(ch == ' ' || ch == '\t' || ch == '\r') {
		} else if(ch == '$' || ch == '#' || ch == '@') {
			if(ch == '$') {
				t->kind = TOK_VARIABLE;
			} else if(ch == '#') {
				t->kind = TOK_TAG;
			} else {
				t->kind = TOK_DICTWORD;
			}
			at_start = (column == 1);
			for(;;) {
				ch = fgetc(f);
				column++;
				if(valid_varname_char((uint8_t) ch)
				|| (t->kind == TOK_DICTWORD && !strchr("\n\r\t $#[|](){}@~*%", ch))) {
					if(ch == '\\') {
						ch = fgetc(f);
						column++;
						if(ch == EOF) {
							report(LVL_ERR, line, "Backslash not allowed at end of file.");
							exit(1);
						}
					}
					if(pos >= MAXWORDLENGTH) {
						report(LVL_ERR, line, "Name too long.");
						exit(1);
					}
					buf[pos++] = ch;
				} else {
					if(ch != EOF) {
						ungetc(ch, f);
						column--;
					}
					buf[pos] = 0;
					if(t->kind == TOK_TAG && !pos) {
						report(LVL_ERR, line, "Invalid tag.");
						exit(1);
					}
					if(t->kind == TOK_DICTWORD && !pos && !at_start) {
						report(LVL_ERR, line, "Invalid dictionary word.");
						exit(1);
					}
					t->word = find_word(buf);
					if(t->kind == TOK_TAG) {
						create_worldobj(t->word);
					}
					return 1 + at_start;
				}
			}
		} else if(strchr("[|](){}~", ch)) {
			at_start = (column == 1);
			t->kind = ch;
			return 1 + at_start;
		} else if(ch == '/') {
			at_start = (column == 1);
			if(parsemode == PMODE_BODY) {
				buf[pos++] = '/';
				buf[pos] = 0;
				t->kind = TOK_BAREWORD;
				t->word = find_word(buf);
			} else {
				t->kind = ch;
			}
			return 1 + at_start;
		} else if(ch == '*') {
			at_start = (column == 1);
			ch = fgetc(f);
			if(ch == '(') {
				column++;
				t->kind = TOK_STARPAREN;
			} else {
				if(ch != EOF) {
					ungetc(ch, f);
					column--;
				}
				t->kind = '*';
			}
			return 1 + at_start;
		} else if(ch == '%') {
			at_start = (column == 1);
			ch = fgetc(f);
			column++;
			if(ch == '%') {
				do {
					ch = fgetc(f);
					if(ch == EOF) return 0;
				} while(ch != '\n');
				column = 0;
				line++;
			} else {
				if(ch != EOF) {
					ungetc(ch, f);
					column--;
				}
				buf[pos++] = '%';
				buf[pos] = 0;
				t->kind = TOK_BAREWORD;
				t->word = find_word(buf);
				return 1 + at_start;
			}
		} else {
			at_start = (column == 1);
			if(ch == '\\') {
				ch = fgetc(f);
				column++;
				if(ch == EOF) {
					report(LVL_ERR, line, "Backslash not allowed at end of file.");
					exit(1);
				}
			}
			buf[pos++] = ch;
			for(;;) {
				ch = fgetc(f);
				column++;
				if(ch == '\\') {
					ch = fgetc(f);
					if(ch == '\n') {
						line++;
						column = 0;
					} else {
						column++;
					}
					if(ch == EOF) {
						report(LVL_ERR, line, "Backslash not allowed at end of file.");
						exit(1);
					}
					if(pos >= MAXWORDLENGTH) {
						report(LVL_ERR, line, "Word too long.");
						exit(1);
					}
					buf[pos++] = ch;
				} else if(ch == EOF || strchr("\n\r\t $#[|](){}@~*%/", ch)) {
					if(ch != EOF) {
						ungetc(ch, f);
						column--;
					}
					buf[pos] = 0;
					if(parsemode != PMODE_BODY) {
						for(i = 0; i < pos; i++) {
							if(buf[i] < '0' || buf[i] > '9') break;
						}
						if(i == pos && (pos == 1 || buf[0] != '0')) {
							t->kind = TOK_INTEGER;
							t->value = strtol(buf, 0, 10);
							if(t->value < 0 || t->value > 0x3fff) {
								report(LVL_ERR, line, "Integer out of range (%d)", t->value);
								exit(1);
							}
						} else {
							t->kind = TOK_BAREWORD;
							t->word = find_word(buf);
						}
					} else {
						t->kind = TOK_BAREWORD;
						t->word = find_word(buf);
					}
					return 1 + at_start;
				} else {
					if(pos >= MAXWORDLENGTH) {
						report(LVL_ERR, line, "Word too long.");
						exit(1);
					}
					buf[pos++] = ch;
				}
			}
		}
	}
}

int look_ahead_for_slash(FILE *f) {
	int ch;
	int found;

	do {
		ch = fgetc(f);
		column++;
	} while(ch == ' ' || ch == '\t');

	found = (ch == '/');

	if(ch != EOF) {
		ungetc(ch, f);
		column--;
	}

	return found;
}

struct astnode *parse_expr(int parsemode);
struct astnode *parse_expr_nested(struct astnode **nested_rules, int *nnested, int parsemode, int *nonvar_detected);

struct astnode *fold_disjunctions(struct astnode *body) {
	struct astnode *an, **prev, *or;

	for(prev = &body; *prev; prev = &an->next_in_body) {
		an = *prev;
		if(an->kind == AN_RULE
		&& an->predicate->special == SP_OR) {
			*prev = 0;
			or = mkast(AN_OR);
			or->line = an->line;
			or->nchild = 2;
			or->children = malloc(2 * sizeof(struct astnode *));
			or->children[0] = body;
			or->children[1] = fold_disjunctions(an->next_in_body);
			return or;
		}
	}

	return body;
}

struct astnode *parse_rule_head(struct astnode **nested_rules, int *nnested, int *nested_nonvar) {
	int n = 0, i, j, nparam = 0;
	struct astnode *nodes[MAXRULEWORDS], *an;
	struct word *words[MAXRULEWORDS];

	for(;;) {
		status = next_token(&tok, f, PMODE_RULE);
		if(!status) {
			report(LVL_ERR, line, "Unterminated rule expression.");
			exit(1);
		}
		if(status == 2) {
			report(LVL_ERR, line, "Unexpected token at beginning of line.");
			exit(1);
		}
		if(tok.kind == ')') break;

		if(n >= MAXRULEWORDS) {
			report(LVL_ERR, line, "Too many words in rule name.");
			exit(1);
		}
		nodes[n++] = parse_expr_nested(nested_rules, nnested, PMODE_RULE, nested_nonvar);
	}

	for(i = 0; i < n; i++) {
		if(nodes[i]->kind != AN_BAREWORD) {
			nparam++;
		}
	}
	an = mkast(AN_RULE);
	an->nchild = nparam;
	an->children = malloc(nparam * sizeof(struct astnode *));
	j = 0;
	for(i = 0; i < n; i++) {
		if(nodes[i]->kind == AN_BAREWORD) {
			words[i] = nodes[i]->word;
		} else {
			words[i] = 0;
			assert(j < nparam);
			an->children[j++] = nodes[i];
		}
	}
	assert(j == nparam);
	an->predicate = find_predicate(n, words);
	assert(an->predicate->arity == nparam);

	return an;
}

struct astnode *parse_rule() {
	int n = 0, i, j, nparam = 0;
	struct astnode *nodes[MAXRULEWORDS], *an;
	struct word *words[MAXRULEWORDS];

	for(;;) {
		status = next_token(&tok, f, PMODE_RULE);
		if(!status) {
			report(LVL_ERR, line, "Unterminated rule expression.");
			exit(1);
		}
		if(status == 2) {
			report(LVL_ERR, line, "Unexpected token at beginning of line.");
			exit(1);
		}
		if(tok.kind == ')') break;

		if(n >= MAXRULEWORDS) {
			report(LVL_ERR, line, "Too many words in rule name.");
			exit(1);
		}
		nodes[n++] = parse_expr(PMODE_RULE);
	}

	for(i = 0; i < n; i++) {
		if(nodes[i]->kind != AN_BAREWORD) {
			nparam++;
		}
	}
	an = mkast(AN_RULE);
	an->nchild = nparam;
	an->children = malloc(nparam * sizeof(struct astnode *));
	j = 0;
	for(i = 0; i < n; i++) {
		if(nodes[i]->kind == AN_BAREWORD) {
			words[i] = nodes[i]->word;
		} else {
			words[i] = 0;
			assert(j < nparam);
			an->children[j++] = nodes[i];
		}
	}
	assert(j == nparam);
	an->predicate = find_predicate(n, words);
	assert(an->predicate->arity == nparam);

	return an;
}

void analyse_dynamic(struct astnode *an, int pos) {
	struct predicate *pred = an->predicate;
	struct dynamic *dyn = pred->dynamic;

	if(pred->arity) {
		if(an->children[0]->kind == AN_TAG) {
			if(!(dyn->first_param_use & (FPUSE_ONE_TAG | FPUSE_MANY_TAGS | FPUSE_OTHER))) {
				dyn->first_param_use |= FPUSE_ONE_TAG;
				dyn->first_param_value = an->children[0];
			} else if(dyn->first_param_use & FPUSE_ONE_TAG) {
				if(dyn->first_param_value->word != an->children[0]->word) {
					dyn->first_param_use &= ~FPUSE_ONE_TAG;
					dyn->first_param_use |= FPUSE_MANY_TAGS;
				}
			}
		} else {
			dyn->first_param_use &= ~(FPUSE_ONE_TAG | FPUSE_MANY_TAGS);
			dyn->first_param_use |= FPUSE_OTHER;
		}
	}
}

struct astnode *deepcopy_astnode(struct astnode *an, line_t line) {
	struct astnode *a;
	int i;

	if(!an) return 0;

	a = malloc(sizeof(*a));
	memcpy(a, an, sizeof(*a));
	if(line) a->line = line;
	for(i = 0; i < a->nchild; i++) {
		a->children[i] = deepcopy_astnode(an->children[i], line);
	}
	a->next_in_body = deepcopy_astnode(an->next_in_body, line);

	return a;
}

static void trace_enqueue(struct predicate *pred) {
	if(!(pred->flags & PREDF_IN_QUEUE)) {
		pred->flags |= PREDF_IN_QUEUE;
		tracequeue[tracequeue_w++] = pred;
		if(tracequeue_w == npredicate) tracequeue_w = 0;
	}
}

static struct predicate *trace_read_queue() {
	struct predicate *pred = 0;

	if(tracequeue_r != tracequeue_w) {
		pred = tracequeue[tracequeue_r++];
		if(tracequeue_r == npredicate) tracequeue_r = 0;
		pred->flags &= ~PREDF_IN_QUEUE;
	}

	return pred;
}

void add_bound_vars(struct astnode *an, struct tracevar **vars) {
	struct tracevar *v;
	int i;

	while(an) {
		if(an->kind == AN_VARIABLE) {
			if(an->word->name[0]) {
				for(v = *vars; v; v = v->next) {
					if(v->name == an->word) break;
				}
				if(!v) {
					v = malloc(sizeof(*v));
					v->name = an->word;
					v->next = *vars;
					*vars = v;
				}
			}
		} else {
			for(i = 0; i < an->nchild; i++) {
				add_bound_vars(an->children[i], vars);
			}
		}
		an = an->next_in_body;
	}
}

int any_unbound(struct astnode *an, struct tracevar *vars) {
	struct tracevar *v;
	int i;

	while(an) {
		if(an->kind == AN_VARIABLE) {
			for(v = vars; v; v = v->next) {
				if(v->name == an->word) break;
			}
			if(!v) {
				return 1;
			}
		} else {
			for(i = 0; i < an->nchild; i++) {
				if(any_unbound(an->children[i], vars)) {
					return 1;
				}
			}
		}
		an = an->next_in_body;
	}

	return 0;
}

void trace_add_caller(struct predicate *callee, struct predicate *caller) {
	struct predlist *pl;

	for(pl = callee->callers; pl; pl = pl->next) {
		if(pl->pred == caller) return;
	}

	pl = malloc(sizeof(*pl));
	pl->pred = caller;
	pl->next = callee->callers;
	callee->callers = pl;
}

void join_subvars(struct tracevar **dest, struct tracevar *second, struct tracevar *tail) {
	struct tracevar *first = *dest;
	struct tracevar *result = tail;
	struct tracevar *v, *newv;

	while(second != tail) {
		for(v = first; v != tail; v = v->next) {
			if(v->name == second->name) break;
		}
		if(v != tail) {
			newv = malloc(sizeof(*newv));
			newv->name = v->name;
			newv->next = result;
			result = newv; 
		}
		v = second;
		second = second->next;
		free(v);
	}

	while(first != tail) {
		v = first;
		first = first->next;
		free(v);
	}

	*dest = result;
}

void trace_invoke_pred(struct predicate *pred, int flags, uint32_t unbound_in, struct clause *caller);

void trace_now_expression(struct astnode *an, struct tracevar *vars) {
	int i;

	if(an->kind == AN_RULE || an->kind == AN_NEG_RULE) {
		for(i = 0; i < an->predicate->arity; i++) {
			if(any_unbound(an->children[i], vars)) {
				an->children[i]->unbound = 1;
				if(an->kind == AN_RULE
				&& (!(an->predicate->flags & PREDF_GLOBAL_VAR) || an->predicate->dynamic->global_bufsize == 1)) {
					report(
						LVL_WARN,
						an->line,
						"Argument %d of now-expression can be unbound, leading to runtime errors.",
						i + 1);
				}
			}
		}
		if(an->predicate->arity == 1 && !(an->predicate->flags & PREDF_GLOBAL_VAR)) {
			if(an->kind == AN_RULE) {
				an->predicate->dynamic->linkage_flags |= LINKF_SET;
			} else {
				if(an->children[0]->kind != AN_VARIABLE
				|| an->children[0]->word->name[0]) {
					an->predicate->dynamic->linkage_flags |= LINKF_RESET;
				}
				if(an->children[0]->unbound) {
					an->predicate->dynamic->linkage_flags |= LINKF_CLEAR;
					an->predicate->dynamic->linkage_due_to_line = an->line;
				}
			}
		}
	} else if(an->kind == AN_BLOCK || an->kind == AN_FIRSTRESULT) {
		for(an = an->children[0]; an; an = an->next_in_body) {
			trace_now_expression(an, vars);
		}
	} else {
		assert(0);
		exit(1);
	}
}

int trace_invocations_body(struct astnode **anptr, int flags, struct tracevar **vars, struct clause *me, int tail) {
	struct astnode *an;
	struct tracevar *subvars, *combined_subvars;
	int i, failed = 0;
	uint32_t unbound;
	int moreflags;

	while((an = *anptr) && !failed) {
		switch(an->kind) {
		case AN_RULE:
		case AN_NEG_RULE:
			an->predicate->invoked_at_line = an->line;
			moreflags = 0;
			if(an->subkind == RULE_SIMPLE
			|| (tail && !an->next_in_body && (me->predicate->flags & PREDF_INVOKED_SIMPLE))) {
				moreflags |= PREDF_INVOKED_SIMPLE;
				if(an->predicate != me->predicate) an->predicate->simple_due_to_line = an->line;
			} else {
				moreflags |= PREDF_INVOKED_MULTI;
			}
			for(i = 0; i < an->predicate->arity; i++) {
				if(any_unbound(an->children[i], *vars)) {
					an->children[i]->unbound = 1;
				}
			}
			if(an->predicate->flags & PREDF_FAIL) {
				failed = 1;
			} else if(an->predicate->dynamic) {
				if(an->predicate->arity
				&& !(an->predicate->flags & PREDF_GLOBAL_VAR)
				&& an->predicate->builtin != BI_HASPARENT) {
					if(an->children[0]->unbound) {
						if(an->predicate->arity > 1) {
							report(
								LVL_WARN,
								an->line,
								"Dynamic predicate with unbound first argument will loop over all objects.");
							*anptr = mkast(AN_RULE);
							(*anptr)->line = an->line;
							(*anptr)->subkind = RULE_MULTI;
							(*anptr)->predicate = find_builtin(BI_OBJECT);
							(*anptr)->nchild = 1;
							(*anptr)->children = malloc(sizeof(struct astnode *));
							(*anptr)->children[0] = deepcopy_astnode(an->children[0], an->line);
							(*anptr)->unbound = 1;
							(*anptr)->next_in_body = an;
							an->children[0]->unbound = 0;
						} else {
							an->predicate->dynamic->linkage_flags |= LINKF_LIST;
							an->predicate->dynamic->linkage_due_to_line = an->line;
						}
					}
				}
				for(i = 0; i < an->predicate->arity; i++) {
					add_bound_vars(an->children[i], vars);
				}
			} else {
				unbound = 0;
				for(i = 0; i < an->predicate->arity; i++) {
					if(an->children[i]->unbound) {
						unbound |= 1 << i;
					}
				}
				if(!(me->predicate->flags & PREDF_VISITED)) {
					trace_add_caller(an->predicate, me->predicate);
				}
				trace_invoke_pred(
					an->predicate,
					flags | moreflags,
					unbound,
					me);
				if(an->predicate->builtin == BI_STOP) {
					failed = 1;
				}
				if(an->kind == AN_RULE) {
					if(an->predicate->builtin == BI_UNIFY) {
						if(!an->children[0]->unbound || !an->children[1]->unbound) {
							for(i = 0; i < 2; i++) {
								add_bound_vars(an->children[i], vars);
							}
						}
					} else {
						for(i = 0; i < an->predicate->arity; i++) {
							if(!(an->predicate->unbound_out & (1 << i))) {
								add_bound_vars(an->children[i], vars);
							}
						}
					}
				}
			}
			break;
		case AN_NOW:
			trace_now_expression(an->children[0], *vars);
			break;
		case AN_BLOCK:
		case AN_FIRSTRESULT:
			if(flags & PREDF_INVOKED_FOR_WORDS) {
				flags |= PREDF_INVOKED_DEEP_WORDS;
			}
			failed = !trace_invocations_body(&an->children[0], flags, vars, me, tail && !an->next_in_body);
			break;
		case AN_STOPPABLE:
			if(flags & PREDF_INVOKED_FOR_WORDS) {
				flags |= PREDF_INVOKED_DEEP_WORDS;
			}
			subvars = *vars;
			(void) trace_invocations_body(&an->children[0], flags, &subvars, me, 1);
			break;
		case AN_STATUSBAR:
			if(flags & PREDF_INVOKED_FOR_WORDS) {
				flags |= PREDF_INVOKED_DEEP_WORDS;
			}
			subvars = *vars;
			(void) trace_invocations_body(&an->children[1], PREDF_INVOKED_NORMALLY, &subvars, me, 0);
			break;
		case AN_NEG_BLOCK:
			if(flags & PREDF_INVOKED_FOR_WORDS) {
				flags |= PREDF_INVOKED_DEEP_WORDS;
			}
			for(i = 0; i < an->nchild; i++) {
				subvars = *vars;
				(void) trace_invocations_body(&an->children[i], flags, &subvars, me, 0);
			}
			break;
		case AN_SELECT:
			if(flags & PREDF_INVOKED_FOR_WORDS) {
				flags |= PREDF_INVOKED_DEEP_WORDS;
			}
			/* drop through */
		case AN_OR:
			combined_subvars = 0;
			for(i = 0; i < an->nchild; i++) {
				subvars = *vars;
				if(trace_invocations_body(&an->children[i], flags, &subvars, me, tail && !an->next_in_body)) {
					if(!combined_subvars) {
						combined_subvars = subvars;
					} else {
						join_subvars(&combined_subvars, subvars, *vars);
					}
				}
			}
			if(combined_subvars) {
				*vars = combined_subvars;
			}
			break;
		case AN_EXHAUST:
			if(flags & PREDF_INVOKED_FOR_WORDS) {
				flags |= PREDF_INVOKED_DEEP_WORDS;
			}
			subvars = *vars;
			(void) trace_invocations_body(&an->children[0], flags, &subvars, me, 0);
			break;
		case AN_IF:
			if(flags & PREDF_INVOKED_FOR_WORDS) {
				flags |= PREDF_INVOKED_DEEP_WORDS;
			}
			subvars = *vars;
			trace_invocations_body(&an->children[0], flags, &subvars, me, 0);
			trace_invocations_body(&an->children[1], flags, &subvars, me, tail && !an->next_in_body);
			combined_subvars = subvars;
			subvars = *vars;
			trace_invocations_body(&an->children[2], flags, &subvars, me, tail && !an->next_in_body);
			join_subvars(&combined_subvars, subvars, *vars);
			*vars = combined_subvars;
			break;
		case AN_COLLECT:
			subvars = *vars;
			trace_invocations_body(&an->children[0], PREDF_INVOKED_NORMALLY, &subvars, me, 0);
			add_bound_vars(an->children[2], vars);
			break;
		case AN_COLLECT_WORDS:
			subvars = *vars;
			trace_invocations_body(&an->children[0], PREDF_INVOKED_SHALLOW_WORDS, &subvars, me, 0);
			add_bound_vars(an->children[1], vars);
			break;
		case AN_COLLECT_WORDS_CHECK:
			subvars = *vars;
			trace_invocations_body(&an->children[0], PREDF_INVOKED_SHALLOW_WORDS, &subvars, me, 0);
			break;
		}
		anptr = &an->next_in_body;
	}

	return !failed;
}

uint32_t trace_reconsider_clause(struct clause *cl, int nparam, int flags, uint32_t unbound_in) {
	int i;
	struct tracevar *vars = 0;
	uint32_t unbound_out = 0;

	for(i = 0; i < nparam; i++) {
		if(!(unbound_in & (1 << i))) {
			add_bound_vars(cl->params[i], &vars);
		}
	}

	if(trace_invocations_body(&cl->body, flags, &vars, cl, 1)) {
		for(i = 0; i < nparam; i++) {
			if((unbound_in & (1 << i))
			&& any_unbound(cl->params[i], vars)) {
				unbound_out |= 1 << i;
			}
		}
	}

	return unbound_out;
}

void trace_reconsider_pred(struct predicate *pred) {
	int i;
	uint32_t unbound = pred->unbound_out;
	struct predlist *pl;

	for(i = 0; i < pred->nclause; i++) {
		unbound |= trace_reconsider_clause(
			pred->clauses[i],
			pred->arity,
			pred->flags & PREDF_INVOKED,
			pred->unbound_in);
	}
	pred->flags |= PREDF_VISITED;

	if(unbound != pred->unbound_out) {
		pred->unbound_out = unbound;
		for(pl = pred->callers; pl; pl = pl->next) {
			trace_enqueue(pl->pred);
		}
	}
}

void trace_invoke_pred(struct predicate *pred, int flags, uint32_t unbound_in, struct clause *caller) {
	int i;

	if((pred->flags != (pred->flags | flags))
	|| (pred->unbound_in != (pred->unbound_in | unbound_in))) {
		for(i = 0; i < pred->arity; i++) {
			if((unbound_in & (1 << i))
			&& !(pred->unbound_in & (1 << i))) {
				pred->unbound_in_due_to[i] = caller;
			}
		}
		pred->flags |= flags;
		pred->unbound_in |= unbound_in;
		trace_enqueue(pred);
	}
}

int body_can_be_fixed_flag(struct astnode *an, struct word *safevar) {
	struct predicate *pred;
	int have_constrained_safevar = !safevar;
	struct astnode *sub;

	while(an) {
		if(an->kind == AN_RULE
		|| an->kind == AN_NEG_RULE) {
			pred = an->predicate;
			if(pred->builtin == BI_IS_ONE_OF
			&& (an->children[0]->kind == AN_VARIABLE)
			&& (an->children[0]->word == safevar)) {
				for(sub = an->children[1]; sub->kind == AN_PAIR; sub = sub->children[1]) {
					if(sub->children[0]->kind != AN_TAG) break;
				}
				return sub->kind == AN_EMPTY_LIST;
			} else if(pred->builtin) {
				if(pred->builtin != BI_FAIL
				&& pred->builtin != BI_OBJECT) {
					return 0;
				}
			} else if((pred->flags & PREDF_FIXED_FLAG)
			&& (an->children[0]->kind == AN_VARIABLE)
			&& (an->children[0]->word == safevar)) {
				have_constrained_safevar = 1;
			} else {
				return 0;
			}
		} else if(an->kind == AN_BLOCK
		|| an->kind == AN_NEG_BLOCK) {
			if(body_can_be_fixed_flag(an->children[0], safevar)) {
				have_constrained_safevar = 1;
			} else {
				return 0;
			}
		} else if(an->kind != AN_JUST) {
			return 0;
		}
		an = an->next_in_body;
	}

	return have_constrained_safevar;
}

int pred_can_be_fixed_flag(struct predicate *pred) {
	int i;

	if(!pred->special
	&& !pred->builtin
	&& pred->nclause
	&& !pred->dynamic
	&& !(pred->flags & PREDF_MACRO)
	&& (pred->flags & PREDF_INVOKED)
	&& pred->arity == 1) {
		for(i = 0; i < pred->nclause; i++) {
			if(pred->clauses[i]->params[0]->kind == AN_TAG) {
				if(!body_can_be_fixed_flag(pred->clauses[i]->body, 0)) {
					return 0;
				}
			} else if(pred->clauses[i]->params[0]->kind == AN_VARIABLE
			&& pred->clauses[i]->params[0]->word->name[0]) {
				if(!body_can_be_fixed_flag(
					pred->clauses[i]->body,
					pred->clauses[i]->params[0]->word))
				{
					return 0;
				}
			} else {
				return 0;
			}
		}

		return 1;
	}

	return 0;
}

void find_fixed_flags() {
	int i;
	struct predicate *pred;
	int flag;

	do {
		flag = 0;
		for(i = 0; i < npredicate; i++) {
			pred = predicates[i];

			if(!(pred->flags & PREDF_FIXED_FLAG)
			&& pred_can_be_fixed_flag(pred)) {
#if 0
				printf("candidate: ");
				pp_predname(pred);
				printf("\n");
#endif
				pred->flags |= PREDF_DYNAMIC | PREDF_FIXED_FLAG;
				if(!pred->dynamic) pred->dynamic = calloc(1, sizeof(struct dynamic));
				flag = 1;
			}
		}
	} while(flag);
}

void trace_invocations() {
	struct predicate *pred;

	tracequeue = malloc(npredicate * sizeof(struct predicate *));

	pred = find_builtin(BI_CONSTRUCTORS);
	pred->flags |= PREDF_INVOKED_SIMPLE | PREDF_INVOKED_MULTI;
	trace_invoke_pred(pred, PREDF_INVOKED_NORMALLY, 0, 0);

	pred = find_builtin(BI_ERROR_ENTRY);
	pred->flags |= PREDF_INVOKED_SIMPLE | PREDF_INVOKED_MULTI;
	trace_invoke_pred(pred, PREDF_INVOKED_NORMALLY, 0, 0);

	while((pred = trace_read_queue())) {
		trace_reconsider_pred(pred);
	}
}

void mark_all_dynamic(struct astnode *an, line_t line, int allow_multi) {
	while(an) {
		if(an->kind == AN_RULE
		|| an->kind == AN_NEG_RULE) {
			if(an->predicate->builtin && an->predicate->builtin != BI_HASPARENT) {
				report(LVL_ERR, line, "(now) cannot be combined with this builtin predicate.");
				exit(1);
			}
			if(an->subkind == RULE_MULTI && !allow_multi) {
				report(LVL_ERR, line, "(now) cannot be combined with a multi-call.");
				exit(1);
			}
			an->predicate->flags |= PREDF_DYNAMIC;
			if(!an->predicate->dynamic) {
				an->predicate->dynamic = calloc(1, sizeof(struct dynamic));
				an->predicate->dynamic->dyn_due_to_line = line;
			}
			analyse_dynamic(an, an->kind == AN_RULE);
		} else if(an->kind == AN_BLOCK) {
			mark_all_dynamic(an->children[0], line, allow_multi);
		} else if(an->kind == AN_FIRSTRESULT) {
			mark_all_dynamic(an->children[0], line, 1);
		} else {
			report(LVL_ERR, line, "(now) only works with rules, negated rules and blocks.");
			exit(1);
		}
		an = an->next_in_body;
	}
}

void find_dynamic(struct astnode *an, line_t line) {
	int i;

	while(an) {
		if(an->kind == AN_NOW) {
			mark_all_dynamic(an->children[0], line, 0);
		} else {
			for(i = 0; i < an->nchild; i++) {
				find_dynamic(an->children[i], line);
			}
		}
		an = an->next_in_body;
	}
}

struct clause *parse_clause(int is_macro) {
	struct astnode *an, **dest, **folddest;
	struct astnode *nestednodes[MAXNESTEDEXPR];
	int nnested = 0, nested_nonvar = 0;
	struct clause *cl;
	int i, j;

	if(is_macro) {
		an = parse_rule();
	} else {
		an = parse_rule_head(nestednodes, &nnested, &nested_nonvar);
	}

	if(an->predicate->arity > 12) {
		report(LVL_ERR, line, "Maximum number of arguments exceeded.");
		exit(1);
	}

	if(nested_nonvar
	&& an->predicate->special != SP_GLOBAL_VAR
	&& an->predicate->special != SP_GLOBAL_VAR_2
	&& an->predicate->special != SP_GENERATE) {
		report(LVL_ERR, an->line, "First parameter of nested rule must be a variable.");
		exit(1);
	}

	if(an->predicate->special && !(an->predicate->flags & PREDF_META)) {
		report(LVL_ERR, line, "Special syntax cannot be redefined.");
		exit(1);
	} else {
		cl = calloc(1, sizeof(*cl));
		cl->predicate = an->predicate;
		cl->line = line;
		cl->params = malloc(cl->predicate->arity * sizeof(struct astnode *));
		j = 0;
		for(i = 0; i < an->nchild; i++) {
			if(an->children[i]->kind != AN_BAREWORD) {
				assert(j < cl->predicate->arity);
				cl->params[j++] = an->children[i];
			}
		}
		assert(j == cl->predicate->arity);
		dest = &cl->body;
		for(i = 0; i < nnested; i++) {
			an = nestednodes[i];
			*dest = an;
			dest = &an->next_in_body;
		}
		folddest = dest;
		for(;;) {
			status = next_token(&tok, f, PMODE_BODY);
			if(status != 1) {
				*dest = 0;
				break;
			}
			an = parse_expr(PMODE_BODY);
			*dest = an;
			dest = &an->next_in_body;
		}
		*folddest = fold_disjunctions(*folddest);
	}

	return cl;
}

struct astnode *parse_if() {
	struct astnode *ifnode = mkast(AN_IF), *sub, **dest;

	ifnode->word = fresh_word(); // handy for tracking where to store the cutpoint in the environment
	ifnode->nchild = 3;
	ifnode->children = malloc(3 * sizeof(struct astnode *));
	dest = &ifnode->children[0];
	for(;;) {
		status = next_token(&tok, f, PMODE_BODY);
		if(status != 1) {
			report(LVL_ERR, ifnode->line, "(if) without (then).");
			exit(1);
		}
		sub = parse_expr(PMODE_BODY);
		if(sub->kind == AN_RULE && sub->predicate->special == SP_THEN) {
			*dest = 0;
			break;
		}
		*dest = sub;
		dest = &sub->next_in_body;
	}
	ifnode->children[0] = fold_disjunctions(ifnode->children[0]);
	dest = &ifnode->children[1];
	for(;;) {
		status = next_token(&tok, f, PMODE_BODY);
		if(status != 1) {
			report(LVL_ERR, line, "Unterminated (then)-expression.");
			exit(1);
		}
		sub = parse_expr(PMODE_BODY);
		if(sub->kind == AN_RULE && sub->predicate->special == SP_ENDIF) {
			*dest = 0;
			ifnode->children[2] = 0;
			break;
		} else if(sub->kind == AN_RULE && sub->predicate->special == SP_ELSE) {
			*dest = 0;
			dest = &ifnode->children[2];
			for(;;) {
				status = next_token(&tok, f, PMODE_BODY);
				if(status != 1) {
					report(LVL_ERR, line, "Unterminated (else)-expression.");
					exit(1);
				}
				sub = parse_expr(PMODE_BODY);
				if(sub->kind == AN_RULE && sub->predicate->special == SP_ENDIF) {
					*dest = 0;
					break;
				}
				*dest = sub;
				dest = &sub->next_in_body;
			}
			break;
		} else if(sub->kind == AN_RULE && sub->predicate->special == SP_ELSEIF) {
			*dest = 0;
			ifnode->children[2] = parse_if();
			break;
		}
		*dest = sub;
		dest = &sub->next_in_body;
	}
	ifnode->children[1] = fold_disjunctions(ifnode->children[1]);
	ifnode->children[2] = fold_disjunctions(ifnode->children[2]);

	return ifnode;
}

struct astnode *parse_expr(int parsemode) {
	struct astnode *an, *sub, **dest;

	switch(tok.kind) {
	case TOK_VARIABLE:
		an = mkast(AN_VARIABLE);
		an->word = tok.word;
		break;
	case TOK_TAG:
		an = mkast(AN_TAG);
		an->word = tok.word;
		break;
	case TOK_BAREWORD:
		if(parsemode == PMODE_VALUE) {
			an = mkast(AN_DICTWORD);
		} else {
			an = mkast(AN_BAREWORD);
			if(parsemode == PMODE_BODY
			&& (isalnum(tok.word->name[0]) || tok.word->name[1])) {
				wordcount++;
			}
		}
		an->word = tok.word;
		break;
	case TOK_DICTWORD:
		an = mkast(AN_DICTWORD);
		an->word = tok.word;
		break;
	case TOK_INTEGER:
		an = mkast(AN_INTEGER);
		an->value = tok.value;
		break;
	case TOK_STARPAREN:
		an = parse_rule();
		if(an->predicate->special) {
			report(LVL_ERR, line, "Syntax error.");
			exit(1);
		}
		an->subkind = RULE_MULTI;
		break;
	case '*':
		if(!starword) {
			report(LVL_ERR, line, "Star syntax used before current topic has been defined.");
			exit(1);
		}
		an = mkast(AN_TAG);
		an->word = starword;
		break;
	case '(':
		if(parsemode != PMODE_BODY) {
			report(LVL_ERR, line, "Nested rules are only allowed inside rule heads.");
			exit(1);
		}
		an = parse_rule();
		if(an->predicate->special == SP_NOW) {
			an = mkast(AN_NOW);
			an->nchild = 1;
			an->children = malloc(sizeof(struct astnode *));
			status = next_token(&tok, f, PMODE_BODY);
			if(status != 1) {
				report(LVL_ERR, line, "Expected expression after (now).");
				exit(1);
			}
			an->children[0] = parse_expr(PMODE_BODY);
		} else if(an->predicate->special == SP_EXHAUST) {
			an = mkast(AN_EXHAUST);
			an->nchild = 1;
			an->children = malloc(1 * sizeof(struct astnode *));
			status = next_token(&tok, f, PMODE_BODY);
			if(status != 1) {
				report(LVL_ERR, line, "Expected expression after (exhaust).");
				exit(1);
			}
			an->children[0] = parse_expr(PMODE_BODY);
		} else if(an->predicate->special == SP_JUST) {
			an = mkast(AN_JUST);
		} else if(an->predicate->special == SP_SELECT) {
			an = mkast(AN_SELECT);
			an->nchild = 1;
			an->children = malloc(sizeof(struct astnode *));
			dest = &an->children[0];
			for(;;) {
				status = next_token(&tok, f, PMODE_BODY);
				if(status != 1) {
					report(LVL_ERR, line, "Unterminated select.");
					exit(1);
				}
				sub = parse_expr(PMODE_BODY);
				if(sub->kind == AN_RULE && sub->predicate->special == SP_OR) {
					*dest = 0;
					an->children = realloc(an->children, (an->nchild + 1) * sizeof(struct astnode *));
					dest = &an->children[an->nchild++];
				} else if(sub->kind == AN_RULE && sub->predicate->special == SP_STOPPING) {
					*dest = 0;
					an->subkind = SEL_STOPPING;
					break;
				} else if(sub->kind == AN_RULE && sub->predicate->special == SP_RANDOM) {
					*dest = 0;
					an->subkind = SEL_RANDOM;
					break;
				} else if(sub->kind == AN_RULE && sub->predicate->special == SP_P_RANDOM) {
					*dest = 0;
					an->subkind = SEL_P_RANDOM;
					break;
				} else if(sub->kind == AN_RULE && sub->predicate->special == SP_T_RANDOM) {
					*dest = 0;
					an->subkind = SEL_T_RANDOM;
					break;
				} else if(sub->kind == AN_RULE && sub->predicate->special == SP_T_P_RANDOM) {
					*dest = 0;
					an->subkind = SEL_T_P_RANDOM;
					break;
				} else if(sub->kind == AN_RULE && sub->predicate->special == SP_CYCLING) {
					*dest = 0;
					an->subkind = SEL_CYCLING;
					break;
				} else {
					*dest = sub;
					dest = &sub->next_in_body;
				}
			}
		} else if(an->predicate->special == SP_COLLECT) {
			sub = an;
			an = mkast(AN_COLLECT);
			an->nchild = 3;
			an->children = malloc(3 * sizeof(struct astnode *));
			an->children[1] = sub->children[0];
			dest = &an->children[0];
			for(;;) {
				status = next_token(&tok, f, PMODE_BODY);
				if(status != 1) {
					report(LVL_ERR, line, "Unterminated (collect $).");
					exit(1);
				}
				sub = parse_expr(PMODE_BODY);
				if(sub->kind == AN_RULE && sub->predicate->special == SP_INTO) {
					*dest = 0;
					an->children[2] = sub->children[0];
					break;
				}
				*dest = sub;
				dest = &sub->next_in_body;
			}
			an->children[0] = fold_disjunctions(an->children[0]);
		} else if(an->predicate->special == SP_COLLECT_WORDS) {
			an = mkast(AN_COLLECT_WORDS);
			an->nchild = 2;
			an->children = malloc(2 * sizeof(struct astnode *));
			dest = &an->children[0];
			for(;;) {
				status = next_token(&tok, f, PMODE_BODY);
				if(status != 1) {
					report(LVL_ERR, line, "Unterminated (collect words).");
					exit(1);
				}
				sub = parse_expr(PMODE_BODY);
				if(sub->kind == AN_RULE && sub->predicate->special == SP_INTO) {
					*dest = 0;
					an->children[1] = sub->children[0];
					break;
				} else if(sub->kind == AN_RULE && sub->predicate->special == SP_AND_CHECK) {
					*dest = 0;
					an->kind = AN_COLLECT_WORDS_CHECK;
					an->children[1] = sub->children[0];
					break;
				}
				*dest = sub;
				dest = &sub->next_in_body;
			}
			an->children[0] = fold_disjunctions(an->children[0]);
		} else if(an->predicate->special == SP_STOPPABLE) {
			an = mkast(AN_STOPPABLE);
			an->nchild = 1;
			an->children = malloc(1 * sizeof(struct astnode *));
			status = next_token(&tok, f, PMODE_BODY);
			if(status != 1) {
				report(LVL_ERR, line, "Expected expression after (stoppable).");
				exit(1);
			}
			an->children[0] = parse_expr(PMODE_BODY);
			for(dest = &an->children[0]->next_in_body; *dest; dest = &(*dest)->next_in_body);
			*dest = mkast(AN_RULE);
			(*dest)->predicate = stoppred;
		} else if(an->predicate->special == SP_STATUSBAR) {
			sub = an->children[0];
			an = mkast(AN_STATUSBAR);
			an->word = fresh_word();
			an->nchild = 2;
			an->children = malloc(2 * sizeof(struct astnode *));
			an->children[0] = sub;
			status = next_token(&tok, f, PMODE_BODY);
			if(status != 1) {
				report(LVL_ERR, line, "Expected expression after (status bar $).");
				exit(1);
			}
			an->children[1] = parse_expr(PMODE_BODY);
			for(dest = &an->children[1]->next_in_body; *dest; dest = &(*dest)->next_in_body);
			*dest = mkast(AN_RULE);
			(*dest)->predicate = stoppred;
		} else if(an->predicate->special == SP_IF) {
			an = parse_if();
		} else if(an->predicate->builtin == BI_REPEAT) {
			report(LVL_WARN, an->line, "(repeat forever) not invoked as a multi-query.");
		}
		break;
	case '{':
		an = mkast(AN_BLOCK);
		an->nchild = 1;
		an->children = malloc(sizeof(struct astnode *));
		dest = &an->children[0];
		for(;;) {
			status = next_token(&tok, f, PMODE_BODY);
			if(status != 1) {
				report(LVL_ERR, line, "Unterminated block.");
				exit(1);
			}
			if(tok.kind == '}') {
				*dest = 0;
				break;
			}
			sub = parse_expr(PMODE_BODY);
			*dest = sub;
			dest = &sub->next_in_body;
		}
		an->children[0] = fold_disjunctions(an->children[0]);
		if(an->children[0] && !an->children[0]->next_in_body) {
			an = an->children[0];
		}
		break;
	case '~':
		status = next_token(&tok, f, PMODE_BODY);
		if(status != 1 || (tok.kind != '(' && tok.kind != '{')) {
			report(LVL_ERR, line, "Syntax error after ~.");
			exit(1);
		}
		if(tok.kind == '(') {
			an = parse_rule();
			if(an->predicate->special) {
				report(LVL_ERR, line, "Special syntax cannot be negated.");
				exit(1);
			}
			an->kind = AN_NEG_RULE;
			an->word = fresh_word(); // handy for tracking where to store the cutpoint in the environment
		} else {
			an = mkast(AN_NEG_BLOCK);
			an->word = fresh_word(); // handy for tracking where to store the cutpoint in the environment
			an->nchild = 1;
			an->children = malloc(sizeof(struct astnode *));
			dest = &an->children[0];
			for(;;) {
				status = next_token(&tok, f, PMODE_BODY);
				if(status != 1) {
					report(LVL_ERR, line, "Unterminated block.");
					exit(1);
				}
				if(tok.kind == '}') {
					*dest = 0;
					break;
				}
				sub = parse_expr(PMODE_BODY);
				*dest = sub;
				dest = &sub->next_in_body;
			}
			an->children[0] = fold_disjunctions(an->children[0]);
			if(an->children[0] && !an->children[0]->next_in_body && an->children[0]->kind == AN_RULE) {
				an = an->children[0];
				an->kind = AN_NEG_RULE;
			}
		}
		break;
	case '[':
		dest = &an;
		for(;;) {
			status = next_token(&tok, f, PMODE_VALUE);
			if(status != 1) {
				report(LVL_ERR, line, "Unterminated list.");
				exit(1);
			}
			if(tok.kind == ']') {
				*dest = mkast(AN_EMPTY_LIST);
				break;
			} else if(tok.kind == '|') {
				status = next_token(&tok, f, PMODE_VALUE);
				if(status != 1) {
					report(LVL_ERR, line, "Syntax error near end of list.");
					exit(1);
				}
				*dest = parse_expr(PMODE_VALUE);
				status = next_token(&tok, f, PMODE_VALUE);
				if(status != 1 || tok.kind != ']') {
					report(LVL_ERR, line, "Syntax error near end of list.");
					exit(1);
				}
				break;
			} else {
				sub = mkast(AN_PAIR);
				sub->nchild = 2;
				sub->children = malloc(2 * sizeof(struct astnode *));
				sub->children[0] = parse_expr(PMODE_VALUE);
				*dest = sub;
				dest = &sub->children[1];
			}
		}
		break;
	default:
		report(LVL_ERR, line, "Unexpected %c.", tok.kind);
		exit(1);
	}

	return an;
}

struct astnode *parse_expr_nested(struct astnode **nested_rules, int *nnested, int parsemode, int *nonvar_detected) {
	struct astnode *an, *var, *sub, **dest, *list;
	int negated = 0;

	switch(tok.kind) {
	case '~':
		status = next_token(&tok, f, PMODE_RULE);
		if(status != 1 || tok.kind != '(') {
			report(LVL_ERR, line, "Expected ( after ~ in rule-head.");
			exit(1);
		}
		negated = 1;
		/* drop through */
	case '(':
	case TOK_STARPAREN:
		if(tok.kind == TOK_STARPAREN) {
			an = parse_rule();
			an->subkind = RULE_MULTI;
		} else {
			an = parse_rule();
		}
		if(an->predicate->special) {
			report(LVL_ERR, line, "Special rule can't appear nested.");
			exit(1);
		}
		if(negated) an->kind = AN_NEG_RULE;
		if(!an->nchild) {
			report(LVL_ERR, line, "Nested rule must have at least one parameter.");
			exit(1);
		}
		if(*nnested >= MAXNESTEDEXPR) {
			report(LVL_ERR, an->line, "Too many nested expressions.");
			exit(1);
		}
		if(an->children[0]->kind == AN_VARIABLE) {
			var = an->children[0];
			if(!var->word->name[0]) {
				var->word = fresh_word();
			}
			nested_rules[(*nnested)++] = an;
			an = mkast(AN_VARIABLE);
			an->word = var->word;
			return an;
		} else {
			*nonvar_detected = 1;
			nested_rules[(*nnested)++] = an;
			return an->children[0];
		}
		break;
	case '[':
		dest = &an;
		for(;;) {
			status = next_token(&tok, f, PMODE_VALUE);
			if(status != 1) {
				report(LVL_ERR, line, "Unterminated list.");
				exit(1);
			}
			if(tok.kind == ']') {
				*dest = mkast(AN_EMPTY_LIST);
				break;
			} else if(tok.kind == '|') {
				status = next_token(&tok, f, PMODE_VALUE);
				if(status != 1) {
					report(LVL_ERR, line, "Syntax error near end of list.");
					exit(1);
				}
				*dest = parse_expr_nested(nested_rules, nnested, PMODE_VALUE, 0);
				status = next_token(&tok, f, PMODE_VALUE);
				if(status != 1 || tok.kind != ']') {
					report(LVL_ERR, line, "Syntax error near end of list.");
					exit(1);
				}
				break;
			} else {
				sub = mkast(AN_PAIR);
				sub->nchild = 2;
				sub->children = malloc(2 * sizeof(struct astnode *));
				sub->children[0] = parse_expr_nested(nested_rules, nnested, PMODE_VALUE, 0);
				*dest = sub;
				dest = &sub->children[1];
			}
		}
		return an;
	case '{':
		report(LVL_ERR, line, "Unexpected block in rule-head.");
		exit(1);
	default:
		an = parse_expr(parsemode);
		if(parsemode != PMODE_BODY && look_ahead_for_slash(f)) {
			if(*nnested >= MAXNESTEDEXPR) {
				report(LVL_ERR, an->line, "Too many nested expressions.");
				exit(1);
			}
			list = mkast(AN_PAIR);
			list->line = an->line;
			list->nchild = 2;
			list->children = malloc(2 * sizeof(struct astnode *));
			list->children[0] = an;
			dest = &list->children[1];
			do {
				status = next_token(&tok, f, parsemode);
				assert(status && tok.kind == '/');
				status = next_token(&tok, f, parsemode);
				if(!status) {
					report(LVL_ERR, list->line, "Syntax error near end of slash-expression.");
					exit(1);
				}
				an = parse_expr(parsemode);
				if(an->kind == AN_VARIABLE
				|| an->kind == AN_PAIR) {
					report(LVL_ERR, an->line, "Invalid kind of value inside a slash-expression.");
					exit(1);
				}
				*dest = mkast(AN_PAIR);
				(*dest)->nchild = 2;
				(*dest)->children = malloc(2 * sizeof(struct astnode *));
				(*dest)->children[0] = an;
				dest = &(*dest)->children[1];
			} while(look_ahead_for_slash(f));
			*dest = mkast(AN_EMPTY_LIST);
			var = mkast(AN_VARIABLE);
			var->word = fresh_word();
			an = mkast(AN_RULE);
			an->subkind = RULE_MULTI;
			an->predicate = find_builtin(BI_IS_ONE_OF);
			an->nchild = 2;
			an->children = malloc(2 * sizeof(struct astnode *));
			an->children[0] = mkast(AN_VARIABLE);
			an->children[0]->word = var->word;
			an->children[1] = list;
			nested_rules[(*nnested)++] = an;
			an = var;
		}
		return an;
	}
}

void add_clause(struct clause *cl, struct predicate *pred) {
	pred->clauses = realloc(pred->clauses, (pred->nclause + 1) * sizeof(struct clause *));
	pred->clauses[pred->nclause++] = cl;
}

void pp_expr(struct astnode *an);

void pp_pair(struct astnode *head, struct astnode *tail) {
	pp_expr(head);
	if(tail->kind != AN_EMPTY_LIST) {
		if(tail->kind == AN_PAIR) {
			printf(" ");
			pp_pair(tail->children[0], tail->children[1]);
		} else {
			printf(" | ");
			pp_expr(tail);
		}
	}
}

void pp_body(struct astnode *an) {
	while(an) {
		pp_expr(an);
		an = an->next_in_body;
		if(an) printf(" ");
	}
}

void pp_expr(struct astnode *an) {
	int i, j;

	switch(an->kind) {
	case AN_VARIABLE:
		printf("$%s", an->word->name);
		//if(an->unbound) printf("{u}");
		break;
	case AN_TAG:
		printf("#%s", an->word->name);
		break;
	case AN_INTEGER:
		printf("%d", an->value);
		break;
	case AN_EMPTY_LIST:
		printf("[]");
		break;
	case AN_PAIR:
		printf("[");
		pp_pair(an->children[0], an->children[1]);
		printf("]");
		break;
	case AN_BAREWORD:
		printf("%s", an->word->name);
		break;
	case AN_DICTWORD:
		printf("@%s", an->word->name);
		break;
	case AN_NOW:
		printf("(now) ");
		pp_expr(an->children[0]);
		break;
	case AN_JUST:
		printf("(just)");
		break;
	case AN_SELECT:
		printf("(select) ");
		for(i = 0; i < an->nchild; i++) {
			if(i) printf(" (or) ");
			pp_body(an->children[i]);
		}
		if(an->subkind == SEL_STOPPING) {
			printf(" (stopping)");
		} else if(an->subkind == SEL_RANDOM) {
			printf(" (at random)");
		} else if(an->subkind == SEL_P_RANDOM) {
			printf(" (purely at random)");
		} else if(an->subkind == SEL_T_RANDOM) {
			printf(" (then at random)");
		} else if(an->subkind == SEL_T_P_RANDOM) {
			printf(" (then purely at random)");
		} else if(an->subkind == SEL_CYCLING) {
			printf(" (cycling)");
		}
		break;
	case AN_STOPPABLE:
		printf("(stoppable) ");
		pp_expr(an->children[0]);
		break;
	case AN_COLLECT:
		printf("(collect ");
		pp_expr(an->children[1]);
		printf(") ");
		pp_body(an->children[0]);
		printf(" (into ");
		pp_expr(an->children[2]);
		printf(")");
		break;
	case AN_COLLECT_WORDS:
		printf("(collect words) ");
		pp_body(an->children[0]);
		printf(" (into ");
		pp_expr(an->children[1]);
		printf(")");
		break;
	case AN_COLLECT_WORDS_CHECK:
		printf("(collect words) ");
		pp_body(an->children[0]);
		printf(" (and check ");
		pp_expr(an->children[1]);
		printf(")");
		break;
	case AN_IF:
		printf("(if) ");
		pp_body(an->children[0]);
		printf(" (then) ");
		pp_body(an->children[1]);
		printf(" (else) ");
		pp_body(an->children[2]);
		printf(" (endif)");
		break;
	case AN_NEG_BLOCK:
		printf("~");
		/* drop through */
	case AN_BLOCK:
		printf("{");
		pp_body(an->children[0]);
		printf("}");
		break;
	case AN_NEG_RULE:
		printf("~");
		/* drop through */
	case AN_RULE:
		if(an->subkind == RULE_MULTI) printf("*");
		printf("(");
		if(an->predicate->special) printf("?special? ");
		j = 0;
		for(i = 0; i < an->predicate->nword; i++) {
			if(i) printf(" ");
			if(an->predicate->words[i]) {
				printf("%s", an->predicate->words[i]->name);
			} else {
				pp_expr(an->children[j++]);
			}
		}
		printf(")");
		break;
	case AN_OR:
		pp_body(an->children[0]);
		printf(" (or) ");
		pp_body(an->children[1]);
		break;
	case AN_EXHAUST:
		printf("(exhaust) ");
		pp_body(an->children[0]);
		break;
	case AN_FIRSTRESULT:
		printf("(*first result*) {");
		pp_body(an->children[0]);
		printf("}");
		break;
	default:
		printf("?unknown?");
		break;
	}
}

void pp_clause(struct clause *cl) {
	int i;

	printf("\t%s:%d:", FILEPART(cl->line), LINEPART(cl->line));
	for(i = 0; i < cl->predicate->arity; i++) {
		printf(" ");
		pp_expr(cl->params[i]);
	}
	printf(" ---> ");
	pp_body(cl->body);
	printf("\n");
}

void pp_predname(struct predicate *pred) {
	int i, k;

	printf("(");
	k = 0;
	for(i = 0; i < pred->nword; i++) {
		if(i) printf(" ");
		if(pred->words[i]) {
			printf("%s", pred->words[i]->name);
		} else {
			if(pred->unbound_in & (1 << k)) {
				printf("$u");
			} else {
				printf("$b");
			}
			if(pred->unbound_out & (1 << k)) {
				printf("u");
			} else {
				printf("b");
			}
			k++;
		}
	}
	printf(")");
}

void pp_predicate(struct predicate *pred) {
	int i;

	printf("Predicate %c%c%c%c ",
		(pred->flags & PREDF_INVOKED_NORMALLY)? 'N' : '-',
		(pred->flags & PREDF_INVOKED_FOR_WORDS)? 'W' : '-',
		(pred->flags & PREDF_INVOKED_MULTI)? 'M' : '-',
		pred->dynamic? (
			(pred->flags & PREDF_GLOBAL_VAR)? 'G' :
			(pred->flags & PREDF_FIXED_FLAG)? 'F' : 'D'
		) : '-');
	pp_predname(pred);
	printf(" of arity %d", pred->arity);
#if 0
	if(pred->dynamic && pred->arity && (pred->dynamic->first_param_use & FPUSE_ONE_TAG)) {
		printf(" always called with #%s as first param", pred->dynamic->first_param_value->word->name);
	}
#endif
	printf("\n");
	for(i = 0; i < pred->arity; i++) {
		if(pred->unbound_in & (1 << i)) {
			printf("\tParameter #%d unbound due to e.g. ", i);
			pp_predname(pred->unbound_in_due_to[i]->predicate);
			printf(" at %s:%d\n",
				FILEPART(pred->unbound_in_due_to[i]->line),
				LINEPART(pred->unbound_in_due_to[i]->line));
		}
	}
	for(i = 0; i < pred->nclause; i++) {
		pp_clause(pred->clauses[i]);
	}
}

struct astnode *expand_macro_body(struct astnode *an, struct clause *def, struct astnode **bindings, int instance, line_t line) {
	char buf[64];
	int i;
	struct astnode *exp;

	if(!an) return 0;

	if(an->kind == AN_VARIABLE && an->word->name[0]) {
		for(i = 0; i < def->predicate->arity; i++) {
			if(def->params[i]->word == an->word) {
				break;
			}
		}
		if(i < def->predicate->arity) {
			exp = deepcopy_astnode(bindings[i], line);
		} else {
			snprintf(buf, sizeof(buf), "%s*%d", an->word->name, instance);
			exp = mkast(AN_VARIABLE);
			exp->line = line;
			exp->word = find_word(buf);
		}
	} else {
		exp = malloc(sizeof(*exp));
		memcpy(exp, an, sizeof(*exp));
		if(line) exp->line = line;
		exp->children = malloc(exp->nchild * sizeof(struct astnode *));
		for(i = 0; i < an->nchild; i++) {
			exp->children[i] = expand_macro_body(an->children[i], def, bindings, instance, line);
		}
	}
	exp->next_in_body = expand_macro_body(an->next_in_body, def, bindings, instance, line);

	return exp;
}

static int count_occurrences(struct astnode *an, struct word *w) {
	int n = 0, i;

	while(an) {
		if(an->kind == AN_VARIABLE
		&& an->word == w) {
			n++;
		}
		for(i = 0; i < an->nchild; i++) {
			n += count_occurrences(an->children[i], w);
		}
		an = an->next_in_body;
	}

	return n;
}

void name_all_anonymous(struct astnode *an) {
	int i;

	while(an) {
		if(an->kind == AN_VARIABLE && !*an->word->name) {
			an->word = fresh_word();
		}
		for(i = 0; i < an->nchild; i++) {
			name_all_anonymous(an->children[i]);
		}
		an = an->next_in_body;
	}
}

struct astnode *expand_macros(struct astnode *an) {
	int i;
	struct astnode **bindings, *exp, *sub;
	struct clause *def;

	if(!an) return 0;

	if((an->kind == AN_RULE || an->kind == AN_NEG_RULE)
	&& (an->predicate->flags & PREDF_MACRO)) {
		def = an->predicate->macrodef;
		bindings = malloc(an->nchild * sizeof(struct astnode *));
		for(i = 0; i < an->nchild; i++) {
			assert(def->params[0]->kind == AN_VARIABLE);
			if(count_occurrences(an->children[i], find_word(""))
			&& count_occurrences(def->body, def->params[0]->word) > 1) {
				bindings[i] = deepcopy_astnode(an->children[i], an->children[i]->line);
				name_all_anonymous(bindings[i]);
			} else {
				bindings[i] = an->children[i];
			}
		}
		macro_instance++;
		exp = mkast((an->kind == AN_RULE)?
			((an->subkind == RULE_MULTI)? AN_BLOCK : AN_FIRSTRESULT) :
			AN_NEG_BLOCK);
		exp->line = an->line;
		exp->nchild = 1;
		exp->children = malloc(sizeof(struct astnode *));
		exp->children[0] = expand_macros(expand_macro_body(def->body, def, bindings, macro_instance, an->line));
		if(exp->kind == AN_FIRSTRESULT) {
			for(sub = exp->children[0]; sub; sub = sub->next_in_body) {
				if(!(sub->kind == AN_RULE && sub->subkind == RULE_SIMPLE)
				&& !(sub->kind == AN_NEG_RULE)) {
					break;
				}
			}
			if(!sub) {
				exp->kind = AN_BLOCK;
			} else {
				exp->word = fresh_word();
			}
		}
		if(exp->kind != AN_FIRSTRESULT
		&& exp->children[0]
		&& !exp->children[0]->next_in_body) {
			if(exp->kind == AN_BLOCK) {
				exp = exp->children[0];
			} else {
				if(exp->children[0]->kind == AN_RULE) {
					exp = exp->children[0];
					exp->kind = AN_NEG_RULE;
				}
			}
		}
		free(bindings);
	} else {
		exp = malloc(sizeof(*exp));
		memcpy(exp, an, sizeof(*exp));
		for(i = 0; i < an->nchild; i++) {
			exp->children[i] = expand_macros(an->children[i]);
		}
	}
	exp->next_in_body = expand_macros(an->next_in_body);

	return exp;
}

int eval_pred(struct predicate *pred, struct astnode **bindings);

int eval_clause(struct clause *cl, struct astnode **outer_bindings, int *cutflag) {
	int i;
	struct evalvar *vars = 0, *v;
	struct astnode **bound_outer = calloc(cl->predicate->arity, sizeof(struct astnode *));
	struct astnode *an, **inner_bindings, *sub;

	for(i = 0; i < cl->predicate->arity; i++) {
		bound_outer[i] = outer_bindings[i];
		if(cl->params[i]->kind == AN_TAG) {
			if(outer_bindings[i]) {
				if(outer_bindings[i]->kind != AN_TAG
				|| outer_bindings[i]->word != cl->params[i]->word) {
					free(bound_outer);
					return 0;
				}
			} else {
				bound_outer[i] = cl->params[i];
			}
		} else if(cl->params[i]->kind == AN_INTEGER) {
			if(outer_bindings[i]) {
				if(outer_bindings[i]->kind != AN_INTEGER
				|| outer_bindings[i]->value != cl->params[i]->value) {
					free(bound_outer);
					return 0;
				}
			} else {
				bound_outer[i] = cl->params[i];
			}
		} else if(cl->params[i]->kind == AN_VARIABLE) {
			v = calloc(1, sizeof(*v));
			v->name = cl->params[i]->word;
			v->pointer = &bound_outer[i];
			v->next = vars;
			vars = v;
		} else {
			report(LVL_ERR, cl->line, "Too complex left-hand side in predicate evaluated at compile time.");
			exit(1);
		}
	}

#if 0
	for(v = vars; v; v = v->next) {
		printf("VAR %s -> ", v->name->name);
		if(*v->pointer) {
			if((*v->pointer)->kind == AN_TAG) {
				printf("#%s\n", (*v->pointer)->word->name);
			} else if((*v->pointer)->kind == AN_INTEGER) {
				printf("%d\n", (*v->pointer)->value);
			}
		} else {
			printf("$\n");
		}
	}
#endif

	for(an = cl->body; an; an = an->next_in_body) {
		if(an->kind == AN_RULE || an->kind == AN_NEG_RULE) {
			if(an->predicate->builtin == BI_FAIL) {
				if(an->kind == AN_RULE) return 0;
			} else if(an->predicate->builtin == BI_OBJECT && an->children[0]->kind == AN_VARIABLE) {
				for(v = vars; v; v = v->next) {
					if(v->name == an->children[0]->word) {
						break;
					}
				}
				if(v
				&& (*v->pointer)
				&& (*v->pointer)->kind == AN_TAG) {
					return (an->kind == AN_RULE);
				} else {
					report(LVL_ERR, cl->line, "Too complex right-hand side in predicate evaluated at compile time (5).");
					exit(1);
				}
			} else if(an->predicate->builtin == BI_IS_ONE_OF && an->children[0]->kind == AN_VARIABLE) {
				for(v = vars; v; v = v->next) {
					if(v->name == an->children[0]->word) {
						break;
					}
				}
				if(v
				&& *v->pointer
				&& (*v->pointer)->kind == AN_TAG) {
					for(sub = an->children[1]; sub && sub->kind == AN_PAIR; sub = sub->children[1]) {
						if(sub->children[0]->kind == AN_VARIABLE) {
							sub = 0;
							break;
						}
						if(sub->children[0]->kind == AN_TAG
						&& sub->children[0]->word == (*v->pointer)->word) {
							break;
						}
					}
					if(sub && sub->kind == AN_EMPTY_LIST) {
						return 0;
					}
					if(!(sub && sub->kind == AN_PAIR)) {
						report(LVL_ERR, cl->line, "Too complex right-hand side in predicate evaluated at compile time (7).");
						exit(1);
					}
				} else {
					report(LVL_ERR, cl->line, "Too complex right-hand side in predicate evaluated at compile time (6).");
					exit(1);
				}
			} else if(an->predicate->special || an->predicate->builtin) {
				report(LVL_ERR, cl->line, "Too complex right-hand side in predicate evaluated at compile time (1).");
				exit(1);
			} else {
				if(an->predicate->dynamic && !(an->predicate->flags & PREDF_FIXED_FLAG)) {
					report(
						LVL_ERR,
						an->line,
						"Initial state of dynamic predicate depends on the state of another dynamic predicate, %s.",
						an->predicate->printed_name);
					// todo print due to what line the present predicate is dynamic
					exit(1);
				}
				inner_bindings = calloc(an->predicate->arity, sizeof(struct astnode *));
				for(i = 0; i < an->predicate->arity; i++) {
					if(an->children[i]->kind == AN_VARIABLE) {
						for(v = vars; v; v = v->next) {
							if(v->name == an->children[i]->word) {
								inner_bindings[i] = *v->pointer;
								break;
							}
						}
					} else if(an->children[i]->kind == AN_PAIR) {
						report(LVL_ERR, cl->line, "Too complex right-hand side in predicate evaluated at compile time (2).");
						exit(1);
					} else {
						inner_bindings[i] = an->children[i];
					}
				}
				if(an->kind == AN_RULE
				&& an->subkind == RULE_MULTI
				&& an->next_in_body
				&& !((an->predicate->flags & PREDF_FIXED_FLAG) && inner_bindings[0])) {
					report(LVL_ERR, cl->line, "Too complex right-hand side in predicate evaluated at compile time (4).");
					exit(1);
				}
				if(eval_pred(an->predicate, inner_bindings)) {
					if(an->kind == AN_NEG_RULE) {
						free(inner_bindings);
						return 0;
					}
					for(i = 0; i < an->predicate->arity; i++) {
						if(an->children[i]->kind == AN_VARIABLE) {
							for(v = vars; v; v = v->next) {
								if(v->name == an->children[i]->word) {
									break;
								}
							}
							if(v) {
								if(inner_bindings[i]
								&& inner_bindings[i]->kind != AN_VARIABLE
								&& inner_bindings[i]->kind != AN_PAIR) {
									if(*v->pointer) {
										if((*v->pointer)->kind != inner_bindings[i]->kind) {
											free(inner_bindings);
											return 0;
										}
										if((*v->pointer)->kind == AN_INTEGER
										&& (*v->pointer)->value != inner_bindings[i]->value) {
											free(inner_bindings);
											return 0;
										}
										if((*v->pointer)->kind == AN_TAG
										&& (*v->pointer)->word != inner_bindings[i]->word) {
											free(inner_bindings);
											return 0;
										}
										if((*v->pointer)->kind == AN_DICTWORD
										&& (*v->pointer)->word != inner_bindings[i]->word) {
											free(inner_bindings);
											return 0;
										}
									} else {
										*v->pointer = inner_bindings[i];
									}
								} else {
									report(LVL_ERR, cl->line, "Expected bound output variable during compile-time evaluation.");
									exit(1);
								}
							} else {
								v = calloc(1, sizeof(*v));
								v->name = an->children[i]->word;
								v->pointer = calloc(1, sizeof(struct astnode *));
								v->next = vars;
								vars = v;
							}
						}
					}
				} else {
					free(inner_bindings);
					if(an->kind == AN_RULE) {
						return 0;
					}
				}
			}
		} else if(an->kind == AN_JUST) {
			*cutflag = 1;
		} else {
			report(LVL_ERR, cl->line, "Too complex right-hand side in predicate evaluated at compile time (3).");
			exit(1);
		}
	}

	for(i = 0; i < cl->predicate->arity; i++) {
		outer_bindings[i] = bound_outer[i];
	}

	free(bound_outer);
	return 1;
}

int eval_pred(struct predicate *pred, struct astnode **bindings) {
	int i;
	int cutflag = 0;

#if 0
	printf("eval_pred ");
	pp_predicate(pred);
	for(i = 0; i < pred->arity; i++) {
		if(bindings[i]) {
			if(bindings[i]->kind == AN_TAG) {
				printf(" #%s", bindings[i]->word->name);
			} else if(bindings[i]->kind == AN_INTEGER) {
				printf(" %d", bindings[i]->value);
			} else {
				assert(0);
				exit(1);
			}
		} else {
			printf(" $");
		}
	}
	printf("\n");
#endif

	for(i = 0; i < pred->nclause && !cutflag; i++) {
		if(eval_clause(pred->clauses[i], bindings, &cutflag)) {
			return 1;
		}
	}

	return 0;
}

void eval_initial_values() {
	int i, j;
	struct predicate *pred, *con;
	struct astnode *bindings[2], *an;
	struct dynamic *dyn;

	for(i = 0; i < npredicate; i++) {
		pred = predicates[i];
		if((dyn = pred->dynamic)) {
			if(pred->arity == 0) {
				dyn->initial_global_flag = eval_pred(pred, 0);
			} else if(pred->arity == 1) {
				if(pred->flags & PREDF_GLOBAL_VAR) {
					if(pred->nclause
					&& !pred->clauses[0]->body
					&& pred->clauses[0]->params[0]->kind != AN_VARIABLE) {
						if(dyn->global_bufsize > 1) {
							// Handle complex initial values at runtime.

							an = mkast(AN_NOW);
							an->nchild = 1;
							an->children = malloc(sizeof(struct astnode *));
							an->children[0] = mkast(AN_RULE);
							an->children[0]->line = pred->clauses[0]->line;
							an->children[0]->predicate = pred;
							an->children[0]->nchild = 1;
							an->children[0]->children = malloc(sizeof(struct astnode *));
							an->children[0]->children[0] = pred->clauses[0]->params[0];

							an->next_in_body = mkast(AN_RULE);
							an->next_in_body->predicate = find_builtin(BI_FAIL);

							con = find_builtin(BI_CONSTRUCTORS);
							con->nclause++;
							con->clauses = realloc(con->clauses, con->nclause * sizeof(struct clause *));
							memmove(con->clauses + 1, con->clauses, (con->nclause - 1) * sizeof(struct clause *));
							con->clauses[0] = calloc(1, sizeof(struct clause));
							con->clauses[0]->predicate = con;
							con->clauses[0]->line = pred->clauses[0]->line;
							con->clauses[0]->body = an;
						} else {
							dyn->initial_global_value = pred->clauses[0]->params[0];
						}
					} else {
						bindings[0] = 0;
						if(eval_pred(pred, bindings)) {
							dyn->initial_global_value = bindings[0];
						}
					}
				} else {
					dyn->initial_flag = calloc(nworldobj, 1);
					for(j = 0; j < nworldobj; j++) {
						bindings[0] = worldobjs[j]->astnode;
						if(eval_pred(pred, bindings)) {
							dyn->initial_flag[j] = 1;
							if(pred->flags & PREDF_FIXED_FLAG) {
								dyn->fixed_flag_count++;
							}
						}
					}
				}
			} else if(pred->arity == 2) {
				dyn->initial_value = calloc(nworldobj, sizeof(struct astnode *));
				for(j = 0; j < nworldobj; j++) {
					bindings[0] = worldobjs[j]->astnode;
					bindings[1] = 0;
					if(eval_pred(pred, bindings)) {
						dyn->initial_value[j] = bindings[1];
					}
				}
			} else {
				report(LVL_ERR, dyn->dyn_due_to_line, "Dynamic predicates have a maximum arity of 2.");
				exit(1);
			}
		}
	}
}

void find_dict_words(struct astnode *an, int include_barewords) {
	int i;

	while(an) {
		switch(an->kind) {
		case AN_DICTWORD:
			an->word->flags |= WORDF_DICT;
			break;
		case AN_BAREWORD:
			if(include_barewords) {
				an->word->flags |= WORDF_DICT;
			}
			break;
		default:
			for(i = 0; i < an->nchild; i++) {
				find_dict_words(an->children[i], include_barewords);
			}
		}
		an = an->next_in_body;
	}
}

void build_dictionary() {
	int i, j, k;
	struct predicate *pred;
	struct word *w;

	for(i = 0; i < npredicate; i++) {
		pred = predicates[i];
		if(pred->flags & PREDF_INVOKED) {
			for(j = 0; j < pred->nclause; j++) {
				for(k = 0; k < pred->arity; k++) {
					find_dict_words(pred->clauses[j]->params[k], 0);
				}
				find_dict_words(pred->clauses[j]->body, !!(pred->flags & PREDF_INVOKED_FOR_WORDS));
			}
		}
		if(pred->flags & PREDF_GLOBAL_VAR) {
			find_dict_words(pred->dynamic->initial_global_value, 0);
		}
		if(pred->arity == 2 && pred->dynamic) {
			for(j = 0; j < nworldobj; j++) {
				find_dict_words(pred->dynamic->initial_value[j], 0);
			}
		}
	}

	for(i = 0; i < BUCKETS; i++) {
		for(w = wordhash[i]; w; w = w->next_in_hash) {
			if(w->flags & WORDF_DICT) {
				backend_add_dict(w);
				//printf("%s\n", w->name);
			}
		}
	}
}

int contains_just(struct astnode *an) {
	int i;

	while(an) {
		if(an->kind == AN_JUST) return 1;
		for(i = 0; i < an->nchild; i++) {
			if(contains_just(an->children[i])) return 1;
		}
		an = an->next_in_body;
	}

	return 0;
}

/* Body is guaranteed to fail, regardless of parameters. Possibly with side-effects. */
int body_fails(struct astnode *an) {
	int i;

	while(an) {
		switch(an->kind) {
		case AN_BLOCK:
		case AN_FIRSTRESULT:
			if(body_fails(an->children[0])) return 1;
			break;
		case AN_NEG_BLOCK:
			if(body_succeeds(an->children[0])) return 1;
			break;
		case AN_RULE:
			if(an->predicate->flags & PREDF_FAIL) return 1;
			break;
		case AN_NEG_RULE:
			if(an->predicate->flags & PREDF_SUCCEEDS) return 1;
			break;
		case AN_OR:
		case AN_SELECT:
			for(i = 0; i < an->nchild; i++) {
				if(!body_fails(an->children[i])) break;
			}
			if(i == an->nchild) return 1;
			break;
		case AN_IF:
			if(body_fails(an->children[1])
			&& body_fails(an->children[2])) {
				return 1;
			}
			// todo also consider conditions with known outcome
			break;
		}
		an = an->next_in_body;
	}

	return 0;
}

/* Body is guaranteed to succeed at least once, regardless of parameters. Possibly with side-effects. */
int body_succeeds(struct astnode *an) {
	int i;

	while(an) {
		switch(an->kind) {
		case AN_BLOCK:
		case AN_FIRSTRESULT:
			if(!body_succeeds(an->children[0])) return 0;
			break;
		case AN_NEG_BLOCK:
			if(!body_fails(an->children[0])) return 0;
			break;
		case AN_RULE:
			if(!(an->predicate->flags & PREDF_SUCCEEDS)) return 0;
			break;
		case AN_NEG_RULE:
			if(!(an->predicate->flags & PREDF_FAIL)) return 0;
			break;
		case AN_OR:
			for(i = 0; i < an->nchild; i++) {
				if(body_succeeds(an->children[i])) break;
			}
			if(i == an->nchild) return 0;
			break;
		case AN_IF:
			if(!body_succeeds(an->children[1])) return 0;
			if(!body_succeeds(an->children[2])) return 0;
			// todo also consider conditions with known outcome
			break;
		case AN_COLLECT_WORDS_CHECK:
			return 0;
			break;
		case AN_SELECT:
			for(i = 0; i < an->nchild; i++) {
				if(!body_succeeds(an->children[i])) return 0;
			}
			break;
		}
		an = an->next_in_body;
	}

	return 1;
}

int frontend(int nfile, char **fname) {
	struct clause *clause, **clause_dest, *first_clause, *cl;
	struct predicate *pred;
	struct astnode *an, **anptr;
	struct word *words[8];
	int fnum, i, j, k, m;
	int flag;
	int linecount = 0, totalwordcount = 0;
 
	words[0] = find_word("or");
	find_predicate(1, words)->special = SP_OR;
	words[0] = find_word("now");
	find_predicate(1, words)->special = SP_NOW;
	words[0] = find_word("just");
	find_predicate(1, words)->special = SP_JUST;
	words[0] = find_word("exhaust");
	find_predicate(1, words)->special = SP_EXHAUST;
	words[0] = find_word("if");
	find_predicate(1, words)->special = SP_IF;
	words[0] = find_word("then");
	find_predicate(1, words)->special = SP_THEN;
	words[0] = find_word("else");
	find_predicate(1, words)->special = SP_ELSE;
	words[0] = find_word("elseif");
	find_predicate(1, words)->special = SP_ELSEIF;
	words[0] = find_word("endif");
	find_predicate(1, words)->special = SP_ENDIF;
	words[0] = find_word("select");
	find_predicate(1, words)->special = SP_SELECT;
	words[0] = find_word("stopping");
	find_predicate(1, words)->special = SP_STOPPING;
	words[0] = find_word("at");
	words[1] = find_word("random");
	find_predicate(2, words)->special = SP_RANDOM;
	words[0] = find_word("purely");
	words[1] = find_word("at");
	words[2] = find_word("random");
	find_predicate(3, words)->special = SP_P_RANDOM;
	words[0] = find_word("then");
	words[1] = find_word("at");
	words[2] = find_word("random");
	find_predicate(3, words)->special = SP_T_RANDOM;
	words[0] = find_word("then");
	words[1] = find_word("purely");
	words[2] = find_word("at");
	words[3] = find_word("random");
	find_predicate(4, words)->special = SP_T_P_RANDOM;
	words[0] = find_word("cycling");
	find_predicate(1, words)->special = SP_CYCLING;
	words[0] = find_word("collect");
	words[1] = 0;
	find_predicate(2, words)->special = SP_COLLECT;
	words[1] = find_word("words");
	find_predicate(2, words)->special = SP_COLLECT_WORDS;
	words[0] = find_word("into");
	words[1] = 0;
	find_predicate(2, words)->special = SP_INTO;
	words[0] = find_word("and");
	words[1] = find_word("check");
	words[2] = 0;
	find_predicate(3, words)->special = SP_AND_CHECK;
	words[0] = find_word("status");
	words[1] = find_word("bar");
	words[2] = 0;
	find_predicate(3, words)->special = SP_STATUSBAR;
	words[0] = find_word("stoppable");
	find_predicate(1, words)->special = SP_STOPPABLE;
	words[0] = find_word("global");
	words[1] = find_word("variable");
	words[2] = 0;
	find_predicate(3, words)->special = SP_GLOBAL_VAR;
	find_predicate(3, words)->flags |= PREDF_META;
	words[3] = 0;
	find_predicate(4, words)->special = SP_GLOBAL_VAR_2;
	find_predicate(4, words)->flags |= PREDF_META;
	words[0] = find_word("generate");
	words[1] = 0;
	words[2] = 0;
	find_predicate(3, words)->special = SP_GENERATE;
	find_predicate(3, words)->flags |= PREDF_META;

	for(i = 0; i < sizeof(builtinspec) / sizeof(*builtinspec); i++) {
		for(j = 0; j < builtinspec[i].nword; j++) {
			words[j] = builtinspec[i].word[j]?
				find_word(builtinspec[i].word[j])
				: 0;
		}
		pred = find_predicate(builtinspec[i].nword, words);
		pred->builtin = builtinspec[i].id;
		pred->flags |= builtinspec[i].predflags | PREDF_INVOKED_MULTI | PREDF_INVOKED_SIMPLE;
		if(pred->flags & PREDF_DYNAMIC) {
			pred->dynamic = calloc(1, sizeof(struct dynamic));
		}
	}

	failpred = find_builtin(BI_FAIL);
	stoppred = find_builtin(BI_STOP);

	nsourcefile = nfile;
	sourcefile = fname;

	clause_dest = &first_clause;
	for(fnum = 0; fnum < nfile; fnum++) {
		f = fopen(fname[fnum], "r");
		if(!f) {
			report(LVL_ERR, 0, "Failed to open \"%s\": %s", fname[fnum], strerror(errno));
			exit(1);
		}

		line = MKLINE(fnum, 1);
		column = 0;
		wordcount = 0;

		status = next_token(&tok, f, PMODE_RULE);
		if(status == 1) {
			report(LVL_ERR, line, "Code must begin in leftmost column.");
			exit(1);
		}

		while(status) {
			if(tok.kind == TOK_TAG) {
				starword = tok.word;
				status = next_token(&tok, f, PMODE_RULE);
				if(status == 1) {
					report(LVL_ERR, line, "Unexpected expression after declaration of current topic.");
					exit(1);
				}
			} else if(tok.kind == TOK_DICTWORD) {
				if(tok.word->name[0]) {
					report(LVL_ERR, line, "Expected ( after @.");
					exit(1);
				}
				if(!next_token(&tok, f, PMODE_RULE)) {
					report(LVL_ERR, line, "Unexpected end of file.");
					exit(1);
				}
				if(tok.kind != '(') {
					report(LVL_ERR, line, "Expected ( after @.");
					exit(1);
				}
				clause = parse_clause(1);
				if(clause->predicate->builtin) {
					report(
						LVL_ERR,
						line,
						"Access predicate definition collides with builtin predicate: %s",
						clause->predicate->printed_name);
					exit(1);
				}
				if(clause->predicate->macrodef) {
					report(
						LVL_ERR,
						line,
						"Multiple access predicate definitions with the same name: @%s",
						clause->predicate->printed_name);
					exit(1);
				}
				for(i = 0; i < clause->predicate->arity; i++) {
					if(clause->params[i]->kind != AN_VARIABLE) {
						report(
							LVL_ERR,
							clause->line,
							"Parameters of access predicate heads must be variables (parameter #%d).",
							i + 1);
						exit(1);
					}
				}
				for(an = clause->body; an; an = an->next_in_body) {
					if(an->kind != AN_RULE
					&& an->kind != AN_NEG_RULE) {
						report(
							LVL_ERR,
							clause->line,
							"Access predicate body must be a simple conjunction of queries.");
						exit(1);
					}
				}
				clause->predicate->macrodef = clause;
				clause->predicate->flags |= PREDF_MACRO;
			} else {
				if(tok.kind == '~') {
					if(!next_token(&tok, f, PMODE_RULE)) {
						report(LVL_ERR, line, "Unexpected end of file.");
						exit(1);
					}
					if(tok.kind != '(') {
						report(LVL_ERR, line, "Expected ( after ~.");
						exit(1);
					}
					clause = parse_clause(0);
					clause->negated = 1;
				} else if(tok.kind == '(') {
					clause = parse_clause(0);
				} else {
					report(LVL_ERR, line, "Bad kind of expression at beginning of line.");
					exit(1);
				}
				if(clause->predicate->builtin
				&& clause->predicate->builtin != BI_HASPARENT
				&& !(clause->predicate->flags & (PREDF_META | PREDF_DEFINABLE_BI))) {
					report(LVL_ERR, line, "Rule definition collides with builtin predicate.");
					exit(1);
				}
				*clause_dest = clause;
				clause_dest = &clause->next_in_source;
			}
		}

		fclose(f);
		report(LVL_INFO, 0, "Word count for \"%s\": %d", fname[fnum], wordcount);

		linecount += LINEPART(line);
		totalwordcount += wordcount;
	}
	*clause_dest = 0;

	report(LVL_INFO, 0, "Total word count: %d", totalwordcount);

	for(clause_dest = &first_clause; (clause = *clause_dest); ) {
		if(clause->predicate->special == SP_GLOBAL_VAR
		|| clause->predicate->special == SP_GLOBAL_VAR_2) {
			if(clause->body
			&& clause->body->kind == AN_RULE
			&& !clause->body->next_in_body
			&& clause->body->predicate->arity == 1) {
				/* presumably of the form (global variable (inner declaration ...)) */
				if(clause->body->children[0]->kind != clause->params[0]->kind
				|| (clause->params[0]->kind == AN_VARIABLE && clause->body->children[0]->word != clause->params[0]->word)) {
					/* somebody is attempting e.g. (global variable $X) (inner declaration $Y) */
					report(LVL_ERR, clause->line, "Syntax error in global variable declaration.");
					exit(1);
				}
				if(clause->body->predicate->flags & PREDF_GLOBAL_VAR) {
					report(LVL_WARN, clause->line, "Multiple declarations of the same global variable.");
				}
				if(!clause->body->predicate->dynamic) {
					clause->body->predicate->dynamic = calloc(1, sizeof(struct dynamic));
					clause->body->predicate->dynamic->dyn_due_to_line = clause->line;
				}
				clause->body->predicate->flags |= PREDF_DYNAMIC | PREDF_GLOBAL_VAR;
				if(clause->predicate->special == SP_GLOBAL_VAR) {
					clause->body->predicate->dynamic->global_bufsize = 1;
				} else {
					if(clause->params[1]->kind == AN_INTEGER
					&& clause->params[1]->value >= 1) {
						clause->body->predicate->dynamic->global_bufsize = clause->params[1]->value;
					} else {
						report(LVL_ERR, clause->line, "The declared size of a global variable must be a positive integer.");
						exit(1);
					}
				}
				if(clause->body->children[0]->kind != AN_VARIABLE) {
					/* (global variable (inner declaration #foo)) so we add a separate rule for the initial value */
					cl = calloc(1, sizeof(*cl));
					cl->predicate = clause->body->predicate;
					cl->params = clause->body->children;
					cl->body = 0;
					cl->line = clause->body->line;
					cl->next_in_source = clause->next_in_source;
					clause->next_in_source = cl;
				}
				clause_dest = &clause->next_in_source;
			} else {
				report(LVL_ERR, clause->line, "Syntax error in global variable declaration.");
				exit(1);
			}
		} else if(clause->predicate->special == SP_GENERATE) {
			if(clause->body
			&& clause->body->kind == AN_RULE
			&& !clause->body->next_in_body
			&& clause->body->predicate->arity) {
				struct clause **cld = clause_dest;

				/* presumably of the form (generate N of (inner declaration ...)) */
				if(clause->params[0]->kind != AN_INTEGER
				|| clause->body->children[0]->kind != clause->params[1]->kind
				|| (clause->params[1]->kind == AN_VARIABLE && clause->body->children[0]->word != clause->params[1]->word)) {
					/* somebody is attempting e.g. (generate $X of $Y) (inner declaration $Z) */
					report(LVL_ERR, clause->line, "Syntax error in (generate $ $) declaration.");
					exit(1);
				}
				for(i = 0; i < clause->params[0]->value; i++) {
					char buf[32];

					cl = calloc(1, sizeof(*cl));
					cl->predicate = clause->body->predicate;
					cl->params = malloc(cl->predicate->arity * sizeof(struct astnode *));
					cl->params[0] = mkast(AN_TAG);
					snprintf(buf, sizeof(buf), "%d", nworldobj + 1);
					cl->params[0]->word = find_word(buf);
					create_worldobj(cl->params[0]->word);
					for(j = 1; j < clause->body->predicate->arity; j++) {
						cl->params[j] = deepcopy_astnode(clause->body->children[j], clause->line);
					}
					cl->line = clause->line;
					*cld = cl;
					cld = &cl->next_in_source;
				}
				*cld = clause->next_in_source;
			} else {
				report(LVL_ERR, clause->line, "Syntax error in (generate $ $) declaration.");
				exit(1);
			}
		} else if(clause->predicate->flags & PREDF_MACRO) {
			struct clause **cld = clause_dest;
			an = expand_macro_body(
				clause->predicate->macrodef->body,
				clause->predicate->macrodef,
				clause->params,
				++macro_instance,
				clause->line);
			for(; an; an = an->next_in_body) {
				if(an->kind == AN_RULE || an->kind == AN_NEG_RULE) {
					cl = calloc(1, sizeof(*cl));
					cl->predicate = an->predicate;
					cl->params = an->children;
					cl->body = deepcopy_astnode(clause->body, clause->line);
					cl->line = clause->line;
					cl->negated = (an->kind == AN_NEG_RULE);
					*cld = cl;
					cld = &cl->next_in_source;
				} else {
					report(LVL_ERR, clause->line, "Access predicate must expand into a conjunction of queries.");
					exit(1);
				}
			}
			*cld = clause->next_in_source;
		} else {
			add_clause(clause, clause->predicate);
			clause->body = expand_macros(clause->body);
			clause_dest = &clause->next_in_source;
		}
	}

	for(i = 0; i < npredicate; i++) {
		pred = predicates[i];
		for(j = 0; j < pred->nclause; j++) {
			if(pred->clauses[j]->negated) {
				for(anptr = &pred->clauses[j]->body; *anptr; anptr = &(*anptr)->next_in_body);
				*anptr = mkast(AN_JUST);
				(*anptr)->line = pred->clauses[j]->line;
				(*anptr)->next_in_body = an = mkast(AN_RULE);
				an->line = pred->clauses[j]->line;
				an->predicate = failpred;
				pred->flags |= PREDF_CONTAINS_JUST;
			} else {
				if(!(pred->flags & PREDF_CONTAINS_JUST)
				&& contains_just(pred->clauses[j]->body)) {
					pred->flags |= PREDF_CONTAINS_JUST;
				}
			}
		}
	}

	for(i = 0; i < npredicate; i++) {
		pred = predicates[i];
		for(j = 0; j < pred->nclause; j++) {
			find_dynamic(pred->clauses[j]->body, pred->clauses[j]->line);
		}
	}

#if 0
	for(i = 0; i < npredicate; i++) {
		if(!predicates[i]->special
		&& !(predicates[i]->flags & PREDF_MACRO)
		&& !(predicates[i]->flags & PREDF_DYNAMIC)) {
			printf("%s %s\n",
				(predicates[i]->flags & PREDF_CONTAINS_JUST)? "J" : " ",
				predicates[i]->printed_name);
		}
	}
#endif

	pred = find_builtin(BI_CONSTRUCTORS);
	cl = calloc(1, sizeof(*cl));
	cl->predicate = pred;
	cl->body = mkast(AN_RULE);
	cl->body->subkind = RULE_SIMPLE;
	cl->body->predicate = find_builtin(BI_PROGRAM_ENTRY);
	add_clause(cl, pred);

	trace_invocations();

	if(!(pred = find_builtin(BI_ERROR_ENTRY))->nclause) {
		cl = calloc(1, sizeof(*cl));
		cl->predicate = pred;
		cl->params = calloc(1, sizeof(struct astnode *));
		cl->params[0] = mkast(AN_VARIABLE);
		cl->params[0]->word = find_word("");
		cl->body = mkast(AN_RULE);
		cl->body->subkind = RULE_SIMPLE;
		cl->body->predicate = find_builtin(BI_QUIT);
		add_clause(cl, pred);
	}

	if(!(pred = find_builtin(BI_PROGRAM_ENTRY))->nclause) {
		cl = calloc(1, sizeof(*cl));
		cl->predicate = pred;
		cl->body = mkast(AN_RULE);
		cl->body->subkind = RULE_SIMPLE;
		cl->body->predicate = find_builtin(BI_QUIT);
		add_clause(cl, pred);
	}

	for(i = 0; i < npredicate; i++) {
		pred = predicates[i];
		if(!pred->special
		&& !(pred->builtin && !(pred->flags & PREDF_DEFINABLE_BI))
		&& !pred->dynamic
		&& (pred->flags & PREDF_INVOKED)
		&& !(pred->nclause)) {
			report(
				LVL_WARN,
				pred->invoked_at_line,
				"Predicate %s is invoked but there is no matching rule.",
				pred->printed_name);
			pred->flags |= PREDF_FAIL;
		}
	}

	do {
		flag = 0;
		for(i = 0; i < npredicate; i++) {
			pred = predicates[i];
			for(j = 0; j < pred->nclause; j++) {
				if(!pred->special
				&& !(pred->builtin && !(pred->flags & PREDF_DEFINABLE_BI))
				&& !pred->dynamic
				&& pred->clauses[j]->body
				&& pred->clauses[j]->body->kind == AN_RULE
				&& (pred->clauses[j]->body->predicate->flags & PREDF_FAIL)) {
					memmove(pred->clauses + j, pred->clauses + j + 1, (pred->nclause - j - 1) * sizeof(struct clause *));
					pred->nclause--;
					j--;
					if(!pred->nclause) {
						pred->flags |= PREDF_FAIL;
						flag = 1;
					}
				}
			}
		}
	} while(flag);

	find_fixed_flags();

	eval_initial_values();

	build_dictionary();

	do {
		flag = 0;
		for(i = 0; i < npredicate; i++) {
			pred = predicates[i];
			if((!pred->builtin || (pred->flags & PREDF_DEFINABLE_BI))
			&& !pred->dynamic
			&& !(pred->flags & PREDF_SUCCEEDS)) {
				for(j = 0; j < pred->nclause; j++) {
					for(k = 0; k < pred->arity; k++) {
						if(pred->clauses[j]->params[k]->kind != AN_VARIABLE) break;
						if(pred->clauses[j]->params[k]->word->name[0]) {
							for(m = 0; m < k; m++) {
								if(pred->clauses[j]->params[k]->word
								== pred->clauses[j]->params[m]->word) {
									break;
								}
							}
							if(m < k) break;
						}
					}
					if(k == pred->arity) {
						if(body_succeeds(pred->clauses[j]->body)) {
#if 0
							printf("Succeeds: ");
							pp_predname(pred);
							printf("\n");
#endif
							pred->flags |= PREDF_SUCCEEDS;
							flag = 1;
							if(!(pred->flags & PREDF_INVOKED_MULTI)) {
								pred->nclause = j + 1;
							}
							break;
						}
					}
					if(contains_just(pred->clauses[j]->body)) break;
				}
			}
		}
	} while(flag);

	if(verbose >= 3) {
		for(i = 0; i < npredicate; i++) {
			if(!predicates[i]->special
			&& !(predicates[i]->flags & PREDF_MACRO)) {
				pp_predicate(predicates[i]);
			}
		}
	}

#if 0
	for(i = 0; i < nworldobj; i++) {
		printf("%5d %s\n", i + 1, worldobjs[i]->astnode->word->name);
	}
#endif

#if 0
	for(i = 0; i < npredicate; i++) {
		pred = predicates[i];
		if(pred->flags & PREDF_FIXED_FLAG) {
			printf("Count %d: ", pred->dynamic->fixed_flag_count);
			pp_predname(pred);
			printf("\n");
		}
	}
#endif

#if 0
	for(i = 0; i < npredicate; i++) {
		struct dynamic *dyn;
		pred = predicates[i];
		if((dyn = pred->dynamic)) {
			pp_predicate(pred);
			if(pred->arity == 0) {
				printf("\tInitially %s\n", dyn->initial_global_flag? "true" : "false");
			} else if(pred->arity == 1) {
				if(pred->flags & PREDF_GLOBAL_VAR) {
					printf("\tInitially ");
					if(dyn->initial_global_value) {
						if(dyn->initial_global_value->kind == AN_TAG) {
							printf("#%s\n", dyn->initial_global_value->word->name);
						} else if(dyn->initial_global_value->kind == AN_INTEGER) {
							printf("%d\n", dyn->initial_global_value->value);
						} else {
							printf("???\n");
						}
					} else {
						printf("-\n");
					}
				} else {
					printf("\tInitially true:\n");
					for(j = 0; j < nworldobj; j++) {
						if(dyn->initial_flag[j]) printf("\t\t%s\n", worldobjs[j]->astnode->word->name);
					}
					printf("\tInitially false:\n");
					for(j = 0; j < nworldobj; j++) {
						if(!dyn->initial_flag[j]) printf("\t\t%s\n", worldobjs[j]->astnode->word->name);
					}
				}
			} else {
				for(j = 0; j < nworldobj; j++) {
					printf("\t#%-20s ", worldobjs[j]->astnode->word->name);
					if(dyn->initial_value[j]) {
						if(dyn->initial_value[j]->kind == AN_TAG) {
							printf("#%s\n", dyn->initial_value[j]->word->name);
						} else if(dyn->initial_value[j]->kind == AN_INTEGER) {
							printf("%d\n", dyn->initial_value[j]->value);
						} else {
							printf("???\n");
						}
					} else {
						printf("-\n");
					}
				}
			}
		}
	}
#endif

#if 0
	for(i = 0; i < npredicate; i++) {
		pred = predicates[i];
		if(pred->builtin != BI_HASPARENT) {
			struct dynamic *dyn = pred->dynamic;
			if(dyn) {
				if(dyn->linkage_flags & (LINKF_LIST | LINKF_CLEAR)) {
					printf("Needs dynamic linkage: ");
					pp_predname(pred);
					printf(" (due to %s:%d)\n",
						FILEPART(dyn->linkage_due_to_line),
						LINEPART(dyn->linkage_due_to_line));
				}
			}
		}
	}
#endif

#if 0
	for(i = 0; i < npredicate; i++) {
		pred = predicates[i];
		if(!pred->special) {
			printf("%c %c ",
				(pred->flags & PREDF_INVOKED_SIMPLE)? 'S' : '-',
				(pred->flags & PREDF_INVOKED_MULTI)? 'M' : '-');
			pp_predname(pred);
			printf("\n");
			if(!pred->builtin
			&& (pred->flags & PREDF_INVOKED_SIMPLE)
			&& (pred->flags & PREDF_INVOKED_MULTI)) {
				printf("\tsimple because of %s:%d\n",
					FILEPART(pred->simple_due_to_line),
					LINEPART(pred->simple_due_to_line));
			}
		}
	}
#endif

	return linecount;
}
