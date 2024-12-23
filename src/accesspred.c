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

static void name_all_anonymous(struct astnode *an, struct program *prg) {
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

static void count_vars(struct astnode *an, int *count, struct clause *cl) {
	int i;

	if(!an) return;

	if(an->kind == AN_VARIABLE && an->word->name[0]) {
		i = findvar(cl, an->word);
		assert(i >= 0 && i < cl->nvar);
		count[i]++;
	}

	for(i = 0; i < an->nchild; i++) {
		count_vars(an->children[i], count, cl);
	}
}

int accesspred_valid_def(struct clause *cl, struct program *prg) {
	struct astnode *an;
	int i;

	if(cl->predicate->builtin) {
		report(
			LVL_ERR,
			cl->line,
			"Access predicate definition collides with built-in predicate: %s",
			cl->predicate->printed_name);
		return 0;
	}

	for(an = cl->body; an; an = an->next_in_body) {
		if(an->kind != AN_RULE
		&& an->kind != AN_NEG_RULE) {
			report(
				LVL_ERR,
				cl->line,
				"Access predicate body must be a simple conjunction of queries.");
			return 0;
		}
	}

	analyse_clause(prg, cl, 1);

	int count[cl->nvar];

	memset(count, 0, cl->nvar * sizeof(int));
	for(i = 0; i < cl->predicate->arity; i++) {
		count_vars(cl->params[i], count, cl);
	}

	for(i = 0; i < cl->nvar; i++) {
		if(count[i] > 1) {
			report(
				LVL_ERR,
				cl->line,
				"Variable $%s appears more than once in access predicate rule header.",
				cl->varnames[i]->name);
			return 0;
		}
	}

	return 1;
}

static int match_template_param(struct astnode *formal, struct astnode *actual) {
	for(;;) {
		if(formal->kind == AN_VARIABLE) {
			return 1;
		} else if(formal->kind == AN_PAIR) {
			if(actual->kind == AN_PAIR) {
				if(!match_template_param(formal->children[0], actual->children[0])) {
					return 0;
				}
				formal = formal->children[1];
				actual = actual->children[1];
			} else {
				return 0;
			}
		} else {
			return astnode_equals(formal, actual);
		}
	}
}

static void accesspred_bind_expr(
	struct astnode *formal,
	struct astnode *actual,
	struct program *prg,
	struct arena *arena,
	struct clause *def,
	struct astnode **table)
{
	struct astnode *an;

	for(;;) {
		if(formal->kind == AN_VARIABLE && formal->word->name[0]) {
			if(count_occurrences(actual, find_word(prg, ""))
			&& count_occurrences(def->body, formal->word) > 1) {
				an = deepcopy_astnode(actual, arena, actual->line);
				name_all_anonymous(an, prg);
			} else {
				an = actual;
			}
			table[findvar(def, formal->word)] = an;
			break;
		} else if(formal->kind == AN_PAIR) {
			assert(actual->kind == AN_PAIR);
			accesspred_bind_expr(formal->children[0], actual->children[0], prg, arena, def, table);
			formal = formal->children[1];
			actual = actual->children[1];
		} else {
			break;
		}
	}
}

struct clause *accesspred_find_def(struct predicate *pred, struct astnode **params) {
	int i, j;
	struct clause *def;

	assert(pred->nmacrodef);
	for(i = 0; i < pred->nmacrodef; i++) {
		def = pred->macrodefs[i];
		for(j = 0; j < pred->predname->arity; j++) {
			if(!match_template_param(def->params[j], params[j])) {
				break;
			}
		}
		if(j == pred->predname->arity) {
			return def;
		}
	}

	return 0;
}

void accesspred_bind_vars(struct clause *def, struct astnode **params, struct astnode **table, struct program *prg, struct arena *arena) {
	int i;

	for(i = 0; i < def->predicate->arity; i++) {
		accesspred_bind_expr(def->params[i], params[i], prg, arena, def, table);
	}
}

struct astnode *expand_macro_body(
	struct astnode *an,
	struct clause *def,
	struct astnode **bindings,
	int instance,
	line_t line,
	struct program *prg,
	struct clause *container)
{
	char buf[64];
	int i;
	struct astnode *exp;

	if(!an) return 0;

	if(!instance) {
		if(container->macro_instance >= 255) {
			report(LVL_ERR, line, "Too many nested access predicate invocations. Cyclic definition?");
			prg->errorflag = 1;
			return 0;
		}
		instance = ++container->macro_instance;
	}

	if(an->kind == AN_VARIABLE && an->word->name[0]) {
		i = findvar(def, an->word);
		if(bindings[i]) {
			exp = deepcopy_astnode(bindings[i], container->arena, line);
		} else {
			snprintf(buf, sizeof(buf), "%s*%d", an->word->name, instance);
			exp = mkast(AN_VARIABLE, 0, container->arena, line);
			exp->word = find_word(prg, buf);
		}
	} else {
		exp = arena_alloc(container->arena, sizeof(*exp));
		memcpy(exp, an, sizeof(*exp));
		if(line) exp->line = line;
		exp->children = arena_alloc(container->arena, exp->nchild * sizeof(struct astnode *));
		for(i = 0; i < an->nchild; i++) {
			exp->children[i] = expand_macro_body(an->children[i], def, bindings, instance, line, prg, container);
		}
	}
	exp->next_in_body = expand_macro_body(an->next_in_body, def, bindings, instance, line, prg, container);

	return exp;
}

struct astnode *expand_macros(struct astnode *an, struct program *prg, struct clause *container) {
	int i;
	struct astnode **bindings, *exp, *sub;
	struct clause *def;

	if(!an) return 0;

	if((an->kind == AN_RULE || an->kind == AN_NEG_RULE)
	&& (an->predicate->pred->flags & PREDF_MACRO)) {
		def = accesspred_find_def(an->predicate->pred, an->children);
		if(!def) {
			report(LVL_ERR, an->line, "Couldn't find a matching access predicate definition for @%s.", an->predicate->printed_name);
			printf("Tried to match: ");
			pp_expr(an);
			printf("\n");
			prg->errorflag = 1;
			return an;
		}
		bindings = calloc(def->nvar, sizeof(struct astnode *));
		accesspred_bind_vars(def, an->children, bindings, prg, container->arena);
		exp = mkast(
			(an->kind == AN_RULE)?
				((an->subkind == RULE_MULTI)? AN_BLOCK : AN_FIRSTRESULT) :
				AN_NEG_BLOCK,
			1,
			container->arena,
			an->line);
		exp->children[0] = expand_macros(
			expand_macro_body(def->body, def, bindings, 0, an->line, prg, container),
			prg,
			container);
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
		exp = arena_alloc(container->arena, sizeof(*exp));
		memcpy(exp, an, sizeof(*exp));
		if(an->nchild) {
			exp->children = arena_alloc(container->arena, an->nchild * sizeof(struct astnode *));
			for(i = 0; i < an->nchild; i++) {
				exp->children[i] = expand_macros(an->children[i], prg, container);
			}
		}
	}
	exp->next_in_body = expand_macros(an->next_in_body, prg, container);

	return exp;
}
