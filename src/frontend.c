#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "arena.h"
#include "ast.h"
#include "accesspred.h"
#include "frontend.h"
#include "compile.h"
#include "eval.h"
#include "parse.h"
#include "report.h"
#include "unicode.h"

struct predlist {
	struct predlist		*next;
	struct predicate	*pred;
};

char **sourcefile;
int nsourcefile;

static struct predicate **tracequeue;
static int tracequeue_r, tracequeue_w;

struct specialspec {
	int id;
	int prednameflags;
	int nword;
	char *word[8];
} specialspec[] = {
	{SP_RANDOM,		0,				2,	{"at", "random"}},
	{SP_COLLECT,		0,				2,	{"collect", 0}},
	{SP_COLLECT_WORDS,	0,				2,	{"collect", "words"}},
	{SP_ACCUMULATE,		0,				2,	{"accumulate", 0}},
	{SP_CYCLING,		0,				1,	{"cycling"}},
	{SP_DETERMINE_OBJECT,	0,				3,	{"determine", "object", 0}},
	{SP_DIV,		0,				2,	{"div", 0}},
	{SP_ENDIF,		0,				1,	{"endif"}},
	{SP_ELSE,		0,				1,	{"else"}},
	{SP_ELSEIF,		0,				1,	{"elseif"}},
	{SP_EXHAUST,		0,				1,	{"exhaust"}},
	{SP_FROM_WORDS,		0,				2,	{"from", "words"}},
	{SP_GENERATE,		PREDNF_META,			3,	{"generate", 0, 0}},
	{SP_GLOBAL_VAR,		PREDNF_META,			3,	{"global", "variable", 0}},
	{SP_IF,			0,				1,	{"if"}},
	{SP_INTERFACE,		PREDNF_META,			2,	{"interface", 0}},
	{SP_INTO,		0,				2,	{"into", 0}},
	{SP_JUST,		0,				1,	{"just"}},
	{SP_LINK,		0,				2,	{"link", 0}},
	{SP_LINK_RES,		0,				3,	{"link", "resource", 0}},
	{SP_LINK_SELF,		0,				1,	{"link"}},
	{SP_LOG,		0,				1,	{"log"}},
	{SP_MATCHING_ALL_OF,	0,				4,	{"matching", "all", "of", 0}},
	{SP_NOW,		0,				1,	{"now"}},
	{SP_OR,			0,				1,	{"or"}},
	{SP_P_RANDOM,		0,				3,	{"purely", "at", "random"}},
	{SP_SELECT,		0,				1,	{"select"}},
	{SP_SPAN,		0,				2,	{"span", 0}},
	{SP_STATUSBAR,		0,				3,	{"status", "bar", 0}},
	{SP_INLINE_STATUSBAR,	0,				4,	{"inline", "status", "bar", 0}},
	{SP_STOPPABLE,		0,				1,	{"stoppable"}},
	{SP_STOPPING,		0,				1,	{"stopping"}},
	{SP_THEN,		0,				1,	{"then"}},
	{SP_T_RANDOM,		0,				3,	{"then", "at", "random"}},
	{SP_T_P_RANDOM,		0,				4,	{"then", "purely", "at", "random"}},
};

struct builtinspec {
	int id;
	int prednameflags;
	int predflags;
	int nword;
	char *word[8];
} builtinspec[] = {
	{BI_LESSTHAN,		0, 0,				3,	{0, "<", 0}},
	{BI_GREATERTHAN,	0, 0,				3,	{0, ">", 0}},
	{BI_PLUS,		0, 0,				5,	{0, "plus", 0, "into", 0}},
	{BI_MINUS,		0, 0,				5,	{0, "minus", 0, "into", 0}},
	{BI_TIMES,		0, 0,				5,	{0, "times", 0, "into", 0}},
	{BI_DIVIDED,		0, 0,				6,	{0, "divided", "by", 0, "into", 0}},
	{BI_MODULO,		0, 0,				5,	{0, "modulo", 0, "into", 0}},
	{BI_RANDOM,		0, 0,				7,	{"random", "from", 0, "to", 0, "into", 0}},
	{BI_FAIL,		0, PREDF_FAIL,			1,	{"fail"}},
	{BI_STOP,		0, PREDF_SUCCEEDS|PREDF_STOP|PREDF_MIGHT_STOP,	1,	{"stop"}},
	{BI_REPEAT,		0, PREDF_SUCCEEDS,		2,	{"repeat", "forever"}},
	{BI_NUMBER,		0, 0,				2,	{"number", 0}},
	{BI_LIST,		0, 0,				2,	{"list", 0}},
	{BI_EMPTY,		0, 0,				2,	{"empty", 0}},
	{BI_NONEMPTY,		0, 0,				2,	{"nonempty", 0}},
	{BI_WORD,		0, 0,				2,	{"word", 0}},
	{BI_UNKNOWN_WORD,	0, 0,				3,	{"unknown", "word", 0}},
	{BI_OBJECT,		0, 0,				2,	{"object", 0}},
	{BI_BOUND,		0, 0,				2,	{"bound", 0}},
	{BI_FULLY_BOUND,	0, 0,				3,	{"fully", "bound", 0}},
	{BI_QUIT,		0, PREDF_SUCCEEDS,		1,	{"quit"}},
	{BI_RESTART,		0, PREDF_SUCCEEDS,		1,	{"restart"}},
	{BI_BREAKPOINT,		0, PREDF_SUCCEEDS,		1,	{"breakpoint"}},
	{BI_SAVE,		0, 0,				2,	{"save", 0}},
	{BI_RESTORE,		0, PREDF_SUCCEEDS,		1,	{"restore"}},
	{BI_SAVE_UNDO,		0, 0,				3,	{"save", "undo", 0}},
	{BI_UNDO,		0, 0,				1,	{"undo"}},
	{BI_SCRIPT_ON,		0, 0,				2,	{"transcript", "on"}},
	{BI_SCRIPT_OFF,		0, PREDF_SUCCEEDS,		2,	{"transcript", "off"}},
	{BI_TRACE_ON,		0, PREDF_SUCCEEDS,		2,	{"trace", "on"}},
	{BI_TRACE_OFF,		0, PREDF_SUCCEEDS,		2,	{"trace", "off"}},
	{BI_NOSPACE,		0, PREDF_SUCCEEDS,		2,	{"no", "space"}},
	{BI_SPACE,		0, PREDF_SUCCEEDS,		1,	{"space"}},
	{BI_SPACE_N,		0, PREDF_SUCCEEDS,		2,	{"space", 0}},
	{BI_LINE,		0, PREDF_SUCCEEDS,		1,	{"line"}},
	{BI_PAR,		0, PREDF_SUCCEEDS,		1,	{"par"}},
	{BI_PROGRESS_BAR,	0, PREDF_SUCCEEDS,		5,	{"progress", "bar", 0, "of", 0}},
	{BI_ROMAN,		0, PREDF_SUCCEEDS,		1,	{"roman"}},
	{BI_UNSTYLE,		0, PREDF_SUCCEEDS,		1,	{"unstyle"}},
	{BI_BOLD,		0, PREDF_SUCCEEDS,		1,	{"bold"}},
	{BI_ITALIC,		0, PREDF_SUCCEEDS,		1,	{"italic"}},
	{BI_REVERSE,		0, PREDF_SUCCEEDS,		1,	{"reverse"}},
	{BI_FIXED,		0, PREDF_SUCCEEDS,		2,	{"fixed", "pitch"}},
	{BI_UPPER,		0, PREDF_SUCCEEDS,		1,	{"uppercase"}},
	{BI_CLEAR,		0, PREDF_SUCCEEDS,		1,	{"clear"}},
	{BI_CLEAR_ALL,		0, PREDF_SUCCEEDS,		2,	{"clear", "all"}},
	{BI_CLEAR_LINKS,	0, PREDF_SUCCEEDS,		2,	{"clear", "links"}},
	{BI_CLEAR_DIV,		0, PREDF_SUCCEEDS,		2,	{"clear", "div"}},
	{BI_CLEAR_OLD,		0, PREDF_SUCCEEDS,		2,	{"clear", "old"}},
	{BI_EMBEDRESOURCE,	0, 0,				3,	{"embed", "resource", 0}},
	{BI_GETINPUT,		0, 0,				3,	{"get", "input", 0}},
	{BI_GETRAWINPUT,	0, 0,				5,	{"", "get", "raw", "input", 0}},	// disabled for now
	{BI_GETKEY,		0, 0,				3,	{"get", "key", 0}},
	{BI_SERIALNUMBER,	0, PREDF_SUCCEEDS,		2,	{"serial", "number"}},
	{BI_COMPILERVERSION,	0, PREDF_SUCCEEDS,		2,	{"compiler", "version"}},
	{BI_MEMSTATS,		0, PREDF_SUCCEEDS,		3,	{"display", "memory", "statistics"}},
	{BI_HASPARENT,		0, PREDF_DYNAMIC,		4,	{0, "has", "parent", 0}},
	{BI_UNIFY,		0, 0,				3,	{0, "=", 0}},
	{BI_IS_ONE_OF,		0, 0,				5,	{0, "is", "one", "of", 0}},
	{BI_SPLIT,		0, 0,				8,	{"split", 0, "by", 0, "into", 0, "and", 0}},
	{BI_APPEND,		0, 0,				4,	{"append", 0, 0, 0}},
	{BI_SPLIT_WORD,		0, 0,				5,	{"split", "word", 0, "into", 0}},
	{BI_JOIN_WORDS,		0, 0,				5,	{"join", "words", 0, "into", 0}},
	{BI_HAVE_UNDO,		0, 0,				3,	{"interpreter", "supports", "undo"}},
	{BI_HAVE_LINK,		0, 0,				3,	{"interpreter", "supports", "links"}},
	{BI_HAVE_QUIT,		0, 0,				3,	{"interpreter", "supports", "quit"}},
	{BI_HAVE_STATUS,	0, 0,				4,	{"interpreter", "supports", "status", "bar"}},
	{BI_HAVE_INLINE_STATUS,	0, 0,				5,	{"interpreter", "supports", "inline", "status", "bar"}},
	{BI_CAN_EMBED,		0, 0,				4,	{"interpreter", "can", "embed", 0}},
	{BI_PROGRAM_ENTRY,	PREDNF_DEFINABLE_BI, 0,		3,	{"program", "entry", "point"}},
	{BI_ERROR_ENTRY,	PREDNF_DEFINABLE_BI, 0,		4,	{"error", 0, "entry", "point"}},
	{BI_QUERY,		0, 0,				2,	{"query", 0}},
	{BI_QUERY_ARG,		0, 0,				3,	{"query", 0, 0}},
	{BI_INVOKE_CLOSURE,	PREDNF_DEFINABLE_BI, 0,		5,	{"", "invoke-closure", 0, 0, 0}},
	{BI_STORY_IFID,		PREDNF_DEFINABLE_BI, 0,		2,	{"story", "ifid"}},
	{BI_STORY_TITLE,	PREDNF_DEFINABLE_BI, 0,		2,	{"story", "title"}},
	{BI_STORY_AUTHOR,	PREDNF_DEFINABLE_BI, 0,		2,	{"story", "author"}},
	{BI_STORY_NOUN,		PREDNF_DEFINABLE_BI, 0,		2,	{"story", "noun"}},
	{BI_STORY_BLURB,	PREDNF_DEFINABLE_BI, 0,		2,	{"story", "blurb"}},
	{BI_STORY_RELEASE,	PREDNF_DEFINABLE_BI, 0,		3,	{"story", "release", 0}},
	{BI_STYLEDEF,		PREDNF_DEFINABLE_BI, 0,		3,	{"style", "class", 0}},
	{BI_ENDINGS,		PREDNF_DEFINABLE_BI, 0,		3,	{"removable", "word", "endings"}},
	{BI_LIB_VERSION,	PREDNF_DEFINABLE_BI, 0,		2,	{"library", "version"}},
	{BI_RESOURCEDEF,	PREDNF_DEFINABLE_BI, 0,		3,	{"define", "resource", 0}},
	{BI_RESOLVERESOURCE,	PREDNF_DEFINABLE_BI, 0,		5,	{"", "resolve", "resource", 0, 0}},
	{BI_INJECTED_QUERY,	PREDNF_DEFINABLE_BI, 0,		3,	{"", "query", 0}},
	{BI_EMBED_INTERNAL,	0, 0,				4,	{"", "embed", "resource", 0}},
	{BI_CAN_EMBED_INTERNAL,	0, 0,				4,	{"", "can", "embed", 0}},
	{BI_BREAKPOINT_AGAIN,	0, 0,				2,	{"", "breakpoint"}},
	{BI_BREAK_GETKEY,	0, 0,				2,	{"", "key"}},
	{BI_BREAK_FAIL,		0, 0,				2,	{"", "fail"}},
};

