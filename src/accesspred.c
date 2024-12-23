#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "arena.h"
#include "ast.h"
#include "report.h"
#include "accesspred.h"

static int macro_instance = 0;

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

void name_all_anonymous(struct astnode *an, struct program *prg) {
	int i;

	while(an) {
		if(an->kind == AN_VARIABLE && !*an->word->name) {
			an->word = fresh_word(prg);
		}
		for(i = 0; i < an->nchild; i++) {
			name_all_anonymous(an->children[i], prg);
		}
		an = an->next_in_body;
	}
}

struct astnode *expand_macro_body(struct astnode *an, struct clause *def, struct astnode **bindings, int instance, line_t line, struct program *prg, struct arena *arena) {
	char buf[64];
	int i;
	struct astnode *exp;

	if(!an) return 0;

	if(!instance) {
		instance = ++macro_instance;
	}

	if(an->kind == AN_VARIABLE && an->word->name[0]) {
		for(i = 0; i < def->predicate->arity; i++) {
			if(def->params[i]->word == an->word) {
				break;
			}
		}
		if(i < def->predicate->arity) {
			exp = deepcopy_astnode(bindings[i], arena, line);
		} else {
			snprintf(buf, sizeof(buf), "%s*%d", an->word->name, instance);
			exp = mkast(AN_VARIABLE, 0, arena, line);
			exp->word = find_word(prg, buf);
		}
	} else {
		exp = arena_alloc(arena, sizeof(*exp));
		memcpy(exp, an, sizeof(*exp));
		if(line) exp->line = line;
		exp->children = arena_alloc(arena, exp->nchild * sizeof(struct astnode *));
		for(i = 0; i < an->nchild; i++) {
			exp->children[i] = expand_macro_body(an->children[i], def, bindings, instance, line, prg, arena);
		}
	}
	exp->next_in_body = expand_macro_body(an->next_in_body, def, bindings, instance, line, prg, arena);

	return exp;
}

struct astnode *expand_macros(struct astnode *an, struct program *prg, struct arena *arena) {
	int i;
	struct astnode **bindings, *exp, *sub;
	struct clause *def;

	if(!an) return 0;

	if((an->kind == AN_RULE || an->kind == AN_NEG_RULE)
	&& (an->predicate->pred->flags & PREDF_MACRO)) {
		def = an->predicate->pred->macrodef;
		bindings = malloc(an->nchild * sizeof(struct astnode *));
		for(i = 0; i < an->nchild; i++) {
			assert(def->params[0]->kind == AN_VARIABLE);
			if(count_occurrences(an->children[i], find_word(prg, ""))
			&& count_occurrences(def->body, def->params[0]->word) > 1) {
				bindings[i] = deepcopy_astnode(an->children[i], arena, an->children[i]->line);
				name_all_anonymous(bindings[i], prg);
			} else {
				bindings[i] = an->children[i];
			}
		}
		macro_instance++;
		exp = mkast(
			(an->kind == AN_RULE)?
				((an->subkind == RULE_MULTI)? AN_BLOCK : AN_FIRSTRESULT) :
				AN_NEG_BLOCK,
			1,
			arena,
			an->line);
		exp->children[0] = expand_macros(
			expand_macro_body(def->body, def, bindings, macro_instance, an->line, prg, arena),
			prg,
			arena);
		if(exp->kind == AN_FIRSTRESULT) {
			for(sub = exp->children[0]; sub; sub = sub->next_in_body) {
				if(!(sub->kind == AN_RULE && sub->subkind == RULE_SIMPLE)
				&& !(sub->kind == AN_NEG_RULE)) {
					break;
				}
			}
			if(!sub) {
				exp->kind = AN_BLOCK;
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
		exp = arena_alloc(arena, sizeof(*exp));
		memcpy(exp, an, sizeof(*exp));
		for(i = 0; i < an->nchild; i++) {
			exp->children[i] = expand_macros(an->children[i], prg, arena);
		}
	}
	exp->next_in_body = expand_macros(an->next_in_body, prg, arena);

	return exp;
}

static int accesspred_body_has_cycle(struct astnode *an) {
	int i;

	while(an) {
		if(an->kind == AN_RULE || an->kind == AN_NEG_RULE) {
			if(an->predicate->pred->flags & PREDF_MACRO) {
				if(accesspred_has_cycle(an->predicate->pred)) {
					return 1;
				}
			}
		} else {
			for(i = 0; i < an->nchild; i++) {
				if(accesspred_body_has_cycle(an->children[i])) {
					return 1;
				}
			}
		}
		an = an->next_in_body;
	}

	return 0;
}

int accesspred_has_cycle(struct predicate *pred) {
	assert(pred->flags & PREDF_MACRO);

	if(pred->flags & PREDF_VISITED) {
		return 1;
	}

	pred->flags |= PREDF_VISITED;
	if(accesspred_body_has_cycle(pred->macrodef->body)) {
		pred->flags &= ~PREDF_VISITED;
		return 1;
	}
	pred->flags &= ~PREDF_VISITED;

	return 0;
}
