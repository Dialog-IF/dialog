#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "arena.h"
#include "ast.h"
#include "report.h"

static unsigned int hashfunc(char *name) {
	int i;
	unsigned int h = 0;

	for(i = 0; name[i]; i++) {
		h += (unsigned) name[i] * (1 + i);
	}
	h %= WORDBUCKETS;
	return h;
}

struct word *find_word(struct program *prg, char *name) {
	unsigned int h = hashfunc(name);
	struct word *w;

	for(w = prg->wordhash[h]; w; w = w->next_in_hash) {
		if(!strcmp(w->name, name)) return w;
	}

	w = arena_calloc(&prg->arena, sizeof(*w));
	w->name = arena_strdup(&prg->arena, name);
	w->word_id = prg->nword++;
	if(prg->nword > prg->nalloc_word) {
		prg->nalloc_word = prg->nword * 2 + 8;
		prg->allwords = realloc(prg->allwords, prg->nalloc_word * sizeof(struct word *));
	}
	prg->allwords[w->word_id] = w;
	w->next_in_hash = prg->wordhash[h];
	prg->wordhash[h] = w;

	return w;
}

struct word *find_word_nocreate(struct program *prg, char *name) {
	unsigned int h = hashfunc(name);
	struct word *w;

	for(w = prg->wordhash[h]; w; w = w->next_in_hash) {
		if(!strcmp(w->name, name)) return w;
	}

	return 0;
}

void ensure_dict_word(struct program *prg, struct word *w) {
	if(!(w->flags & WORDF_DICT)) {
		w->flags |= WORDF_DICT;
		w->dict_id = prg->ndictword;
		if(prg->ndictword >= prg->nalloc_dictword) {
			prg->nalloc_dictword = prg->ndictword * 2 + 8;
			prg->dictwordnames = realloc(prg->dictwordnames, prg->nalloc_dictword * sizeof(struct word *));
		}
		prg->dictwordnames[prg->ndictword++] = w;
	}
}

struct word *fresh_word(struct program *prg) {
	char buf[16];

	snprintf(buf, sizeof(buf), "*%d", prg->nextfresh++);
	return find_word(prg, buf);
}

void pred_clear(struct predname *predname) {
	pred_release(predname->pred);

	predname->pred = calloc(1, sizeof(struct predicate));
	predname->pred->refcount = 1;
	predname->total_refcount++;
	arena_init(&predname->pred->arena, 4096);

	predname->pred->normal_entry = -1;
	predname->pred->initial_value_entry = -1;

	predname->pred->unbound_in_due_to = arena_calloc(&predname->pred->arena, predname->arity * sizeof(struct clause *));
	predname->pred->unbound_out_due_to = arena_calloc(&predname->pred->arena, predname->arity * sizeof(struct clause *));

	predname->pred->predname = predname;
}

struct predname *find_predicate(struct program *prg, int nword, struct word **words) {
	int i, j, len;
	struct predname *predname;

	for(i = 0; i < prg->npredicate; i++) {
		if(prg->predicates[i]->nword == nword) {
			for(j = 0; j < nword; j++) {
				if(prg->predicates[i]->words[j] != words[j]) break;
			}
			if(j == nword) {
				return prg->predicates[i];
			}
		}
	}

	predname = arena_calloc(&prg->arena, sizeof(*predname));
	predname->nword = nword;
	predname->words = arena_alloc(&prg->arena, nword * sizeof(struct word *));
	len = 2;
	for(j = 0; j < nword; j++) {
		if(j) len++;
		predname->words[j] = words[j];
		if(words[j]) {
			len += strlen(words[j]->name);
		} else {
			predname->arity++;
			len++;
		}
	}
	predname->printed_name = arena_alloc(&prg->arena, len + 1);
	i = 0;
	predname->printed_name[i++] = '(';
	for(j = 0; j < nword; j++) {
		if(j) predname->printed_name[i++] = ' ';
		if(words[j]) {
			strcpy(predname->printed_name + i, words[j]->name);
			i += strlen(words[j]->name);
		} else {
			predname->printed_name[i++] = '$';
		}
	}
	predname->printed_name[i++] = ')';
	predname->printed_name[i] = 0;
	assert(i == len);

	predname->pred_id = prg->npredicate;

	predname->dyn_id = DYN_NONE;
	predname->dyn_var_id = DYN_NONE;

	pred_clear(predname);

	prg->predicates = realloc(prg->predicates, (prg->npredicate + 1) * sizeof(struct predname *));
	prg->predicates[prg->npredicate++] = predname;

	return predname;
}

struct predname *find_builtin(struct program *prg, int id) {
	int i;

	for(i = 0; i < prg->npredicate; i++) {
		if(prg->predicates[i]->builtin == id) {
			return prg->predicates[i];
		}
	}
	printf("%d\n", id);
	assert(0); exit(1);
}

struct astnode *mkast(int kind, int nchild, struct arena *arena, line_t line) {
	struct astnode *an = arena_calloc(arena, sizeof(*an));
	an->kind = kind;
	an->line = line;
	an->nchild = nchild;
	an->children = arena_alloc(arena, nchild * sizeof(struct astnode *));
	return an;
}