int body_can_be_fixed_flag(struct astnode *an, struct word *safevar) {
	struct predname *predname;
	int have_constrained_safevar = !safevar;
	struct astnode *sub;

	while(an) {
		if(an->kind == AN_RULE
		|| an->kind == AN_NEG_RULE) {
			predname = an->predicate;
			if(predname->builtin == BI_IS_ONE_OF
			&& (an->children[0]->kind == AN_VARIABLE)
			&& (an->children[0]->word == safevar)) {
				for(sub = an->children[1]; sub->kind == AN_PAIR; sub = sub->children[1]) {
					if(sub->children[0]->kind != AN_TAG) break;
				}
				if(sub->kind != AN_EMPTY_LIST) {
					return 0;
				}
				have_constrained_safevar = 1;
			} else if(predname->builtin) {
				if(predname->builtin != BI_FAIL
				&& predname->builtin != BI_OBJECT) {
					return 0;
				}
			} else if((predname->pred->flags & PREDF_FIXED_FLAG)
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

int pred_can_be_fixed_flag(struct predname *predname) {
	int i;
	struct predicate *pred = predname->pred;

	if(!predname->special
	&& !predname->builtin
	&& pred->nclause
	&& !(pred->flags & PREDF_DYNAMIC)
	&& !(pred->flags & PREDF_MACRO)
	&& (pred->flags & PREDF_INVOKED)
	&& predname->arity == 1) {
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

void find_fixed_flags(struct program *prg) {
	int i;
	struct predname *predname;
	int flag;

	do {
		flag = 0;
		for(i = 0; i < prg->npredicate; i++) {
			predname = prg->predicates[i];

			if(!(predname->pred->flags & PREDF_FIXED_FLAG)
			&& pred_can_be_fixed_flag(predname)) {
#if 0
				printf("fixed flag: %s\n", predname->printed_name);
#endif
				predname->pred->flags |= PREDF_DYNAMIC | PREDF_FIXED_FLAG;
				if(!predname->pred->dynamic) {
					predname->pred->dynamic = calloc(1, sizeof(struct dynamic));
				}
				if(predname->dyn_id == DYN_NONE) {
					predname->dyn_id = prg->nobjflag++;
					prg->objflagpred = realloc(prg->objflagpred, prg->nobjflag * sizeof(struct predname *));
					prg->objflagpred[predname->dyn_id] = predname;
				}
				flag = 1;
			}
		}
	} while(flag);
}

static int may_report_interface_violation(struct program *prg) {
	if(prg->reported_violations > 10) {
		return 0;
	} else {
		prg->reported_violations++;
		if(prg->reported_violations > 10) {
			report(LVL_WARN, 0,
				"Hiding further warnings about interface violations. "
				"The first warning usually describes the root cause.");
			return 0;
		} else {
			return 1;
		}
	}
}

static void trace_enqueue(struct predicate *pred, int queuelen) {
	if(tracequeue && !(pred->flags & PREDF_IN_QUEUE)) {
		pred->flags |= PREDF_IN_QUEUE;
		tracequeue[tracequeue_w++] = pred;
		if(tracequeue_w == queuelen) tracequeue_w = 0;
	}
}

static struct predicate *trace_read_queue(int queuelen) {
	struct predicate *pred = 0;

	if(tracequeue_r != tracequeue_w) {
		pred = tracequeue[tracequeue_r++];
		if(tracequeue_r == queuelen) tracequeue_r = 0;
		pred->flags &= ~PREDF_IN_QUEUE;
	}

	return pred;
}

void add_bound_vars(struct astnode *an, uint8_t *bound, struct clause *cl) {
	int i;

	while(an) {
		if(an->kind == AN_VARIABLE) {
			if(an->word->name[0]) {
				for(i = 0; i < cl->nvar; i++) {
					if(an->word == cl->varnames[i]) break;
				}
				assert(i < cl->nvar);
#if 0
				if(!bound[i]) printf("now, $%s is bound\n", an->word->name);
#endif
				bound[i] = 1;
			}
		} else {
			for(i = 0; i < an->nchild; i++) {
				add_bound_vars(an->children[i], bound, cl);
			}
		}
		an = an->next_in_body;
	}
}

int any_unbound(struct astnode *an, const uint8_t *bound, struct clause *cl) {
	int i;

	while(an) {
		if(an->kind == AN_VARIABLE) {
			if(!an->word->name[0]) return 1;
			for(i = 0; i < cl->nvar; i++) {
				if(an->word == cl->varnames[i]) break;
			}
			assert(i < cl->nvar);
			if(!bound[i]) {
				return 1;
			}
		} else {
			for(i = 0; i < an->nchild; i++) {
				if(any_unbound(an->children[i], bound, cl)) {
					return 1;
				}
			}
		}
		an = an->next_in_body;
	}

	return 0;
}

int mark_ast_unbound(struct astnode *an, const uint8_t *bound, struct clause *cl) {
	int i;

	assert(!an->next_in_body);
	if(an->kind == AN_VARIABLE) {
		if(!an->word->name[0]) {
			an->unbound = 1;
			return 1;
		}
		for(i = 0; i < cl->nvar; i++) {
			if(an->word == cl->varnames[i]) break;
		}
		assert(i < cl->nvar);
		if(!bound[i]) {
			an->unbound = 1;
			return 1;
		}
	} else {
		for(i = 0; i < an->nchild; i++) {
			if(mark_ast_unbound(an->children[i], bound, cl)) {
				an->unbound = 1;
				return 1;
			}
		}
	}

	return 0;
}

void trace_add_caller(struct predicate *callee, struct predicate *caller) {
	struct predlist *pl;

	for(pl = callee->callers; pl; pl = pl->next) {
		if(pl->pred == caller) return;
	}

	pl = arena_alloc(&callee->arena, sizeof(*pl));
	pl->pred = caller;
	pl->next = callee->callers;
	callee->callers = pl;
}

void trace_invoke_pred(struct predname *predname, int flags, uint32_t unbound_in, struct clause *caller, struct program *prg);

void trace_now_expression(struct astnode *an, uint8_t *bound, struct clause *cl, struct program *prg) {
	int i;

	if(an->kind == AN_RULE || an->kind == AN_NEG_RULE) {
		if(!an->predicate->pred->dynamic) {
			// This error will be reported elsewhere.
			return;
		}

		for(i = 0; i < an->predicate->arity; i++) {
			if(any_unbound(an->children[i], bound, cl)) {
				an->children[i]->unbound = 1;
				if(/*(prg->optflags & OPTF_BOUND_PARAMS)
				&& */(an->kind == AN_RULE || an->children[i]->kind != AN_VARIABLE || an->children[i]->word->name[0])) {
					report(
						LVL_WARN,
						an->line,
						"Argument %d of now-expression can be unbound, leading to runtime errors.",
						i + 1);
				}
			}
		}
		if(an->predicate->arity == 1 && !(an->predicate->pred->flags & PREDF_GLOBAL_VAR)) {
			if(an->kind == AN_RULE) {
				an->predicate->pred->dynamic->linkage_flags |= LINKF_SET;
			} else {
				if(an->children[0]->kind != AN_VARIABLE
				|| an->children[0]->word->name[0]) {
					an->predicate->pred->dynamic->linkage_flags |= LINKF_RESET;
				}
				if(an->children[0]->unbound) {
					an->predicate->pred->flags |= PREDF_DYN_LINKAGE;
					an->predicate->pred->dynamic->linkage_flags |= LINKF_CLEAR;
					an->predicate->pred->dynamic->linkage_due_to_line = an->line;
				}
			}
		}
	} else if(an->kind == AN_BLOCK || an->kind == AN_NEG_BLOCK || an->kind == AN_FIRSTRESULT) {
		for(an = an->children[0]; an; an = an->next_in_body) {
			trace_now_expression(an, bound, cl, prg);
		}
	} else {
		report(LVL_ERR, an->line, "Invalid (now) syntax.");
	}
}

int trace_invocations_body(struct astnode **anptr, int flags, uint8_t *bound, struct clause *cl, int tail, struct program *prg) {
	struct astnode *an;
	uint8_t bound_sub[cl->nvar], bound_accum[cl->nvar];
	int i, j, failed = 0;
	uint32_t unbound;
	int moreflags;

	while((an = *anptr) && !failed) {
		switch(an->kind) {
		case AN_RULE:
		case AN_NEG_RULE:
			an->predicate->pred->invoked_at_line = an->line;
			moreflags = 0;
			if(an->subkind == RULE_SIMPLE
			&& an->predicate->builtin == BI_REPEAT
			&& !prg->did_warn_about_repeat) {
				report(LVL_WARN, an->line, "(repeat forever) not invoked as a multi-query.");
				prg->did_warn_about_repeat = 1;
			}
			if(an->subkind == RULE_SIMPLE
			|| (tail && !an->next_in_body && (cl->predicate->pred->flags & PREDF_INVOKED_SIMPLE))) {
				moreflags |= PREDF_INVOKED_SIMPLE;
			}
			if(an->subkind == RULE_MULTI) {
				moreflags |= PREDF_INVOKED_MULTI;
			}
			for(i = 0; i < an->predicate->arity; i++) {
				mark_ast_unbound(an->children[i], bound, cl);
			}
			if(an->predicate->pred->flags & PREDF_FAIL) {
				an->predicate->pred->flags |= flags | moreflags;
				if(an->kind == AN_RULE) failed = 1;
			} else if(an->predicate->pred->dynamic) {
				if(an->predicate->arity
				&& !(an->predicate->pred->flags & PREDF_GLOBAL_VAR)
				&& an->predicate->builtin != BI_HASPARENT) {
					if(an->children[0]->unbound) {
						if(an->predicate->arity > 1) {
							if(prg->optflags & OPTF_BOUND_PARAMS) report(
								LVL_WARN,
								an->line,
								"Dynamic predicate with unbound first argument will loop over all objects.");
						} else {
							an->predicate->pred->flags |= PREDF_DYN_LINKAGE;
							an->predicate->pred->dynamic->linkage_flags |= LINKF_LIST;
							an->predicate->pred->dynamic->linkage_due_to_line = an->line;
						}
					}
				}
				for(i = 0; i < an->predicate->arity; i++) {
					add_bound_vars(an->children[i], bound, cl);
				}
			} else {
				unbound = 0;
				for(i = 0; i < an->predicate->arity; i++) {
					if(an->children[i]->unbound) {
						unbound |= 1 << i;
						if(an->predicate->pred->iface_bound_in & (1 << i)) {
							if(may_report_interface_violation(prg)) {
								report(LVL_WARN, an->line,
									"Parameter #%d of %s can be (partially) unbound, "
									"which violates the interface declaration at %s:%d.",
									i + 1,
									an->predicate->printed_name,
									FILEPART(an->predicate->pred->iface_decl->line),
									LINEPART(an->predicate->pred->iface_decl->line));
							}
						}
					}
				}
				if(!(cl->predicate->pred->flags & PREDF_VISITED)) {
					trace_add_caller(an->predicate->pred, cl->predicate->pred);
				}
				trace_invoke_pred(
					an->predicate,
					flags | moreflags,
					unbound,
					cl,
					prg);
				if(an->predicate->pred->flags & PREDF_STOP) {
					failed = 1;
				}
				if(an->kind == AN_RULE) {
					if(an->predicate->builtin == BI_UNIFY) {
						if(!an->children[0]->unbound || !an->children[1]->unbound) {
							for(i = 0; i < 2; i++) {
								add_bound_vars(an->children[i], bound, cl);
							}
						}
					} else if(an->predicate->builtin == BI_SPLIT) {
						if(!an->children[0]->unbound) {
							add_bound_vars(an->children[2], bound, cl);
							add_bound_vars(an->children[3], bound, cl);
						}
					} else if(an->predicate->builtin == BI_APPEND) {
						if(!an->children[1]->unbound || !an->children[2]->unbound) {
							for(i = 1; i < 3; i++) {
								add_bound_vars(an->children[i], bound, cl);
							}
						}
					} else if(an->predicate->builtin == BI_IS_ONE_OF) {
						if(!an->children[1]->unbound) {
							add_bound_vars(an->children[0], bound, cl);
						}
					} else {
						for(i = 0; i < an->predicate->arity; i++) {
							if(!(an->predicate->pred->unbound_out & (1 << i))) {
								add_bound_vars(an->children[i], bound, cl);
							}
						}
					}
				}
			}
			break;
		case AN_NOW:
			trace_now_expression(an->children[0], bound, cl, prg);
			break;
		case AN_BLOCK:
		case AN_FIRSTRESULT:
			failed = !trace_invocations_body(&an->children[0], flags, bound, cl, tail && !an->next_in_body, prg);
			break;
		case AN_STOPPABLE:
			memcpy(bound_sub, bound, cl->nvar);
			(void) trace_invocations_body(&an->children[0], flags, bound_sub, cl, 1, prg);
			break;
		case AN_STATUSAREA:
		case AN_OUTPUTBOX:
			memcpy(bound_sub, bound, cl->nvar);
			(void) trace_invocations_body(&an->children[1], flags, bound_sub, cl, 0, prg);
			break;
		case AN_LINK_SELF:
			memcpy(bound_sub, bound, cl->nvar);
			(void) trace_invocations_body(&an->children[0], flags, bound_sub, cl, 0, prg);
			break;
		case AN_LINK:
		case AN_LINK_RES:
			memcpy(bound_sub, bound, cl->nvar);
			(void) trace_invocations_body(&an->children[1], flags, bound_sub, cl, 0, prg);
			break;
		case AN_LOG:
			if(!(prg->optflags & OPTF_NO_LOG)) {
				memcpy(bound_sub, bound, cl->nvar);
				(void) trace_invocations_body(&an->children[0], flags, bound_sub, cl, 0, prg);
			}
			break;
		case AN_NEG_BLOCK:
			for(i = 0; i < an->nchild; i++) {
				memcpy(bound_sub, bound, cl->nvar);
				(void) trace_invocations_body(&an->children[i], flags, bound_sub, cl, 0, prg);
			}
			break;
		case AN_SELECT:
			/* drop through */
		case AN_OR:
			memset(bound_accum, 1, cl->nvar);
			failed = 1;
			for(i = 0; i < an->nchild; i++) {
				memcpy(bound_sub, bound, cl->nvar);
				if(trace_invocations_body(&an->children[i], flags, bound_sub, cl, tail && !an->next_in_body, prg)) {
					for(j = 0; j < cl->nvar; j++) {
						bound_accum[j] &= bound_sub[j];
					}
					failed = 0;
				}
			}
			if(!failed) memcpy(bound, bound_accum, cl->nvar);
			break;
		case AN_EXHAUST:
			memcpy(bound_sub, bound, cl->nvar);
			(void) trace_invocations_body(&an->children[0], flags, bound_sub, cl, 0, prg);
			break;
		case AN_IF:
			failed = 1;
			memset(bound_accum, 1, cl->nvar);
			memcpy(bound_sub, bound, cl->nvar);
			if(trace_invocations_body(&an->children[0], flags, bound_sub, cl, 0, prg)
			&& trace_invocations_body(&an->children[1], flags, bound_sub, cl, tail && !an->next_in_body, prg)) {
				memcpy(bound_accum, bound_sub, cl->nvar);
				failed = 0;
			}
			memcpy(bound_sub, bound, cl->nvar);
			if(trace_invocations_body(&an->children[2], flags, bound_sub, cl, tail && !an->next_in_body, prg)) {
				for(j = 0; j < cl->nvar; j++) {
					bound_accum[j] &= bound_sub[j];
				}
				failed = 0;
			}
			if(!failed) memcpy(bound, bound_accum, cl->nvar);
			break;
		case AN_COLLECT:
		case AN_ACCUMULATE:
			memcpy(bound_sub, bound, cl->nvar);
			if(!trace_invocations_body(&an->children[0], flags, bound_sub, cl, 0, prg)
			|| an->kind == AN_ACCUMULATE
			|| !any_unbound(an->children[1], bound_sub, cl)) {
				add_bound_vars(an->children[2], bound, cl);
			}
			break;
		case AN_COLLECT_WORDS:
			memcpy(bound_sub, bound, cl->nvar);
			(void) trace_invocations_body(&an->children[0], PREDF_INVOKED_FOR_WORDS, bound_sub, cl, 0, prg);
			add_bound_vars(an->children[1], bound, cl);
			break;
		case AN_DETERMINE_OBJECT:
			if(any_unbound(an->children[0], bound, cl)) {
				an->children[0]->unbound = 1;
			}
			if(trace_invocations_body(&an->children[1], flags, bound, cl, 0, prg)) {
				memcpy(bound_sub, bound, cl->nvar);
				(void) trace_invocations_body(&an->children[2], PREDF_INVOKED_FOR_WORDS, bound_sub, cl, 0, prg);
			} else {
				failed = 1;
			}
			break;
		}
		anptr = &an->next_in_body;
	}

	return !failed;
}

uint32_t trace_reconsider_clause(struct clause *cl, int flags, uint32_t unbound_in, struct program *prg) {
	int i;
	uint8_t bound[cl->nvar];
	uint32_t unbound_out = 0;

#if 0
	printf("considering clause at %s:%d\n", FILEPART(cl->line), LINEPART(cl->line));
#endif

	memset(bound, 0, cl->nvar);
	for(i = 0; i < cl->predicate->arity; i++) {
		if(unbound_in & (1 << i)) {
			mark_ast_unbound(cl->params[i], bound, cl);
		} else {
			add_bound_vars(cl->params[i], bound, cl);
		}
	}

	if(trace_invocations_body(&cl->body, flags, bound, cl, 1, prg)) {
		for(i = 0; i < cl->predicate->arity; i++) {
			if((unbound_in & (1 << i))
			&& any_unbound(cl->params[i], bound, cl)) {
				unbound_out |= 1 << i;
			}
		}
	}

	return unbound_out;
}

void trace_reconsider_pred(struct predicate *pred, struct program *prg) {
	int i, j;
	uint32_t unbound = pred->unbound_out;
	struct predlist *pl;

#if 0
	printf("reconsidering %s with bits %x %x\n", pred->predname->printed_name, pred->unbound_in, pred->unbound_out);
#endif
	for(i = 0; i < pred->nclause; i++) {
		uint32_t old = unbound;

		unbound |= trace_reconsider_clause(
			pred->clauses[i],
			pred->flags & PREDF_INVOKED,
			pred->unbound_in,
			prg);

		for(j = 0; j < pred->predname->arity; j++) {
			if((unbound & ~old) & (1 << j)) {
				assert(!pred->unbound_out_due_to[j]);
				pred->unbound_out_due_to[j] = pred->clauses[i];
#if 0
				printf("now, parameter %d of %s can be left unbound\n", j, pred->predname->printed_name);
#endif
				if(pred->iface_bound_out & (1 << j)) {
					if(may_report_interface_violation(prg)) {
						report(LVL_WARN, pred->clauses[i]->line,
							"Rule can leave parameter #%d (partially) unbound, "
							"which violates the interface declaration at %s:%d.",
							j + 1,
							FILEPART(pred->iface_decl->line),
							LINEPART(pred->iface_decl->line));
					}
				}
			}
		}
	}
	pred->flags |= PREDF_VISITED;

	if(unbound != pred->unbound_out) {
		pred->unbound_out = unbound;
		for(pl = pred->callers; pl; pl = pl->next) {
			trace_enqueue(pl->pred, prg->npredicate);
		}
	}
}

void trace_invoke_pred(struct predname *predname, int flags, uint32_t unbound_in, struct clause *caller, struct program *prg) {
	int i;
	struct predicate *pred = predname->pred;

	//printf("invoking %s, old flags %x, new flags %x\n", predname->printed_name, pred->flags, flags);

	if((pred->flags != (pred->flags | flags))
	|| (pred->unbound_in != (pred->unbound_in | unbound_in))) {
		for(i = 0; i < predname->arity; i++) {
			if(unbound_in & (1 << i)) {
				if(!(pred->unbound_in & (1 << i))) {
#if 0
					printf("now, %s was called with parameter %d potentially unbound\n",
						pred->predname->printed_name,
						i);
#endif
				}
				if(!pred->unbound_in_due_to[i]
				&& caller->predicate != predname) {
					pred->unbound_in_due_to[i] = caller;
				}
			}
		}
		pred->flags |= flags;
		pred->unbound_in |= unbound_in;
		trace_enqueue(pred, prg->npredicate);
	}
}

void trace_entrypoint(struct predname *predname, struct program *prg, int mode) {
	predname->pred->flags |= PREDF_INVOKED_SIMPLE | PREDF_INVOKED_MULTI | PREDF_INVOKED_NORMALLY;
	trace_invoke_pred(predname, mode, 0, 0, prg);
}

void trace_invocations(struct program *prg) {
	struct predicate *pred;
	struct predname *predname;
	int i;

	tracequeue = malloc(prg->npredicate * sizeof(struct predicate *));

	trace_entrypoint(find_builtin(prg, BI_PROGRAM_ENTRY), prg, PREDF_INVOKED_BY_PROGRAM);
	trace_entrypoint(find_builtin(prg, BI_ERROR_ENTRY), prg, PREDF_INVOKED_BY_PROGRAM);
	trace_entrypoint(find_builtin(prg, BI_OBJECT), prg, PREDF_INVOKED_BY_PROGRAM); // invoked by implicit object loops

	find_builtin(prg, BI_BOUND)->pred->unbound_out |= 1; // it might be a list with some unbound element

	for(i = 0; i < prg->npredicate; i++) {
		predname = prg->predicates[i];
		pred = predname->pred;
		if(pred->flags & PREDF_DYNAMIC) {
			pred->flags |= PREDF_INVOKED_SIMPLE | PREDF_INVOKED_NORMALLY; // for initial values
			if(pred->flags & PREDF_GLOBAL_VAR) {
				pred->unbound_in |= 1; // when computing the initial value
			}
			if(predname->arity == 2) {
				pred->unbound_in |= 2; // when computing the initial value
				if(predname->builtin == BI_HASPARENT) {
					pred->unbound_in |= 1;
				}
			}
			trace_invoke_pred(predname, PREDF_INVOKED_BY_DEBUGGER, 0, 0, prg);
		}
	}

	if(!(prg->optflags & OPTF_BOUND_PARAMS)) {
		// In the debugger, all predicates are potential entry points.
		// All incoming parameters are potentially unbound too,
		// but we handle that in compile.c.

		for(i = 0; i < prg->npredicate; i++) {
			predname = prg->predicates[i];
			if(!predname->special
			&& !(predname->pred->flags & PREDF_DYNAMIC)) {
				trace_entrypoint(predname, prg, PREDF_INVOKED_BY_DEBUGGER);
			}
		}
	}

	while((pred = trace_read_queue(prg->npredicate))) {
		trace_reconsider_pred(pred, prg);
	}

	free(tracequeue);
	tracequeue = 0;
	tracequeue_r = tracequeue_w = 0;

	for(i = 0; i < prg->npredicate; i++) {
		predname = prg->predicates[i];
		pred = predname->pred;
		if(!predname->special
		&& !(predname->builtin && !(predname->nameflags & PREDNF_DEFINABLE_BI))
		&& predname->builtin != BI_INVOKE_CLOSURE
		&& predname->builtin != BI_ERROR_ENTRY
		&& !pred->dynamic
		&& (pred->flags & PREDF_INVOKED_BY_PROGRAM)
		&& !(pred->flags & PREDF_DEFINED)) {
			report(
				LVL_WARN,
				pred->invoked_at_line,
				"A query is made to '%s', but there is no matching rule definition.",
				predname->printed_name);
		}
	}
}

static int mark_all_dynamic(struct program *prg, struct astnode *an, line_t line, int allow_multi) {
	int success = 1;

	while(an) {
		if(an->kind == AN_RULE
		|| an->kind == AN_NEG_RULE) {
			if(an->predicate->special) {
				report(LVL_ERR, line, "(now) cannot be followed by special syntax.");
				success = 0;
			} else if(an->predicate->builtin && an->predicate->builtin != BI_HASPARENT) {
				report(LVL_ERR, line, "(now) cannot be combined with this built-in predicate.");
				success = 0;
			} else if(an->subkind == RULE_MULTI && !allow_multi) {
				report(LVL_ERR, line, "(now) cannot be combined with a multi-call.");
				success = 0;
			} else if(an->predicate->arity == 0) {
				if(an->predicate->dyn_id == DYN_NONE) {
					an->predicate->dyn_id = prg->nglobalflag++;
					prg->globalflagpred = realloc(prg->globalflagpred, prg->nglobalflag * sizeof(struct predname *));
					prg->globalflagpred[an->predicate->dyn_id] = an->predicate;
				}
			} else if(an->predicate->arity == 1) {
				if(an->predicate->pred->flags & PREDF_GLOBAL_VAR) {
					assert(an->predicate->dyn_var_id != DYN_NONE);
				} else {
					if(an->predicate->dyn_id == DYN_NONE) {
						an->predicate->dyn_id = prg->nobjflag++;
						prg->objflagpred = realloc(prg->objflagpred, prg->nobjflag * sizeof(struct predname *));
						prg->objflagpred[an->predicate->dyn_id] = an->predicate;
					}
				}
			} else if(an->predicate->arity == 2) {
				if(an->predicate->dyn_id == DYN_NONE) {
					an->predicate->dyn_id = prg->nobjvar++;
					prg->objvarpred = realloc(prg->objvarpred, prg->nobjvar * sizeof(struct predname *));
					prg->objvarpred[an->predicate->dyn_id] = an->predicate;
				}
			} else {
				report(LVL_ERR, an->line, "Dynamic predicates can have a maximum of two parameters.");
				success = 0;
			}
			if(success) {
				an->predicate->pred->flags |= PREDF_DYNAMIC;
				if(!an->predicate->pred->dynamic) {
					an->predicate->pred->dynamic = calloc(1, sizeof(struct dynamic));
				}
			}
		} else if(an->kind == AN_BLOCK) {
			success &= mark_all_dynamic(prg, an->children[0], line, allow_multi);
		} else if(an->kind == AN_FIRSTRESULT) {
			success &= mark_all_dynamic(prg, an->children[0], line, 1);
		} else if(an->kind == AN_NEG_BLOCK) {
			if((an->children[0]->kind != AN_RULE && an->children[0]->kind != AN_NEG_RULE)
			|| an->children[0]->next_in_body) {
				report(LVL_ERR, line, "(now) only works with rules, negated rules, and blocks.");
				success = 0;
			} else {
				success &= mark_all_dynamic(prg, an->children[0], line, 1);
			}
		} else {
			report(LVL_ERR, line, "(now) only works with rules, negated rules, and blocks.");
			success = 0;
		}
		an = an->next_in_body;
	}

	return success;
}

static int find_dynamic(struct program *prg, struct astnode *an, line_t line) {
	int i, success = 1;

	while(an) {
		if(an->kind == AN_NOW) {
			success &= mark_all_dynamic(prg, an->children[0], line, 0);
		} else {
			for(i = 0; i < an->nchild; i++) {
				success &= find_dynamic(prg, an->children[i], line);
			}
		}
		an = an->next_in_body;
	}

	return success;
}

void find_dict_words(struct program *prg, struct astnode *an, int include_barewords) {
	int i;
	struct word *w;

	while(an) {
		if(an->kind == AN_DICTWORD
		|| (an->kind == AN_BAREWORD && include_barewords)) {
			int len = strlen(an->word->name);
			uint16_t wstr[len + 1];
			char strbuf[(len * 3) + 1];

			assert(an->word->name[0]);
			utf8_to_unicode(wstr, len + 1, (uint8_t *) an->word->name);
			for(i = 0; wstr[i]; i++) {
				if(wstr[i] >= 'A' && wstr[i] <= 'Z') {
					wstr[i] = wstr[i] - 'A' + 'a';
				} else if(wstr[i] >= 0x80) {
					wstr[i] = unicode_to_lower(wstr[i]);
				}
			}
			unicode_to_utf8((uint8_t *) strbuf, (len * 3) + 1, wstr);
			w = find_word(prg, strbuf);
			ensure_dict_word(prg, w);
			if(!(an->word->flags & WORDF_DICT)) {
				an->word->flags |= WORDF_DICT;
				an->word->dict_id = w->dict_id;
			}
		}
		if(an->kind == AN_COLLECT_WORDS) {
			find_dict_words(prg, an->children[0], 1);
			find_dict_words(prg, an->children[1], include_barewords);
		} else if(an->kind == AN_DETERMINE_OBJECT) {
			find_dict_words(prg, an->children[0], include_barewords);
			find_dict_words(prg, an->children[1], include_barewords);
			find_dict_words(prg, an->children[2], 1);
			find_dict_words(prg, an->children[3], include_barewords);
		} else if(an->kind == AN_OUTPUTBOX || an->kind == AN_STATUSAREA) {
			find_dict_words(prg, an->children[1], include_barewords);
		} else if(an->kind == AN_LINK_SELF) {
			find_dict_words(prg, an->children[0], include_barewords);
		} else if(an->kind == AN_LOG) {
			if(!(prg->optflags & OPTF_NO_LOG)) {
				find_dict_words(prg, an->children[0], include_barewords);
			}
		} else {
			for(i = 0; i < an->nchild; i++) {
				find_dict_words(prg, an->children[i], include_barewords);
			}
		}
		an = an->next_in_body;
	}
}

void build_dictionary(struct program *prg) {
	int i, j, k;
	struct predname *predname;
	struct predicate *pred;

	for(i = 0; i < prg->npredicate; i++) {
		predname = prg->predicates[i];
		pred = predname->pred;
		if(pred->flags & (PREDF_INVOKED | PREDF_DYNAMIC)) {
			for(j = 0; j < pred->nclause; j++) {
				for(k = 0; k < predname->arity; k++) {
					find_dict_words(prg, pred->clauses[j]->params[k], 0);
				}
				find_dict_words(
					prg,
					pred->clauses[j]->body,
					!!(pred->flags & PREDF_INVOKED_FOR_WORDS));
			}
		}
	}
}

static int query_known_to_fail(struct program *prg, struct astnode *an, struct word *objvar, int onum, int max_depth);
static int query_known_to_succeed(struct program *prg, struct astnode *an, struct word *objvar, int onum, int max_depth);

static int body_can_succeed(struct program *prg, struct astnode *an, struct word *objvar, int onum, int max_depth) {
	if(max_depth <= 0) {
		return 1;
	}

	while(an) {
		if(an->kind == AN_RULE) {
			if(query_known_to_fail(prg, an, objvar, onum, max_depth - 1)) {
				return 0;
			}
		} else if(an->kind == AN_NEG_RULE) {
			if(query_known_to_succeed(prg, an, objvar, onum, max_depth - 1)) {
				return 0;
			}
		} else if(an->kind != AN_BAREWORD) {
			return 1;
		}
		an = an->next_in_body;
	}

	return 1;
}

static int body_must_succeed(struct program *prg, struct astnode *an, struct word *objvar, int onum, int max_depth) {
	if(max_depth <= 0) {
		return 0;
	}

	while(an) {
		if(an->kind == AN_RULE) {
			if(!query_known_to_succeed(prg, an, objvar, onum, max_depth - 1)) {
				return 0;
			}
		} else if(an->kind == AN_NEG_RULE) {
			if(!query_known_to_fail(prg, an, objvar, onum, max_depth - 1)) {
				return 0;
			}
		} else {
			return 0;
		}
		an = an->next_in_body;
	}

	return 1;
}

static int query_known_to_fail(struct program *prg, struct astnode *an, struct word *objvar, int onum, int max_depth) {
	struct predname *predname;
	struct astnode *sub;
	int i;

	if(max_depth <= 0) {
		return 0;
	}

	predname = an->predicate;

	if(predname->pred->flags & PREDF_FAIL) {
		return 1;
	}

	if(predname->builtin == BI_IS_ONE_OF) {
		if(an->children[0]->kind == AN_VARIABLE
		&& an->children[0]->word == objvar
		&& an->children[1]->kind == AN_PAIR) {
			for(sub = an->children[1]; sub && sub->kind == AN_PAIR; sub = sub->children[1]) {
				if(sub->children[0]->kind == AN_TAG
				&& sub->children[0]->word->obj_id == onum) {
					break;
				}
			}
			if(sub->kind == AN_EMPTY_LIST) {
				return 1;
			}
		}
	} else if(predname->builtin == BI_LIST || predname->builtin == BI_NONEMPTY) {
		if(an->children[0]->kind == AN_VARIABLE
		&& an->children[0]->word == objvar) {
			return 1;
		}
	} else if(predname->arity
	&& !predname->builtin
	&& (
		(predname->pred->flags & PREDF_FIXED_FLAG) ||
		!(predname->pred->flags & PREDF_DYNAMIC)))
	{
		if(an->children[0]->kind == AN_VARIABLE
		&& an->children[0]->word == objvar) {
			for(i = 0; i < predname->pred->nclause; i++) {
				struct clause *cl = predname->pred->clauses[i];

				if(cl->params[0]->kind == AN_TAG
				&& cl->params[0]->word->obj_id == onum) {
					if(body_can_succeed(prg, cl->body, 0, -1, max_depth - 1)) {
						return 0;
					}
				} else if(cl->params[0]->kind == AN_VARIABLE) {
					if(cl->params[0]->word->name[0]) {
						if(body_can_succeed(prg, cl->body, cl->params[0]->word, onum, max_depth - 1)) {
							return 0;
						}
					} else {
						if(body_can_succeed(prg, cl->body, 0, -1, max_depth - 1)) {
							return 0;
						}
					}
				}
			}
			return 1;
		}
	}

	return 0;
}

static int query_known_to_succeed(struct program *prg, struct astnode *an, struct word *objvar, int onum, int max_depth) {
	struct predname *predname;
	struct astnode *sub;
	int i;

	if(max_depth <= 0) {
		return 0;
	}

	predname = an->predicate;

	if(predname->pred->flags & PREDF_FAIL) {
		return 0;
	}

	if(predname->builtin == BI_IS_ONE_OF) {
		if(an->children[0]->kind == AN_VARIABLE
		&& an->children[0]->word == objvar
		&& an->children[1]->kind == AN_PAIR) {
			for(sub = an->children[1]; sub && sub->kind == AN_PAIR; sub = sub->children[1]) {
				if(sub->children[0]->kind == AN_TAG
				&& sub->children[0]->word->obj_id == onum) {
					return 1;
				}
			}
		}
	} else if(predname->pred->flags & PREDF_FIXED_FLAG) {
		if(an->children[0]->kind == AN_VARIABLE
		&& an->children[0]->word == objvar) {
			for(i = 0; i < predname->pred->nclause; i++) {
				struct clause *cl = predname->pred->clauses[i];

				if(cl->params[0]->kind == AN_TAG
				&& cl->params[0]->word->obj_id == onum) {
					if(body_must_succeed(prg, cl->body, 0, -1, max_depth - 1)) {
						return 1;
					}
				} else if(cl->params[0]->kind == AN_VARIABLE) {
					if(body_must_succeed(prg, cl->body, cl->params[0]->word, onum, max_depth - 1)) {
						return 1;
					}
				} // We will not enter this clause.
			}
		}
	}
	return 0;
}

static void extract_wordmap_from_body(struct wordmap_tally *tallies, int tally_onum, struct program *prg, struct astnode *an, struct word *objvar, int onum, int max_depth);

static void extract_wordmap_from_pred(struct wordmap_tally *tallies, int tally_onum, struct program *prg, struct predname *predname, int objarg, int onum, int max_depth) {
	int i;

	for(i = 0; i < predname->pred->nclause; i++) {
		struct clause *cl = predname->pred->clauses[i];

		if(objarg >= 0 && cl->params[objarg]->kind == AN_TAG) {
			if(cl->params[objarg]->word == prg->worldobjnames[onum]) {
				extract_wordmap_from_body(tallies, tally_onum, prg, cl->body, 0, -1, max_depth - 1);
				if(cl->body && cl->body->kind == AN_JUST) break;
			}
		} else if(objarg >= 0 && cl->params[objarg]->kind == AN_VARIABLE) {
			extract_wordmap_from_body(tallies, tally_onum, prg, cl->body, cl->params[objarg]->word, onum, max_depth - 1);
			if(cl->body && cl->body->kind == AN_JUST) break;
		} else if(objarg < 0) {
			extract_wordmap_from_body(tallies, tally_onum, prg, cl->body, 0, -1, max_depth - 1);
			if(cl->body && cl->body->kind == AN_JUST) break;
		}
	}
}

static void extract_wordmap_from_body(struct wordmap_tally *tallies, int tally_onum, struct program *prg, struct astnode *an, struct word *objvar, int onum, int max_depth) {
	int i;
	struct wordmap_tally *tally;

	if(max_depth <= 0) {
		// Prevent infinite recursion, since we can't detect the base case.
		// This object must always be considered.

		tally = &tallies[prg->ndictword];
		for(i = 0; i < tally->count && i < MAXWORDMAP; i++) {
			if(tally->onumtable[i] == tally_onum) break;
		}
		if(i == tally->count || i == MAXWORDMAP) {
			if(tally->count < MAXWORDMAP) {
				tally->onumtable[tally->count] = tally_onum;
			}
			if(tally->count <= MAXWORDMAP) {
				tally->count++;
			}
		}

		return;
	}

	while(an) {
		switch(an->kind) {
			case AN_RULE:
			case AN_NEG_RULE:
				for(i = 0; i < an->predicate->arity; i++) {
					if(an->children[i]->kind == AN_VARIABLE
					&& an->children[i]->word == objvar) {
						if(!(an->predicate->pred->flags & PREDF_DYNAMIC)) {
							extract_wordmap_from_pred(tallies, tally_onum, prg, an->predicate, i, onum, max_depth);
						}
						break;
					} else if(an->children[i]->kind == AN_TAG) {
						if(!(an->predicate->pred->flags & PREDF_DYNAMIC)) {
							extract_wordmap_from_pred(tallies, tally_onum, prg, an->predicate, i, an->children[i]->word->obj_id, max_depth);
						}
						break;
					}
				}
				if(i == an->predicate->arity) {
					extract_wordmap_from_pred(tallies, tally_onum, prg, an->predicate, -1, -1, max_depth);
				}
				if(an->kind == AN_RULE && query_known_to_fail(prg, an, objvar, onum, max_depth - 1)) {
					return;
				} else if(an->kind == AN_NEG_RULE && query_known_to_succeed(prg, an, objvar, onum, max_depth - 1)) {
					return;
				}
				break;
			case AN_BAREWORD:
			case AN_DICTWORD:
			case AN_VARIABLE:
				tally = 0;
				if(an->kind == AN_VARIABLE) {
					tally = &tallies[prg->ndictword];
				} else if(an->word->flags & WORDF_DICT) {
					tally = &tallies[an->word->dict_id];
				}
				// trace_invocations is more discerning when determining what dictionary words
				// are reachable. If WORDF_DICT is unset, we can safely ignore this word.
				if(tally) {
					for(i = 0; i < tally->count && i < MAXWORDMAP; i++) {
						if(tally->onumtable[i] == tally_onum) break;
					}
					if(i == tally->count || i == MAXWORDMAP) {
						if(tally->count < MAXWORDMAP) {
							tally->onumtable[tally->count] = tally_onum;
						}
						if(tally->count <= MAXWORDMAP) {
							tally->count++;
						}
					}
				}
				break;
			case AN_IF:
				extract_wordmap_from_body(tallies, tally_onum, prg, an->children[0], objvar, onum, max_depth - 1);
				if(body_can_succeed(prg, an->children[0], objvar, onum, max_depth - 1)) {
					extract_wordmap_from_body(tallies, tally_onum, prg, an->children[1], objvar, onum, max_depth - 1);
				}
				if(!body_must_succeed(prg, an->children[0], objvar, onum, max_depth - 1)) {
					extract_wordmap_from_body(tallies, tally_onum, prg, an->children[2], objvar, onum, max_depth - 1);
				}
				break;
			case AN_BLOCK:
			case AN_NEG_BLOCK:
			case AN_FIRSTRESULT:
			case AN_STOPPABLE:
			case AN_OUTPUTBOX:
			case AN_STATUSAREA:
			case AN_LINK_SELF:
			case AN_LINK:
			case AN_LINK_RES:
			case AN_OR:
			case AN_SELECT:
			case AN_EXHAUST:
			case AN_COLLECT:
			case AN_ACCUMULATE:
				for(i = 0; i < an->nchild; i++) {
					extract_wordmap_from_body(tallies, tally_onum, prg, an->children[i], objvar, onum, max_depth - 1);
				}
				break;
			case AN_LOG:
				if(!(prg->optflags & OPTF_NO_LOG)) {
					extract_wordmap_from_body(tallies, tally_onum, prg, an->children[0], objvar, onum, max_depth - 1);
				}
				break;
			case AN_COLLECT_WORDS:
			case AN_DETERMINE_OBJECT:
			case AN_NOW:
			case AN_JUST:
			case AN_EMPTY_LIST:
			case AN_PAIR:
			case AN_INTEGER:
			case AN_TAG:
				break;
			default:
				assert(0); exit(1);
		}
		an = an->next_in_body;
	}
}

static int cmp_tally(const void *a, const void *b) {
	const struct wordmap_tally *aa = a;
	const struct wordmap_tally *bb = b;

	return aa->key - bb->key;
}

static int compute_wordmap(struct program *prg, struct astnode *generators, struct astnode *collectbody, struct word *objvar, struct predicate *pred) {
	int onum, i, j, k, n;
	struct wordmap_tally tallies[prg->ndictword + 1];
	struct wordmap *map;

	for(i = 0; i < prg->ndictword + 1; i++) {
		tallies[i].key = i;
		tallies[i].count = 0;
	}
	tallies[prg->ndictword].key = 0xffff;

	for(onum = 0; onum < prg->nworldobj; onum++) {
		if(body_can_succeed(prg, generators, objvar, onum, 8)) {
			extract_wordmap_from_body(tallies, onum, prg, collectbody, objvar, onum, 8);
		}
	}

	(void) j;
	(void) k;

	if(tallies[prg->ndictword].count > MAXWORDMAP) {
		return -1;
	}

#if 1
	for(i = 0; i < prg->ndictword; i++) {
		if(tallies[i].count <= MAXWORDMAP) {
			for(j = 0; j < tallies[i].count; ) {
				for(k = 0; k < tallies[prg->ndictword].count; k++) {
					if(tallies[i].onumtable[j] == tallies[prg->ndictword].onumtable[k]) break;
				}
				if(k < tallies[prg->ndictword].count) {
					memmove(tallies[i].onumtable + j, tallies[i].onumtable + j + 1, (tallies[i].count - j - 1) * sizeof(uint16_t));
					tallies[i].count--;
				} else {
					j++;
				}
			}
		}
	}
#endif

#if 0
	printf("reverse wordmap, line %d:\n", LINEPART(collectbody->line));
	for(i = 0; i < prg->ndictword + 1; i++) {
		if(tallies[i].count) {
			printf("  ");
			if(i == prg->ndictword) {
				printf("%-20s", "(always)");
			} else {
				printf("%-20s", prg->dictwordnames[i]->name);
			}
			if(tallies[i].count > MAXWORDMAP) {
				printf(" (many)");
			} else {
				for(j = 0; j < tallies[i].count; j++) {
					printf(" #%s", prg->worldobjnames[tallies[i].onumtable[j]]->name);
				}
			}
			printf("\n");
		}
	}
#endif

	pred->wordmaps = realloc(pred->wordmaps, (pred->nwordmap + 1) * sizeof(struct wordmap));
	map = &pred->wordmaps[pred->nwordmap++];

	n = 0;
	for(i = 0; i < prg->ndictword + 1; i++) {
		if(tallies[i].count) n++;
	}
	map->nmap = n;
	map->map = arena_alloc(&pred->arena, n * sizeof(struct wordmap_tally));
	n = 0;
	for(i = 0; i < prg->ndictword + 1; i++) {
		if(tallies[i].count) {
			memcpy(&map->map[n], &tallies[i], sizeof(struct wordmap_tally));
			n++;
		}
	}
	assert(map->nmap == n);

	if(prg->dictmap) {
		for(i = 0; i < n; i++) {
			if(map->map[i].key != 0xffff) {
				map->map[i].key = prg->dictmap[map->map[i].key];
			}
		}
		qsort(map->map, map->nmap, sizeof(struct wordmap_tally), cmp_tally);
		for(i = 0; i < map->nmap - 1; ) {
			if(map->map[i].key == map->map[i + 1].key) {
				if(map->map[i].count >= MAXWORDMAP || map->map[i + 1].count >= MAXWORDMAP) {
					map->map[i].count += map->map[i + 1].count;
				} else {
					n = map->map[i].count;
					for(j = 0; j < map->map[i + 1].count; j++) {
						for(k = 0; k < n; k++) {
							if(map->map[i].onumtable[k] == map->map[i + 1].onumtable[j]) break;
						}
						if(k == n) {
							if(map->map[i].count < MAXWORDMAP) {
								map->map[i].onumtable[map->map[i].count] = map->map[i + 1].onumtable[j];
							}
							map->map[i].count++;
						}
					}
				}
				memmove(&map->map[i + 1], &map->map[i + 2], (map->nmap - i - 2) * sizeof(struct wordmap_tally));
				map->nmap--;
			} else {
				i++;
			}
		}
	}

	return pred->nwordmap - 1;
}

static void find_wordmaps(struct program *prg, struct astnode **anptr, int pred_id) {
	int i;
	struct astnode *an;

	while((an = *anptr)) {
		if(an->kind == AN_DETERMINE_OBJECT) {
			assert(an->children[0]->kind == AN_VARIABLE);
			assert(an->children[0]->word->name[0]);
			if(an->children[3]->unbound) {
				an->value = -1;
			} else {
				an->value = compute_wordmap(
					prg,
					an->children[1],
					an->children[2],
					an->children[0]->word,
					prg->predicates[pred_id]->pred);
			}
		}
		for(i = 0; i < an->nchild; i++) {
			find_wordmaps(prg, &an->children[i], pred_id);
		}
		anptr = &an->next_in_body;
	}
}

void build_reverse_wordmaps(struct program *prg) {
	int i, j;
	struct predicate *pred;

	prg->nwordmappred = 0;
	for(i = 0; i < prg->npredicate; i++) {
		pred = prg->predicates[i]->pred;
		if(pred->flags & PREDF_INVOKED) {
			for(j = 0; j < pred->nclause; j++) {
				find_wordmaps(prg, &pred->clauses[j]->body, i);
			}
		}
	}
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
			if(an->predicate->pred->flags & PREDF_FAIL) return 1;
			break;
		case AN_NEG_RULE:
			if(an->predicate->pred->flags & PREDF_SUCCEEDS) return 1;
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
		case AN_LINK_SELF:
			if(!body_succeeds(an->children[0])) return 0;
			break;
		case AN_NEG_BLOCK:
			if(!body_fails(an->children[0])) return 0;
			break;
		case AN_RULE:
			if(!(an->predicate->pred->flags & PREDF_SUCCEEDS)) return 0;
			break;
		case AN_NEG_RULE:
			if(!(an->predicate->pred->flags & PREDF_FAIL)) return 0;
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
		case AN_SELECT:
			for(i = 0; i < an->nchild; i++) {
				if(!body_succeeds(an->children[i])) return 0;
			}
			break;
		case AN_COLLECT:
		case AN_COLLECT_WORDS:
		case AN_ACCUMULATE:
		case AN_DETERMINE_OBJECT:
			return 0;
			break;
		case AN_LINK:
		case AN_LINK_RES:
		case AN_OUTPUTBOX:
		case AN_STATUSAREA:
			if(!body_succeeds(an->children[1])) return 0;
			break;
		}
		an = an->next_in_body;
	}

	return 1;
}

int body_might_stop(struct astnode *an) {
	int i;

	while(an) {
		if(an->kind == AN_RULE || an->kind == AN_NEG_RULE) {
			if(an->predicate->pred->flags & PREDF_MIGHT_STOP) {
				return 1;
			}
		}
		if(an->kind != AN_STOPPABLE) {
			for(i = 0; i < an->nchild; i++) {
				if(body_might_stop(an->children[i])) return 1;
			}
		}
		an = an->next_in_body;
	}

	return 0;
}

int body_succeeds_at_most_once(struct astnode *an) {
	int i;

	while(an) {
		switch(an->kind) {
		case AN_BLOCK:
			if(!body_succeeds_at_most_once(an->children[0])) return 0;
			break;
		case AN_RULE:
			if(an->subkind == RULE_MULTI) return 0;
			break;
		case AN_OR:
			if(an->nchild > 1
			|| !body_succeeds_at_most_once(an->children[0])) {
				return 0;
			}
			break;
		case AN_IF:
			if(!body_succeeds_at_most_once(an->children[1])
			|| !body_succeeds_at_most_once(an->children[2])) {
				return 0;
			}
			break;
		case AN_DETERMINE_OBJECT:
			return 0;
		case AN_SELECT:
			for(i = 0; i < an->nchild; i++) {
				if(!body_succeeds_at_most_once(an->children[i])) {
					return 0;
				}
			}
			break;
		case AN_NEG_BLOCK:
		case AN_NEG_RULE:
		case AN_BAREWORD:
		case AN_DICTWORD:
		case AN_TAG:
		case AN_VARIABLE:
		case AN_INTEGER:
		case AN_PAIR:
		case AN_EMPTY_LIST:
		case AN_NOW:
		case AN_JUST:
		case AN_EXHAUST:
		case AN_FIRSTRESULT:
		case AN_COLLECT:
		case AN_COLLECT_WORDS:
		case AN_ACCUMULATE:
		case AN_STOPPABLE:
		case AN_STATUSAREA:
		case AN_OUTPUTBOX:
		case AN_LINK_SELF:
		case AN_LINK:
		case AN_LINK_RES:
			break;
		}
		an = an->next_in_body;
	}

	return 1;
}

static int add_ending(struct program *prg, char *utf8) {
	uint16_t unicode[64], ch;
	int len;
	struct endings_point *pt;
	struct endings_way **ways;
	int i;

	if(utf8[utf8_to_unicode(unicode, sizeof(unicode) / sizeof(uint16_t), (uint8_t *) utf8)]) {
		report(LVL_ERR, 0, "Word ending too long: '%s'", utf8);
		return 0;
	}

	for(len = 0; unicode[len]; len++);
	assert(len);

	pt = &prg->endings_root;
	while(len--) {
		ch = unicode[len];
		if(ch >= 'A' && ch <= 'Z') {
			ch += 'a' - 'A';
		} else if(ch >= 0x80) {
			ch = unicode_to_lower(ch);
		}
		for(i = 0; i < pt->nway; i++) {
			if(pt->ways[i]->letter == ch) {
				break;
			}
		}
		if(i == pt->nway) {
			pt->nway++;
			ways = arena_alloc(&prg->endings_arena, pt->nway * sizeof(struct endings_way *));
			memcpy(ways, pt->ways, i * sizeof(struct endings_way *));
			ways[i] = arena_calloc(&prg->endings_arena, sizeof(struct endings_way));
			ways[i]->letter = ch;
			pt->ways = ways;
		}
		if(len) {
			pt = &pt->ways[i]->more;
		} else {
			pt->ways[i]->final = 1;
		}
	}

	return 1;
}

static int build_endings_tree(struct program *prg) {
	struct predicate *pred;
	struct astnode *an;
	int i;

	arena_free(&prg->endings_arena);
	arena_init(&prg->endings_arena, 1024);
	memset(&prg->endings_root, 0, sizeof(prg->endings_root));

	pred = find_builtin(prg, BI_ENDINGS)->pred;
	for(i = 0; i < pred->nclause; i++) {
		for(an = pred->clauses[i]->body; an; an = an->next_in_body) {
			if(an->kind != AN_BAREWORD) {
				report(LVL_ERR, an->line, "Body of (removable word endings) may only contain simple words.");
				return 0;
			}
			if(!add_ending(prg, an->word->name)) {
				return 0;
			}
		}
	}

	return 1;
}

void frontend_add_builtins(struct program *prg) {
	int i, j;
	struct word *words[8];
	struct predname *predname;
	struct predicate *pred;

	for(i = 0; i < sizeof(specialspec) / sizeof(*specialspec); i++) {
		for(j = 0; j < specialspec[i].nword; j++) {
			words[j] = specialspec[i].word[j]?
				find_word(prg, specialspec[i].word[j])
				: 0;
		}
		predname = find_predicate(prg, specialspec[i].nword, words);
		predname->special = specialspec[i].id;
		predname->nameflags |= specialspec[i].prednameflags;
	}

	for(i = 0; i < sizeof(builtinspec) / sizeof(*builtinspec); i++) {
		for(j = 0; j < builtinspec[i].nword; j++) {
			words[j] = builtinspec[i].word[j]?
				find_word(prg, builtinspec[i].word[j])
				: 0;
		}
		predname = find_predicate(prg, builtinspec[i].nword, words);
		pred = predname->pred;
		predname->builtin = builtinspec[i].id;
		predname->nameflags |= builtinspec[i].prednameflags;
		pred->flags |= builtinspec[i].predflags | PREDF_INVOKED_MULTI | PREDF_INVOKED_SIMPLE;
		if(pred->flags & PREDF_DYNAMIC) {
			assert(predname->builtin == BI_HASPARENT);
			assert(predname->arity == 2);
			assert(prg->nobjvar == DYN_HASPARENT);
			predname->dyn_id = prg->nobjvar++;
			prg->objvarpred = realloc(prg->objvarpred, prg->nobjvar * sizeof(struct predname *));
			prg->objvarpred[predname->dyn_id] = predname;
			pred->dynamic = calloc(1, sizeof(struct dynamic));
		}
	}
}

static void add_to_buf(char **buf, int *nalloc, int *pos, char ch) {
	if(*pos >= *nalloc) {
		*nalloc = (*pos) * 2 + 32;
		*buf = realloc(*buf, *nalloc);
	}
	(*buf)[(*pos)++] = ch;
}

static int decode_output(char **bufptr, struct astnode *an, int *p_space, int *p_nl, struct arena *arena, char *context) {
	int post_space = 0, nl_printed = 0;
	int i;
	char last = 0;
	char *buf = 0;
	int nalloc = 0, pos = 0;
	char numbuf[8];

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
		} else if(an->kind == AN_INTEGER) {
			snprintf(numbuf, sizeof(numbuf), "%d", an->value);
			if(post_space) {
				add_to_buf(&buf, &nalloc, &pos, ' ');
			}
			for(i = 0; numbuf[i]; i++) {
				last = numbuf[i];
				add_to_buf(&buf, &nalloc, &pos, last);
			}
			nl_printed = 0;
			post_space = 1;
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
				add_to_buf(&buf, &nalloc, &pos, '\n');
				nl_printed++;
				post_space = 0;
			}
		} else if(an->kind == AN_RULE && an->predicate->builtin == BI_PAR) {
			while(nl_printed < 2) {
				add_to_buf(&buf, &nalloc, &pos, '\n');
				nl_printed++;
				post_space = 0;
			}
		} else {
			report(
				LVL_ERR,
				an->line,
				"%s may only consist of static text.",
				context);
			return 0;
		}
		an = an->next_in_body;
	}

	add_to_buf(&buf, &nalloc, &pos, 0);
	*bufptr = arena_strdup(arena, buf);
	free(buf);

	if(p_space) *p_space = post_space;
	if(p_nl) *p_nl = nl_printed;

	return 1;
}

static void frontend_inline(struct astnode **anptr, struct program *prg, struct clause *container) {
	struct astnode *an, *sub;

	while((an = *anptr)) {
		if(an->kind == AN_RULE
		&& (an->predicate->pred->flags & PREDF_MAY_INLINE)) {
			struct clause *def = an->predicate->pred->clauses[0];
			struct astnode *bindings[def->nvar];

			memset(bindings, 0, def->nvar * sizeof(struct astnode *));
			accesspred_bind_vars(def, an->children, bindings, prg, container->arena);
			sub = expand_macro_body(
				def->body,
				def,
				bindings,
				0,
				an->line,
				prg,
				container);
			assert(sub->kind == AN_RULE);
			if(an->subkind == RULE_SIMPLE) {
				sub->subkind = RULE_SIMPLE;
			}
			*anptr = sub;
			sub->next_in_body = an->next_in_body;
			anptr = &sub->next_in_body;
		} else {
			anptr = &an->next_in_body;
		}
	}
}

int frontend_visit_clauses(struct program *prg, struct arena *temp_arena, struct clause **first_ptr) {
	struct clause *cl, *sub, **clause_dest, **cld, *def;
	struct astnode *an;
	int i, j;
	char buf[32];
	struct predicate *pred;

	prg->errorflag = 0;
	for(clause_dest = first_ptr; (cl = *clause_dest); ) {
		if(cl->predicate->special == SP_GLOBAL_VAR) {
			if(cl->body
			&& cl->body->kind == AN_RULE
			&& !cl->body->next_in_body
			&& cl->body->predicate->arity == 1) {
				/* presumably of the form (global variable (inner declaration ...)) */
				if(cl->body->children[0]->kind != cl->params[0]->kind
				|| (cl->params[0]->kind == AN_VARIABLE && cl->body->children[0]->word != cl->params[0]->word)) {
					/* somebody is attempting e.g. (global variable $X) (inner declaration $Y) */
					report(LVL_ERR, cl->line, "Syntax error in global variable declaration.");
					return 0;
				}
				if(cl->body->predicate->builtin) {
					report(LVL_ERR, cl->line, "Global variable declaration collides with built-in predicate.");
					return 0;
				}
				if(cl->body->predicate->pred->flags & PREDF_MACRO) {
					report(LVL_ERR, cl->line, "Global variable collides with access predicate of the same name.");
					return 0;
				}
				if(cl->body->predicate->pred->flags & PREDF_GLOBAL_VAR) {
					report(LVL_WARN, cl->line, "Multiple declarations of the same global variable.");
				}
				if(!(cl->body->predicate->pred->flags & PREDF_GLOBAL_VAR)) {
					cl->body->predicate->pred->flags |= PREDF_DYNAMIC | PREDF_GLOBAL_VAR;
					if(cl->body->predicate->dyn_var_id == DYN_NONE) {
						cl->body->predicate->dyn_var_id = prg->nglobalvar++;
						prg->globalvarpred = realloc(prg->globalvarpred, prg->nglobalvar * sizeof(struct predname *));
						prg->globalvarpred[cl->body->predicate->dyn_var_id] = cl->body->predicate;
					}
					if(!cl->body->predicate->pred->dynamic) {
						cl->body->predicate->pred->dynamic = calloc(1, sizeof(struct dynamic));
					}
				}
				if(cl->body->children[0]->kind != AN_VARIABLE) {
					/* (global variable (inner declaration #foo)) so we add a separate rule for the initial value */
					sub = arena_calloc(&cl->body->predicate->pred->arena, sizeof(*sub));
					sub->predicate = cl->body->predicate;
					sub->arena = &cl->body->predicate->pred->arena;
					sub->params = cl->body->children;
					sub->body = 0;
					sub->line = cl->body->line;
					sub->next_in_source = cl->next_in_source;
					cl->next_in_source = sub;
				}
				clause_dest = &cl->next_in_source;
			} else {
				report(LVL_ERR, cl->line, "Syntax error in global variable declaration.");
				return 0;
			}
		} else if(cl->predicate->special == SP_GENERATE) {
			if(cl->body
			&& cl->body->kind == AN_RULE
			&& !cl->body->next_in_body
			&& cl->body->predicate->arity) {
				cld = clause_dest;

				/* presumably of the form (generate N of (inner declaration ...)) */
				if(cl->params[0]->kind != AN_INTEGER
				|| cl->body->children[0]->kind != cl->params[1]->kind
				|| (cl->params[1]->kind == AN_VARIABLE && cl->body->children[0]->word != cl->params[1]->word)) {
					/* somebody is attempting e.g. (generate $X of $Y) (inner declaration $Z) */
					report(LVL_ERR, cl->line, "Syntax error in (generate $ $) declaration.");
					return 0;
				}
				for(i = 0; i < cl->params[0]->value; i++) {
					sub = calloc(1, sizeof(*sub));
					sub->predicate = cl->body->predicate;
					sub->arena = &cl->body->predicate->pred->arena;
					sub->params = malloc(sub->predicate->arity * sizeof(struct astnode *));
					sub->params[0] = mkast(AN_TAG, 0, sub->arena, cl->line);
					snprintf(buf, sizeof(buf), "%d", prg->nworldobj + 1);
					sub->params[0]->word = find_word(prg, buf);
					create_worldobj(prg, sub->params[0]->word);
					for(j = 1; j < cl->body->predicate->arity; j++) {
						sub->params[j] = deepcopy_astnode(cl->body->children[j], sub->arena, cl->line);
					}
					sub->line = cl->line;
					*cld = sub;
					cld = &sub->next_in_source;
				}
				*cld = cl->next_in_source;
			} else {
				report(LVL_ERR, cl->line, "Syntax error in (generate $ $) declaration.");
				return 0;
			}
		} else if(cl->predicate->builtin == BI_RESOURCEDEF) {
			char *body, *url, *stem, *alt, *ptr;
			int id, len;

			if(!decode_output(&body, cl->body, 0, 0, &cl->predicate->pred->arena, "Resource definition body")) {
				return 0;
			}
			id = prg->nresource++;
			prg->resources = realloc(prg->resources, prg->nresource * sizeof(struct extresource));
			alt = strchr(body, ';');
			if(alt) {
				*alt++ = 0;
				while(*alt == ' ' || *alt == 9) alt++;
			}
			while(*body == ' ' || *body == 9) body++;
			while((len = strlen(body)) && (body[len - 1] == ' ' || body[len - 1] == 9)) {
				body[len - 1] = 0;
			}
			url = body;
			stem = body;
			if(!strncmp(body, "http:", 5)
			|| !strncmp(body, "https:", 6)) {
				prg->resources[id].path = 0;
			} else if(!strncmp(body, "mailto:", 7)) {
				prg->resources[id].path = 0;
				stem += 7;
			} else {
				prg->resources[id].path = body;
				while(
					(ptr = strchr(stem, '/')) ||
					(ptr = strchr(stem, '\\')) ||
					(ptr = strchr(stem, ':')))
				{
					stem = ptr + 1;
				}
				len = 5 + strlen(stem) + 1;
				url = arena_alloc(&cl->predicate->pred->arena, len);
				snprintf(url, len, "file:%s", stem);
			}
			if(!alt) {
				alt = stem;
			}
			prg->resources[id].url = url;
			prg->resources[id].stem = stem;
			prg->resources[id].options = "";
			prg->resources[id].alt = alt;
			prg->resources[id].line = cl->line;
			sub = arena_calloc(&cl->predicate->pred->arena, sizeof(*sub));
			sub->predicate = find_builtin(prg, BI_RESOLVERESOURCE);
			sub->arena = &cl->predicate->pred->arena;
			sub->params = arena_alloc(sub->arena, 2 * sizeof(struct astnode *));
			sub->params[0] = deepcopy_astnode(cl->params[0], &cl->predicate->pred->arena, cl->line);
			sub->params[1] = mkast(AN_INTEGER, 0, &cl->predicate->pred->arena, cl->line);
			sub->params[1]->value = id;
			sub->line = cl->line;
			sub->next_in_source = cl->next_in_source;
			cl->next_in_source = sub;
			clause_dest = &cl->next_in_source;
		} else if(cl->predicate->special == SP_INTERFACE) {
			if(!cl->body || cl->body->kind != AN_RULE || cl->body->predicate->special) {
				report(LVL_ERR, cl->line, "Syntax error in interface declaration.");
				return 0;
			}
			an = cl->body;
			pred = an->predicate->pred;
			if(pred->iface_decl) {
				report(LVL_ERR, cl->line, "Interface already declared for '%s' at %s:%d.",
					an->predicate->printed_name,
					FILEPART(pred->iface_decl->line),
					LINEPART(pred->iface_decl->line));
				return 0;
			}
			pred->iface_decl = cl;
			for(i = 0; i < an->nchild; i++) {
				if(an->children[i]->kind != AN_VARIABLE) {
					report(LVL_ERR, cl->line,
						"Interface declaration parameters must be a variables.");
					return 0;
				}
				if(an->children[i]->word->name[0] == '<') {
					pred->iface_bound_in |= 1 << i;
				} else if(an->children[i]->word->name[0] == '>') {
					pred->iface_bound_out |= 1 << i;
				}
			}
			clause_dest = &cl->next_in_source;
		} else if(cl->predicate->pred->flags & PREDF_MACRO) {
			cld = clause_dest;
			def = accesspred_find_def(cl->predicate->pred, cl->params);
			if(!def) {
				report(LVL_ERR, cl->line, "Couldn't find a matching access predicate rule for @%s.", cl->predicate->printed_name);
				prg->errorflag = 1;
				clause_dest = &cl->next_in_source;
			} else {
				struct astnode *bindings[def->nvar];
				memset(bindings, 0, def->nvar * sizeof(struct astnode *));
				accesspred_bind_vars(def, cl->params, bindings, prg, temp_arena);
				an = expand_macro_body(
					def->body,
					def,
					bindings,
					0,
					cl->line,
					prg,
					cl);
				for(; an; an = an->next_in_body) {
					if(an->kind == AN_RULE || an->kind == AN_NEG_RULE) {
						if(an->predicate->builtin
						&& an->predicate->builtin != BI_HASPARENT
						&& !(an->predicate->nameflags & PREDNF_DEFINABLE_BI)) {
							report(LVL_ERR, cl->line, "Access predicate may not expand into a redefinition of a builtin.");
							return 0;
						}
						sub = mkclause(an->predicate->pred);
						for(i = 0; i < sub->predicate->arity; i++) {
							sub->params[i] = deepcopy_astnode(an->children[i], sub->arena, cl->line);
						}
						sub->body = deepcopy_astnode(cl->body, sub->arena, cl->line);
						sub->line = cl->line;
						sub->negated = (an->kind == AN_NEG_RULE);
						sub->macro_instance = ++cl->macro_instance;
						*cld = sub;
						cld = &sub->next_in_source;
					} else {
						report(LVL_ERR, cl->line, "Access predicate must expand into a conjunction of queries.");
						return 0;
					}
				}
				*cld = cl->next_in_source;
			}
		} else {
			cl->predicate->pred->flags |= PREDF_DEFINED;
			add_clause(cl, cl->predicate->pred);
			cl->body = expand_macros(cl->body, prg, cl);
			clause_dest = &cl->next_in_source;
		}
	}

	pred = find_builtin(prg, BI_INVOKE_CLOSURE)->pred;
	for(i = 0; i < pred->nclause; i++) {
		pred->clauses[i]->body = expand_macros(pred->clauses[i]->body, prg, pred->clauses[i]);
	}

	if(verbose >= 3) {
		for(i = 0; i < prg->nresource; i++) {
			printf("Resource %d:", i);
			printf("URL \"%s\"", prg->resources[i].url);
			if(prg->resources[i].path) {
				printf(", local file \"%s\", stem \"%s\"",
					prg->resources[i].path,
					prg->resources[i].stem);
			}
			printf(", alt \"%s\"\n", prg->resources[i].alt);
		}
	}

	return !prg->errorflag;
}

static void frontend_reset_program(struct program *prg) {
	int i, j;
	struct predname *predname;
	struct clause *cl;
	struct astnode *sub;

	for(i = 0; i < prg->npredicate; i++) {
		predname = prg->predicates[i];

		if(!predname->special) {
			pred_clear(predname);
		}
	}

	for(i = 0; i < sizeof(builtinspec) / sizeof(*builtinspec); i++) {
		predname = find_builtin(prg, builtinspec[i].id);
		predname->pred->flags |=
			builtinspec[i].predflags |
			PREDF_INVOKED_MULTI |
			PREDF_INVOKED_SIMPLE;
		if(predname->builtin == BI_HASPARENT) {
			assert(predname->dyn_id == DYN_HASPARENT);
			predname->pred->dynamic = calloc(1, sizeof(struct dynamic));
		}
	}

	predname = find_builtin(prg, BI_INVOKE_CLOSURE);
	for(i = 0; i < prg->nclosurebody; i++) {
		cl = mkclause(predname->pred);
		cl->line = 0;
		cl->params[0] = mkast(AN_INTEGER, 0, cl->arena, 0);
		cl->params[0]->value = i;
		cl->params[1] = mkast(AN_EMPTY_LIST, 0, cl->arena, 0);
		cl->params[2] = mkast(AN_VARIABLE, 0, cl->arena, 0);
		cl->params[2]->word = find_word(prg, "");
		cl->body = deepcopy_astnode(prg->closurebodies[i], cl->arena, 0);
		add_clause(cl, predname->pred);
		analyse_clause(prg, cl, 0);
		assert(cl == predname->pred->clauses[i]);
		for(j = 0; j < cl->nvar; j++) {
			if(!strcmp(cl->varnames[j]->name, "_")) {
				cl->params[2]->word = find_word(prg, "_");
			} else {
				sub = cl->params[1];
				cl->params[1] = mkast(AN_PAIR, 2, cl->arena, 0);
				cl->params[1]->children[0] = mkast(AN_VARIABLE, 0, cl->arena, 0);
				cl->params[1]->children[0]->word = cl->varnames[j];
				cl->params[1]->children[1] = sub;
			}
		}
	}
}

static int nselectform;
static int nalloc_selectform;
static struct selectform *selectforms;

static void match_select_forms(struct selectform *oldform, int n_old, struct selectform *newform, int n_new) {
	uint16_t cost[n_old + 1][n_new + 1], bestcost;
	char path[n_old + 1][n_new + 1], bestpath;
	int i, j;

	for(i = n_old; i >= 0; i--) {
		for(j = n_new; j >= 0; j--) {
			bestcost = 0xffff;
			bestpath = 0;
			if(i == n_old && j == n_new) {
				// match end-of-list with end-of-list at zero cost
				bestcost = 0;
				bestpath = 'e';
			}
			if(j < n_new) {
				// we could insert a new clause here, at a cost of 1
				if(cost[i][j + 1] + 1 < bestcost) {
					bestcost = cost[i][j + 1] + 1;
					bestpath = 'i';
				}
			}
			if(i < n_old) {
				// we could remove an old clause here, at a cost of 1
				if(cost[i + 1][j] + 1 < bestcost) {
					bestcost = cost[i + 1][j] + 1;
					bestpath = 'r';
				}
			}
			if(i < n_old && j < n_new) {
				if(oldform[i].subkind == newform[j].subkind
				&& oldform[i].nchild == newform[j].nchild) {
					// a match is possible here, at zero cost
					if(cost[i + 1][j + 1] < bestcost) {
						bestcost = cost[i + 1][j + 1];
						bestpath = 'm';
					}
				}
			}
			cost[i][j] = bestcost;
			path[i][j] = bestpath;
		}
	}

	i = j = 0;
	while(i < n_old || j < n_new) {
		switch(path[i][j]) {
		case 'i':
			//printf("generate new for %04x\n", j);
			newform[j++].assigned_id = 0xffff;
			break;
		case 'r':
			i++;
			break;
		case 'm':
			//printf("copy old %04x to new %04x\n", i, j);
			newform[j++].assigned_id = oldform[i++].assigned_id;
			break;
		default:
			assert(0); exit(1);
		}
	}
}

static void discover_select_statements(struct program *prg, struct astnode *an) {
	int i;

	while(an) {
		if(an->kind == AN_SELECT) {
			if(nselectform >= nalloc_selectform) {
				nalloc_selectform = 2 * nselectform + 8;
				selectforms = realloc(selectforms, nalloc_selectform * sizeof(struct selectform));
			}
			selectforms[nselectform].subkind = an->subkind;
			selectforms[nselectform].nchild = an->nchild;
			selectforms[nselectform].assigned_id = 0xffff;
			nselectform++;
		}
		for(i = 0; i < an->nchild; i++) {
			discover_select_statements(prg, an->children[i]);
		}
		an = an->next_in_body;
	}
}

static void update_select_statements(struct program *prg, struct astnode *an, struct selectform *table, int *next) {
	int i;

	while(an) {
		if(an->kind == AN_SELECT) {
			assert(an->value == -1);
			an->value = table[(*next)++].assigned_id;
			assert(an->value != -1);
		}
		for(i = 0; i < an->nchild; i++) {
			update_select_statements(prg, an->children[i], table, next);
		}
		an = an->next_in_body;
	}
}

static void assign_select_statements(struct program *prg) {
	int i, j, next;
	struct predname *predname;
	struct predicate *pred;

	for(i = 0; i < prg->npredicate; i++) {
		predname = prg->predicates[i];
		if(!predname->special) {
			pred = predname->pred;
			nselectform = 0;
			for(j = 0; j < pred->nclause; j++) {
				discover_select_statements(prg, pred->clauses[j]->body);
			}
			assert(!pred->selectforms);
			pred->nselectform = nselectform;
			pred->selectforms = arena_alloc(&pred->arena, nselectform * sizeof(struct selectform));
			memcpy(pred->selectforms, selectforms, nselectform * sizeof(struct selectform));
			if(predname->old_pred && predname->old_pred->nselectform) {
				match_select_forms(
					predname->old_pred->selectforms,
					predname->old_pred->nselectform,
					pred->selectforms,
					nselectform);
			}
			for(j = 0; j < nselectform; j++) {
				if(pred->selectforms[j].assigned_id == 0xffff) {
					if(prg->nselect >= prg->nalloc_select) {
						prg->nalloc_select = 2 * prg->nselect + 8;
						prg->select = realloc(prg->select, prg->nalloc_select);
					}
					pred->selectforms[j].assigned_id = prg->nselect;
					prg->select[prg->nselect++] = 0;
				}
			}
			next = 0;
			for(j = 0; j < pred->nclause; j++) {
				update_select_statements(prg, pred->clauses[j]->body, pred->selectforms, &next);
			}
		}
	}

	free(selectforms);
	nalloc_selectform = 0;
	selectforms = 0;
}

char *decode_metadata_str(int builtin, struct word *param, struct program *prg, struct arena *arena) {
	struct predname *predname;
	struct predicate *pred;
	char *buf;
	int i;

	predname = find_builtin(prg, builtin);
	pred = predname->pred;
	for(i = 0; i < pred->nclause; i++) {
		if(!param
		|| (pred->clauses[i]->params[0]->kind == AN_DICTWORD && pred->clauses[i]->params[0]->word == param)) {
			if(decode_output(&buf, pred->clauses[i]->body, 0, 0, arena, "Story metadata")) {
				return buf;
			} else {
				return 0;
			}
		}
	}

	return 0;
}

int frontend(struct program *prg, int nfile, char **fname, dictmap_callback_t dictmap_callback) {
	struct clause **clause_dest, *first_clause, *cl;
	struct predname *predname;
	struct predicate *pred;
	struct astnode *an, **anptr;
	int fnum, i, j, k, m;
	int flag;
	struct lexer lexer = {0};
	struct eval_state es;
	int success;

	for(i = 0; i < prg->npredicate; i++) {
		predname = prg->predicates[i];
		if(!predname->special) {
			pred_release(predname->old_pred);
			if(predname->pred->nselectform) {
				predname->old_pred = predname->pred;
				pred_claim(predname->old_pred);
			} else {
				predname->old_pred = 0;
			}
			pred_clear(predname);
		}
	}

	frontend_reset_program(prg);

	arena_init(&lexer.temp_arena, 16384);
	lexer.program = prg;

	nsourcefile = nfile;
	sourcefile = fname;

	lexer.lib_file = -1;
	clause_dest = &first_clause;
	for(fnum = 0; fnum < nfile; fnum++) {
		lexer.file = fopen(fname[fnum], "r");
		if(!lexer.file) {
			report(LVL_ERR, 0, "Failed to open \"%s\": %s", fname[fnum], strerror(errno));
			arena_free(&lexer.temp_arena);
			frontend_reset_program(prg);
			return 0;
		}
		success = parse_file(&lexer, fnum, &clause_dest);
		fclose(lexer.file);
		if(!success) {
			arena_free(&lexer.temp_arena);
			frontend_reset_program(prg);
			return 0;
		}
	}
	*clause_dest = 0;

	report(LVL_INFO, 0, "Total word count: %d", lexer.totalwords);

	if(lexer.lib_file < 0) {
		report(LVL_WARN, 0, "No library (such as stdlib.dg) was specified on the commandline.");
	} else if(lexer.lib_file != nfile - 1) {
		report(LVL_WARN, 0, "The library (in this case %s) should normally appear last on the commandline.", sourcefile[lexer.lib_file]);
	}

	if(verbose >= 4) {
		struct word *w;

		for(i = 0; i < WORDBUCKETS; i++) {
			for(w = prg->wordhash[i]; w; w = w->next_in_hash) {
				if(w->flags & (WORDF_OUTPUT | WORDF_DICT)) {
					printf("Output: %s\n", w->name);
				}
			}
		}
	}

	if(!frontend_visit_clauses(prg, &lexer.temp_arena, &first_clause)) {
		arena_free(&lexer.temp_arena);
		frontend_reset_program(prg);
		return 0;
	}

	predname = find_builtin(prg, BI_QUERY);
	cl = mkclause(predname->pred);
	cl->params[0] = mkast(AN_PAIR, 2, cl->arena, 0);
	cl->params[0]->children[0] = mkast(AN_VARIABLE, 0, cl->arena, 0);
	cl->params[0]->children[0]->word = find_word(prg, "Head");
	cl->params[0]->children[1] = mkast(AN_VARIABLE, 0, cl->arena, 0);
	cl->params[0]->children[1]->word = find_word(prg, "Tail");
	cl->body = mkast(AN_RULE, 3, cl->arena, 0);
	cl->body->subkind = RULE_MULTI;
	cl->body->predicate = find_builtin(prg, BI_INVOKE_CLOSURE);
	cl->body->children[0] = deepcopy_astnode(cl->params[0]->children[0], cl->arena, 0);
	cl->body->children[1] = deepcopy_astnode(cl->params[0]->children[1], cl->arena, 0);
	cl->body->children[2] = mkast(AN_EMPTY_LIST, 0, cl->arena, 0);
	add_clause(cl, predname->pred);

	predname = find_builtin(prg, BI_QUERY_ARG);
	cl = mkclause(predname->pred);
	cl->params[0] = mkast(AN_PAIR, 2, cl->arena, 0);
	cl->params[0]->children[0] = mkast(AN_VARIABLE, 0, cl->arena, 0);
	cl->params[0]->children[0]->word = find_word(prg, "Head");
	cl->params[0]->children[1] = mkast(AN_VARIABLE, 0, cl->arena, 0);
	cl->params[0]->children[1]->word = find_word(prg, "Tail");
	cl->params[1] = mkast(AN_VARIABLE, 0, cl->arena, 0);
	cl->params[1]->word = find_word(prg, "Arg");
	cl->body = mkast(AN_RULE, 3, cl->arena, 0);
	cl->body->subkind = RULE_MULTI;
	cl->body->predicate = find_builtin(prg, BI_INVOKE_CLOSURE);
	cl->body->children[0] = deepcopy_astnode(cl->params[0]->children[0], cl->arena, 0);
	cl->body->children[1] = deepcopy_astnode(cl->params[0]->children[1], cl->arena, 0);
	cl->body->children[2] = deepcopy_astnode(cl->params[1], cl->arena, 0);
	add_clause(cl, predname->pred);

	predname = find_builtin(prg, BI_EMBEDRESOURCE);
	cl = mkclause(predname->pred);
	cl->params[0] = mkast(AN_VARIABLE, 0, cl->arena, 0);
	cl->params[0]->word = find_word(prg, "Id");
	cl->body = mkast(AN_RULE, 2, cl->arena, 0);
	cl->body->subkind = RULE_SIMPLE;
	cl->body->predicate = find_builtin(prg, BI_RESOLVERESOURCE);
	cl->body->children[0] = deepcopy_astnode(cl->params[0], cl->arena, 0);
	cl->body->children[1] = mkast(AN_VARIABLE, 0, cl->arena, 0);
	cl->body->children[1]->word = find_word(prg, "Num");
	cl->body->next_in_body = mkast(AN_RULE, 1, cl->arena, 0);
	cl->body->next_in_body->subkind = RULE_SIMPLE;
	cl->body->next_in_body->predicate = find_builtin(prg, BI_EMBED_INTERNAL);
	cl->body->next_in_body->children[0] = deepcopy_astnode(cl->body->children[1], cl->arena, 0);
	add_clause(cl, predname->pred);

	predname = find_builtin(prg, BI_CAN_EMBED);
	cl = mkclause(predname->pred);
	cl->params[0] = mkast(AN_VARIABLE, 0, cl->arena, 0);
	cl->params[0]->word = find_word(prg, "Id");
	cl->body = mkast(AN_RULE, 2, cl->arena, 0);
	cl->body->subkind = RULE_SIMPLE;
	cl->body->predicate = find_builtin(prg, BI_RESOLVERESOURCE);
	cl->body->children[0] = deepcopy_astnode(cl->params[0], cl->arena, 0);
	cl->body->children[1] = mkast(AN_VARIABLE, 0, cl->arena, 0);
	cl->body->children[1]->word = find_word(prg, "Num");
	cl->body->next_in_body = mkast(AN_RULE, 1, cl->arena, 0);
	cl->body->next_in_body->subkind = RULE_SIMPLE;
	cl->body->next_in_body->predicate = find_builtin(prg, BI_CAN_EMBED_INTERNAL);
	cl->body->next_in_body->children[0] = deepcopy_astnode(cl->body->children[1], cl->arena, 0);
	add_clause(cl, predname->pred);

	for(i = 0; i < prg->npredicate; i++) {
		pred = prg->predicates[i]->pred;
		for(j = 0; j < pred->nclause; j++) {
			if(pred->clauses[j]->negated) {
				for(anptr = &pred->clauses[j]->body; *anptr; anptr = &(*anptr)->next_in_body);
				*anptr = mkast(AN_JUST, 0, pred->clauses[j]->arena, pred->clauses[j]->line);
				(*anptr)->next_in_body = an = mkast(AN_RULE, 0, pred->clauses[j]->arena, pred->clauses[j]->line);
				an->predicate = find_builtin(prg, BI_FAIL);
				pred->flags |= PREDF_CONTAINS_JUST;
			} else {
				if(!(pred->flags & PREDF_CONTAINS_JUST)
				&& contains_just(pred->clauses[j]->body)) {
					pred->flags |= PREDF_CONTAINS_JUST;
				}
			}
		}
	}

	success = 1;
	for(i = 0; i < prg->npredicate; i++) {
		pred = prg->predicates[i]->pred;
		for(j = 0; j < pred->nclause; j++) {
			success &= find_dynamic(
				prg,
				pred->clauses[j]->body,
				pred->clauses[j]->line);
		}
	}
	if(!success) {
		arena_free(&lexer.temp_arena);
		frontend_reset_program(prg);
		return 0;
	}

#if 0
	for(i = 0; i < prg->npredicate; i++) {
		if(!prg->predicates[i]->special
		&& !(prg->predicates[i]->pred->flags & PREDF_MACRO)
		&& !(prg->predicates[i]->pred->flags & PREDF_DYNAMIC)) {
			printf("%s %s %d\n",
				(prg->predicates[i]->pred->flags & PREDF_CONTAINS_JUST)? "J" : " ",
				prg->predicates[i]->printed_name,
				prg->predicates[i]->pred->nclause);
		}
	}
#endif

	for(i = 0; i < prg->npredicate; i++) {
		predname = prg->predicates[i];
		pred = predname->pred;
		for(j = 0; j < pred->nclause; j++) {
			analyse_clause(prg, pred->clauses[j], 1);
		}
	}

	if(prg->optflags & OPTF_INLINE) {
		for(i = 0; i < prg->npredicate; i++) {
			pred = prg->predicates[i]->pred;
			if(pred->nclause == 1
			&& !(pred->flags & PREDF_DYNAMIC)) {
				cl = pred->clauses[0];
				for(j = 0; j < pred->predname->arity; j++) {
					if(cl->params[j]->kind != AN_VARIABLE) break;
					if(cl->params[j]->word->name[0]) {
						for(k = 0; k < j; k++) {
							if(cl->params[j]->word == cl->params[k]->word) {
								break;
							}
						}
						if(k < j) break;
					}
				}
				if(j == pred->predname->arity
				&& (an = cl->body)
				&& an->kind == AN_RULE
				&& !(an->predicate->builtin == BI_FAIL)
				&& !an->next_in_body) {
					for(j = 0; j < an->nchild; j++) {
						if(an->children[j]->kind != AN_VARIABLE) {
							break;
						}
						for(k = 0; k < pred->predname->arity; k++) {
							if(an->children[j]->word == cl->params[k]->word) {
								break;
							}
						}
						if(an->children[j]->word->name[0]
						&& k == pred->predname->arity) {
							break;
						}
					}
					if(j == an->nchild) {
						pred->flags |= PREDF_MAY_INLINE;
					}
				}
			}
		}

		for(i = 0; i < prg->npredicate; i++) {
			pred = prg->predicates[i]->pred;
			for(j = 0; j < pred->nclause; j++) {
				frontend_inline(&pred->clauses[j]->body, prg, pred->clauses[j]);
			}
		}
	}

	for(i = 0; i < prg->npredicate; i++) {
		predname = prg->predicates[i];
		pred = predname->pred;
		if(!predname->special
		&& !(predname->builtin && !(predname->nameflags & PREDNF_DEFINABLE_BI))
		&& !pred->dynamic
		&& !(pred->nclause)) {
			pred->flags |= PREDF_FAIL;
		}
	}

	do {
		flag = 0;
		for(i = 0; i < prg->npredicate; i++) {
			predname = prg->predicates[i];
			if(!predname->special
			&& !(predname->builtin && !(predname->nameflags & PREDNF_DEFINABLE_BI))) {
				pred = predname->pred;
				for(j = 0; j < pred->nclause; j++) {
					if(!pred->dynamic
					&& pred->clauses[j]->body
					&& pred->clauses[j]->body->kind == AN_RULE
					&& (pred->clauses[j]->body->predicate->pred->flags & PREDF_FAIL)) {
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
		}
	} while(flag);

	assign_select_statements(prg);
	trace_invocations(prg);
	find_fixed_flags(prg);

	build_dictionary(prg);
	if(dictmap_callback) {
		dictmap_callback(prg);
	}
	build_reverse_wordmaps(prg);

	do {
		flag = 0;
		for(i = 0; i < prg->npredicate; i++) {
			predname = prg->predicates[i];
			pred = predname->pred;
			if((!predname->builtin || (predname->nameflags & PREDNF_DEFINABLE_BI))
			&& !pred->dynamic
			&& !(pred->flags & PREDF_SUCCEEDS)) {
				for(j = 0; j < pred->nclause; j++) {
					for(k = 0; k < predname->arity; k++) {
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
					if(k == predname->arity) {
						if(body_succeeds(pred->clauses[j]->body)) {
#if 0
							printf("Succeeds: %s\n", predname->printed_name);
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

	do {
		flag = 0;
		for(i = 0; i < prg->npredicate; i++) {
			predname = prg->predicates[i];
			pred = predname->pred;
			if((!predname->builtin || (predname->nameflags & PREDNF_DEFINABLE_BI))
			&& !pred->dynamic
			&& !(pred->flags & PREDF_MIGHT_STOP)) {
				for(j = 0; j < pred->nclause; j++) {
					if(body_might_stop(pred->clauses[j]->body)) {
						pred->flags |= PREDF_MIGHT_STOP;
						flag = 1;
						break;
					}
				}
			}
		}
	} while(flag);

	if(verbose >= 3) {
		for(i = 0; i < prg->npredicate; i++) {
			if(!prg->predicates[i]->special
			&& !(prg->predicates[i]->pred->flags & PREDF_MACRO)) {
				pp_predicate(prg->predicates[i], prg);
			}
		}
	}

#if 0
	for(i = 0; i < prg->nworldobj; i++) {
		printf("%5d %s\n", i + 1, prg->worldobjnames[i]->name);
	}
#endif

#if 0
	for(i = 0; i < npredicate; i++) {
		predname = predicates[i];
		if(predname->builtin != BI_HASPARENT) {
			struct dynamic *dyn = predname->pred->dynamic;
			if(dyn) {
				if(predname->pred->flags & PREDF_DYN_LINKAGE) {
					printf("Needs dynamic linkage: %s", predname->printed_name);
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
		predname = predicates[i];
		if(!predname->special) {
			printf("%c %c %s\n",
				(predname->pred->flags & PREDF_INVOKED_SIMPLE)? 'S' : '-',
				(predname->pred->flags & PREDF_INVOKED_MULTI)? 'M' : '-',
				predname->printed_name);
		}
	}
#endif

	if(find_builtin(prg, BI_TRACE_ON)->pred->flags & PREDF_INVOKED) {
		prg->optflags &= ~OPTF_NO_TRACE;
	}

	prg->errorflag = 0;
	comp_builtins(prg);
	comp_program(prg);
	comp_cleanup();

	for(i = 0; i < prg->nboxclass; i++) {
		struct boxclass *bc = &prg->boxclasses[i];
		char *css = decode_metadata_str(BI_STYLEDEF, bc->class, prg, &lexer.temp_arena);
		char *param, *str;
		struct boxclassline *bcl, **bclptr;

		bc->width = 100;
		bc->height = 1;
		bc->flags = BOXF_RELWIDTH;
		bc->margintop = 0;
		bc->marginbottom = 0;
		bclptr = &bc->css_lines;
		if(css) {
			param = arena_alloc(&lexer.temp_arena, strlen(css) + 1);
			while((str = strtok(css, ";"))) {
				while(strchr(" \t\r\n", *str)) str++;

				bcl = arena_alloc(&prg->arena, sizeof(*bcl));
				bcl->data = arena_strdup(&prg->arena, str);
				*bclptr = bcl;
				bclptr = &bcl->next;

				for(j = 0; str[j]; j++) {
					if(str[j] >= 'A' && str[j] <= 'Z') str[j] ^= ' ';
				}

				if(2 == sscanf(str, "width : %d %s", &j, param)) {
					if(!strcmp(param, "%")) {
						bc->width = j;
						bc->flags |= BOXF_RELWIDTH;
					} else if(!strcmp(param, "em") || !strcmp(param, "ch")) {
						bc->width = j;
						bc->flags &= ~BOXF_RELWIDTH;
					}
				} else if(2 == sscanf(str, "height : %d %s", &j, param)) {
					if(!strcmp(param, "%")) {
						bc->height = j;
						bc->flags |= BOXF_RELHEIGHT;
					} else if(!strcmp(param, "em")) {
						bc->height = j;
						bc->flags &= ~BOXF_RELHEIGHT;
					}
				} else if(2 == sscanf(str, "margin-top : %d %s", &j, param)) {
					if(!strcmp(param, "em")) {
						bc->margintop = j;
					}
				} else if(2 == sscanf(str, "margin-bottom : %d %s", &j, param)) {
					if(!strcmp(param, "em")) {
						bc->marginbottom = j;
					}
				} else if(1 == sscanf(str, "float : %s", param)) {
					if(!strcmp(param, "left")) {
						bc->flags &= ~BOXF_FLOATRIGHT;
						bc->flags |= BOXF_FLOATLEFT;
					} else if(!strcmp(param, "right")) {
						bc->flags &= ~BOXF_FLOATLEFT;
						bc->flags |= BOXF_FLOATRIGHT;
					}
				} else if(1 == sscanf(str, "font-style : %s", param)) {
					if(!strcmp(param, "italic") || !strcmp(param, "oblique")) {
						bc->style = STYLE_ITALIC;
					}
				} else if(1 == sscanf(str, "font-weight : %s", param)) {
					if(!strcmp(param, "bold")) {
						bc->style = STYLE_BOLD;
					}
				} else if(1 == sscanf(str, "font-family : %s", param)) {
					// %s stops at first whitespace, so use str:
					if(strstr(str, "monospace")) {
						bc->style = STYLE_FIXED;
					}
				}

				css = 0;
			}
		}
		*bclptr = 0;
		if(verbose >= 3) {
			printf("Box class @%s:\n", bc->class->name);
			printf("\tWidth:\t%d%s\n", bc->width, (bc->flags & BOXF_RELWIDTH)? "%" : " ch");
			printf("\tHeight:\t%d%s\n", bc->height, (bc->flags & BOXF_RELHEIGHT)? "%" : " em");
			printf("\tMargin-top:\t%d em\n", bc->margintop);
			printf("\tMargin-bottom:\t%d em\n", bc->marginbottom);
			printf("\tFloat:\t%s\n",
				(bc->flags & BOXF_FLOATLEFT)? "left" :
				(bc->flags & BOXF_FLOATRIGHT)? "right" : "none");
			printf("\tStyle:\t%s\n",
				(bc->style == STYLE_ITALIC)? "italic" :
				(bc->style == STYLE_BOLD)? "bold" : "inherit");
			for(bcl = bc->css_lines; bcl; bcl = bcl->next) {
				printf("\tCSS: \"%s\"\n", bcl->data);
			}
		}
	}

	arena_free(&lexer.temp_arena);

	if(prg->errorflag || !build_endings_tree(prg)) {
		frontend_reset_program(prg);
		return 0;
	}

	success = 1;
	init_evalstate(&es, prg);
	//es.trace = 1;
	for(i = 0; i < prg->npredicate; i++) {
		predname = prg->predicates[i];
		free(predname->fixedvalues);
		predname->fixedvalues = 0;
		predname->nfixedvalue = 0;
		if(predname->pred->flags & PREDF_FIXED_FLAG) {
			if(!ensure_fixed_values(&es, prg, predname)) {
				success = 0;
			}
		}
	}
	free_evalstate(&es);

	prg->totallines = lexer.totallines;

	return success;
}

int frontend_inject_query(struct program *prg, struct predname *predname, struct predname *tailpred, struct word *prompt, const uint8_t *str) {
	struct lexer lexer = {0};
	struct clause *cl;
	struct astnode *body, *an;
	struct word *vname = find_word(prg, "*Input");
	struct predicate *pred;

	pred_clear(predname);
	pred = predname->pred;

	arena_init(&lexer.temp_arena, 4096);
 	lexer.program = prg;
	lexer.string = str;

	cl = mkclause(pred);
	cl->line = 0;
	assert(predname->arity == 1);
	cl->params[0] = mkast(AN_VARIABLE, 0, cl->arena, 0);
	cl->params[0]->word = vname;

	body = parse_injected_query(&lexer, predname->pred);
	if(!body) {
		arena_free(&lexer.temp_arena);
		return 0;
	}

	find_dict_words(prg, body, 0);

	prg->errorflag = 0;
	body = expand_macros(body, prg, cl);
	if(prg->errorflag) {
		arena_free(&lexer.temp_arena);
		return 0;
	}

	cl->body = an = mkast(AN_EXHAUST, 1, cl->arena, 0);
	an->children[0] = body;

	an = an->next_in_body = mkast(AN_RULE, 0, cl->arena, 0);
	an->predicate = find_builtin(prg, BI_LINE);

	if(prompt) {
		an = an->next_in_body = mkast(AN_BAREWORD, 0, cl->arena, 0);
		an->word = prompt;
	}

	an = an->next_in_body = mkast(AN_RULE, 1, cl->arena, 0);
	an->predicate = tailpred;
	an->children[0] = mkast(AN_VARIABLE, 0, cl->arena, 0);
	an->children[0]->word = vname;

	add_clause(cl, pred);
	analyse_clause(prg, cl, 0);

	arena_free(&lexer.temp_arena);

	//pred->unbound_in = 1;
	trace_reconsider_pred(pred, prg);
	//pp_predicate(predname, prg);

	prg->errorflag = 0;
	comp_predicate(prg, predname);
	comp_cleanup();

	return !prg->errorflag;
}