struct astnode *deepcopy_astnode(struct astnode *an, struct arena *arena, line_t line) {
	struct astnode *a;
	int i;

	if(!an) return 0;

	// Create new copies of the astnode and its children and next_in_body,
	// but don't duplicate words and predicates.

	a = arena_alloc(arena, sizeof(*a));
	memcpy(a, an, sizeof(*a));
	if(line) a->line = line;
	a->children = arena_alloc(arena, a->nchild * sizeof(struct astnode *));
	for(i = 0; i < a->nchild; i++) {
		a->children[i] = deepcopy_astnode(an->children[i], arena, line);
	}
	a->next_in_body = deepcopy_astnode(an->next_in_body, arena, line);

	return a;
}

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
		if(an->unbound) printf(":");
		printf("$%s", an->word->name);
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
	case AN_DETERMINE_OBJECT:
		printf("(determine object ");
		pp_expr(an->children[0]);
		printf(") ");
		pp_body(an->children[1]);
		printf(" (from words) ");
		pp_body(an->children[2]);
		printf(" (matching all of ");
		pp_expr(an->children[3]);
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
	if(cl->structure) printf("\tstructure: %s\n", cl->structure);
}

void pp_predicate(struct predname *predname, struct program *prg) {
	int i;
	struct predicate *pred = predname->pred;

	printf("Predicate %c%c%c%c%c ",
		(pred->flags & PREDF_INVOKED_BY_PROGRAM)? 'P' : '-',
		(pred->flags & PREDF_INVOKED_NORMALLY)? 'N' : '-',
		(pred->flags & PREDF_INVOKED_FOR_WORDS)? 'W' : '-',
		(pred->flags & PREDF_INVOKED_MULTI)? 'M' : '-',
		pred->dynamic? (
			(pred->flags & PREDF_GLOBAL_VAR)? 'G' :
			(pred->flags & PREDF_FIXED_FLAG)? 'F' : 'D'
		) : '-');
	printf("%s of arity %d\n", predname->printed_name, predname->arity);
	if(prg->optflags & OPTF_BOUND_PARAMS) {
		for(i = 0; i < predname->arity; i++) {
			if(pred->unbound_in & (1 << i)) {
				printf("\tIncoming parameter #%d can be unbound because of ", i + 1);
				printf("%s at %s:%d.\n",
					pred->unbound_in_due_to[i]->predicate->printed_name,
					FILEPART(pred->unbound_in_due_to[i]->line),
					LINEPART(pred->unbound_in_due_to[i]->line));
				if(pred->unbound_out & (1 << i)) {
					printf("\tIt can remain unbound after a successful query");
					if(pred->unbound_out_due_to[i]) {
						printf(" because of the clause at %s:%d",
							FILEPART(pred->unbound_out_due_to[i]->line),
							LINEPART(pred->unbound_out_due_to[i]->line));
					}
					printf(".\n");
				}
			}
		}
	}
	for(i = 0; i < pred->nclause; i++) {
		pp_clause(pred->clauses[i]);
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

struct program *new_program() {
	struct program *prg = calloc(1, sizeof(*prg));

	arena_init(&prg->arena, 4096);
	prg->nextfresh = 1;

	return prg;
}

void pred_claim(struct predicate *pred) {
	if(pred) {
		assert(pred->refcount);
		pred->refcount++;
		pred->predname->total_refcount++;
	}
}

void pred_release(struct predicate *pred) {
	if(pred) {
		pred->predname->total_refcount--;
		if(!--pred->refcount) {
			arena_free(&pred->arena);
			free(pred->dynamic);
			free(pred->clauses);
			free(pred->wordmaps);
			free(pred);
		}
	}
}

void free_program(struct program *prg) {
	int i;

	for(i = 0; i < prg->npredicate; i++) {
		pred_release(prg->predicates[i]->old_pred);
		pred_release(prg->predicates[i]->pred);
		if(prg->predicates[i]->total_refcount) {
			report(LVL_WARN, 0, "Internal compiler error: %s has %d remaining reference(s)",
				prg->predicates[i]->printed_name,
				prg->predicates[i]->total_refcount);
		}
		free(prg->predicates[i]->fixedvalues);
	}
	free(prg->predicates);
	free(prg->allwords);
	free(prg->worldobjnames);
	free(prg->dictwordnames);
	free(prg->globalflagpred);
	free(prg->globalvarpred);
	free(prg->objflagpred);
	free(prg->objvarpred);
	free(prg->select);
	arena_free(&prg->arena);
	arena_free(&prg->endings_arena);
	free(prg);
}

void create_worldobj(struct program *prg, struct word *w) {
	if(!(w->flags & WORDF_TAG)) {
		prg->worldobjnames = realloc(prg->worldobjnames, (prg->nworldobj + 1) * sizeof(struct word *));
		prg->worldobjnames[prg->nworldobj] = w;
		w->flags |= WORDF_TAG;
		w->obj_id = prg->nworldobj;
		prg->nworldobj++;
	}
}
