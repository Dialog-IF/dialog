#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "arena.h"
#include "ast.h"
#include "compile.h"
#include "eval.h"
#include "report.h"

static struct cinstr *instrbuf;
static int ninstr;
static int nalloc_instr;
static int instr_routine_id = -1;

static int ntemp;

static struct comp_routine *routines;
static int nroutine;
static int nalloc_routine;

#define NO_TAIL 0xffff
#define CONT_TAIL 0xfffe

struct opinfo {
	uint8_t		flags;
	char		*name;
} opinfo[N_OPCODES];

#define OPF_SUBOP		1
#define OPF_ENDS_ROUTINE	2

struct opinfosrc {
	uint8_t		op;
	uint8_t		flags;
	char		*name;
} opinfosrc[N_OPCODES] = {
	{I_ALLOCATE,		0,				"ALLOCATE"},
	{I_ASSIGN,		0,				"ASSIGN"},
	{I_BEGIN_STATUS,	0,				"BEGIN_STATUS"},
	{I_BREAKPOINT,		OPF_ENDS_ROUTINE,		"BREAKPOINT"},
	{I_BUILTIN,		0,				"BUILTIN"},
	{I_CHECK_INDEX,		0,				"CHECK_INDEX"},
	{I_CLRALL_OFLAG,	0,				"CLRALL_OFLAG"},
	{I_CLRALL_OVAR,		0,				"CLRALL_OVAR"},
	{I_COLLECT_BEGIN,	0,				"COLLECT_BEGIN"},
	{I_COLLECT_END,		0,				"COLLECT_END"},
	{I_COLLECT_PUSH,	0,				"COLLECT_PUSH"},
	{I_COMPUTE_R,		OPF_SUBOP,			"COMPUTE_R"},
	{I_COMPUTE_V,		OPF_SUBOP,			"COMPUTE_V"},
	{I_CUT_CHOICE,		0,				"CUT_CHOICE"},
	{I_DEALLOCATE,		0,				"DEALLOCATE"},
	{I_END_STATUS,		0,				"END_STATUS"},
	{I_FIRST_CHILD,		0,				"FIRST_CHILD"},
	{I_FIRST_OFLAG,		0,				"FIRST_OFLAG"},
	{I_FOR_WORDS,		OPF_SUBOP,			"FOR_WORDS"},
	{I_GET_GFLAG,		0,				"GET_GFLAG"},
	{I_GET_GVAR_R,		0,				"GET_GVAR_R"},
	{I_GET_GVAR_V,		0,				"GET_GVAR_V"},
	{I_GET_INPUT,		OPF_ENDS_ROUTINE,		"GET_INPUT"},
	{I_GET_KEY,		OPF_ENDS_ROUTINE,		"GET_KEY"},
	{I_GET_OFLAG,		0,				"GET_OFLAG"},
	{I_GET_OVAR_R,		0,				"GET_OVAR_R"},
	{I_GET_OVAR_V,		0,				"GET_OVAR_V"},
	{I_GET_PAIR_RR,		0,				"GET_PAIR_RR"},
	{I_GET_PAIR_RV,		0,				"GET_PAIR_RV"},
	{I_GET_PAIR_VR,		0,				"GET_PAIR_VR"},
	{I_GET_PAIR_VV,		0,				"GET_PAIR_VV"},
	{I_GET_RAW_INPUT,	OPF_ENDS_ROUTINE,		"GET_RAW_INPUT"},
	{I_IF_BOUND,		0,				"IF_BOUND"},
	{I_IF_MATCH,		0,				"IF_MATCH"},
	{I_IF_NIL,		0,				"IF_NIL"},
	{I_INVOKE_MULTI,	OPF_ENDS_ROUTINE,		"INVOKE_MULTI"},
	{I_INVOKE_ONCE,		OPF_ENDS_ROUTINE,		"INVOKE_ONCE"},
	{I_INVOKE_TAIL_MULTI,	OPF_ENDS_ROUTINE,		"INVOKE_TAIL_MULTI"},
	{I_INVOKE_TAIL_ONCE,	OPF_ENDS_ROUTINE,		"INVOKE_TAIL_ONCE"},
	{I_JUMP,		OPF_ENDS_ROUTINE,		"JUMP"},
	{I_LESSTHAN,		0,				"LESSTHAN"},
	{I_MAKE_PAIR_RR,	0,				"MAKE_PAIR_RR"},
	{I_MAKE_PAIR_RV,	0,				"MAKE_PAIR_RV"},
	{I_MAKE_PAIR_VR,	0,				"MAKE_PAIR_VR"},
	{I_MAKE_PAIR_VV,	0,				"MAKE_PAIR_VV"},
	{I_MAKE_VAR,		0,				"MAKE_VAR"},
	{I_NEXT_CHILD_PUSH,	0,				"NEXT_CHILD_PUSH"},
	{I_NEXT_OBJ_PUSH,	0,				"NEXT_OBJ_PUSH"},
	{I_NEXT_OFLAG_PUSH,	0,				"NEXT_OFLAG_PUSH"},
	{I_POP_CHOICE,		0,				"POP_CHOICE"},
	{I_POP_STOP,		0,				"POP_STOP"},
	{I_PREPARE_INDEX,	0,				"PREPARE_INDEX"},
	{I_PRINT_VAL,		0,				"PRINT_VAL"},
	{I_PRINT_WORDS,		0,				"PRINT_WORDS"},
	{I_PROCEED,		OPF_ENDS_ROUTINE,		"PROCEED"},
	{I_PUSH_CHOICE,		0,				"PUSH_CHOICE"},
	{I_PUSH_STOP,		0,				"PUSH_STOP"},
	{I_QUIT,		OPF_ENDS_ROUTINE,		"QUIT"},
	{I_RESTART,		OPF_ENDS_ROUTINE,		"RESTART"},
	{I_RESTORE,		0,				"RESTORE"},
	{I_RESTORE_CHOICE,	0,				"RESTORE_CHOICE"},
	{I_SAVE_CHOICE,		0,				"SAVE_CHOICE"},
	{I_SAVE,		OPF_ENDS_ROUTINE,		"SAVE"},
	{I_SAVE_UNDO,		OPF_ENDS_ROUTINE,		"SAVE_UNDO"},
	{I_SELECT,		OPF_ENDS_ROUTINE|OPF_SUBOP,	"SELECT"},
	{I_SET_CONT,		0,				"SET_CONT"},
	{I_SET_GFLAG,		OPF_SUBOP,			"SET_GFLAG"},
	{I_SET_GVAR,		0,				"SET_GVAR"},
	{I_SET_OFLAG,		OPF_SUBOP,			"SET_OFLAG"},
	{I_SET_OVAR,		0,				"SET_OVAR"},
	{I_SPLIT_LIST,		0,				"SPLIT_LIST"},
	{I_STOP,		OPF_ENDS_ROUTINE,		"STOP"},
	{I_TRACEPOINT,		OPF_SUBOP,			"TRACEPOINT"},
	{I_UNDO,		0,				"UNDO"},
	{I_UNIFY,		0,				"UNIFY"},
	{I_WIN_WIDTH,		0,				"WIN_WIDTH"},
	{I_WORDCHECK_BEGIN,	0,				"WORDCHECK_BEGIN"},
	{I_WORDCHECK_END,	0,				"WORDCHECK_END"},
};

static int make_routine_block(int n) {
	int r_id = nroutine;

	if(nroutine + n > nalloc_routine) {
		nalloc_routine = (nroutine + n) * 2 + 8;
		routines = realloc(routines, nalloc_routine * sizeof(struct comp_routine));
		memset(routines + nroutine, 0, (nalloc_routine - nroutine) * sizeof(struct comp_routine));
	}

	nroutine += n;
	return r_id;
}

static int make_routine_id() {
	return make_routine_block(1);
}

static void begin_routine(int r_id) {
	assert(!ninstr);
	instr_routine_id = r_id;
}

static struct cinstr *add_instr(uint8_t op) {
	if(ninstr >= nalloc_instr) {
		nalloc_instr = ninstr * 2 + 8;
		instrbuf = realloc(instrbuf, nalloc_instr * sizeof(struct cinstr));
	}

	assert(!ninstr || !(opinfo[instrbuf[ninstr - 1].op].flags & OPF_ENDS_ROUTINE));

	instrbuf[ninstr].op = op;
	instrbuf[ninstr].subop = 0;
	memset(instrbuf[ninstr].oper, 0, sizeof(instrbuf[ninstr].oper));
	return &instrbuf[ninstr++];
}

static void end_routine(uint16_t clause_id, struct arena *arena) {
	int r_id = instr_routine_id;

	assert(r_id >= 0);
	assert(r_id < nroutine);

	assert(ninstr);
	assert(opinfo[instrbuf[ninstr - 1].op].flags & OPF_ENDS_ROUTINE);

	routines[r_id].instr = arena_alloc(arena, ninstr * sizeof(struct cinstr));
	memcpy(routines[r_id].instr, instrbuf, ninstr * sizeof(struct cinstr));
	routines[r_id].ninstr = ninstr;

	routines[r_id].clause_id = clause_id;

	ninstr = 0;
	instr_routine_id = -1;
}

static void end_routine_cl(struct clause *cl) {
	end_routine(cl->clause_id, &cl->predicate->pred->arena);
}

static int findvar(struct clause *cl, struct word *w) {
	int i;

	for(i = 0; i < cl->nvar; i++) {
		if(cl->varnames[i] == w) return i;
	}

	assert(0);
	return -1;
}

static void comp_ensure_seen(struct clause *cl, struct astnode *an, uint8_t *seen) {
	int i, vnum;
	struct cinstr *ci;

	while(an) {
		if(an->kind == AN_VARIABLE) {
			if(an->word->name[0]) {
				vnum = findvar(cl, an->word);
				if(!seen[vnum]) {
					ci = add_instr(I_MAKE_VAR);
					ci->oper[0] = (value_t) {OPER_VAR, vnum};
					seen[vnum] = 1;
				}
			}
		} else {
			for(i = 0; i < an->nchild; i++) {
				comp_ensure_seen(cl, an->children[i], seen);
			}
		}
		an = an->next_in_body;
	}
}

static int comp_simple_constant_list(struct astnode *an) {
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

static value_t comp_tag_simple(struct astnode *an) {
	switch(an->kind) {
	case AN_DICTWORD:
		return (value_t) {VAL_DICT, an->word->dict_id};
	case AN_TAG:
		return (value_t) {VAL_OBJ, an->word->obj_id};
	case AN_INTEGER:
		return (value_t) {VAL_NUM, an->value};
	case AN_EMPTY_LIST:
		return (value_t) {VAL_NIL, 0};
	default:
		pp_expr(an);
		printf("\n");
		assert(0); exit(1);
	}
}

static value_t comp_value(struct clause *cl, struct astnode *an, uint8_t *seen) {
	struct cinstr *ci;
	int vnum, i, is_ref[2];
	value_t sub[2];

	if(an->kind == AN_VARIABLE) {
		if(an->word->name[0]) {
			vnum = findvar(cl, an->word);
			if(!seen[vnum]) {
				ci = add_instr(I_MAKE_VAR);
				ci->oper[0] = (value_t) {OPER_VAR, vnum};
				seen[vnum] = 1;
			}
			return (value_t) {OPER_VAR, vnum};
		} else {
			ci = add_instr(I_MAKE_VAR);
			ci->oper[0] = (value_t) {OPER_TEMP, ntemp++};
			return ci->oper[0];
		}
	} else if(an->kind == AN_PAIR) {
		for(i = 0; i < 2; i++) {
			if(an->children[i]->kind == AN_VARIABLE) {
				if(an->children[i]->word->name[0]) {
					vnum = findvar(cl, an->children[i]->word);
					sub[i] = (value_t) {OPER_VAR, vnum};
					is_ref[i] = !seen[vnum];
					seen[vnum] = 1;
				} else {
					sub[i] = (value_t) {OPER_TEMP, ntemp++};
					is_ref[i] = 1;
				}
			} else {
				sub[i] = comp_value(cl, an->children[i], seen);
				is_ref[i] = 0;
			}
		}
		ci = add_instr(I_MAKE_PAIR_VV + 2 * is_ref[0] + is_ref[1]);
		ci->oper[0] = (value_t) {OPER_TEMP, ntemp++};
		ci->oper[1] = sub[0];
		ci->oper[2] = sub[1];
		return ci->oper[0];
	} else {
		return comp_tag_simple(an);
	}
}

static void comp_value_into(struct clause *cl, struct astnode *an, value_t dest, uint8_t *seen) {
	struct cinstr *ci;
	int vnum, i, is_ref[2];
	value_t sub[2];

	if(an->kind == AN_VARIABLE) {
		if(an->word->name[0]) {
			vnum = findvar(cl, an->word);
			if(!seen[vnum]) {
				ci = add_instr(I_MAKE_VAR);
				ci->oper[0] = dest;
				ci = add_instr(I_ASSIGN);
				ci->oper[0] = (value_t) {OPER_VAR, vnum};
				ci->oper[1] = dest;
				seen[vnum] = 1;
			} else {
				ci = add_instr(I_ASSIGN);
				ci->oper[0] = dest;
				ci->oper[1] = (value_t) {OPER_VAR, vnum};
			}
		} else {
			ci = add_instr(I_MAKE_VAR);
			ci->oper[0] = dest;
		}
	} else if(an->kind == AN_PAIR) {
		for(i = 0; i < 2; i++) {
			if(an->children[i]->kind == AN_VARIABLE) {
				if(an->children[i]->word->name[0]) {
					vnum = findvar(cl, an->children[i]->word);
					sub[i] = (value_t) {OPER_VAR, vnum};
					is_ref[i] = !seen[vnum];
					seen[vnum] = 1;
				} else {
					sub[i] = (value_t) {OPER_TEMP, ntemp++};
					is_ref[i] = 1;
				}
			} else {
				sub[i] = comp_value(cl, an->children[i], seen);
				is_ref[i] = 0;
			}
		}
		ci = add_instr(I_MAKE_PAIR_VV + 2 * is_ref[0] + is_ref[1]);
		ci->oper[0] = dest;
		ci->oper[1] = sub[0];
		ci->oper[2] = sub[1];
	} else {
		ci = add_instr(I_ASSIGN);
		ci->oper[0] = dest;
		ci->oper[1] = comp_tag_simple(an);
	}
}

static void comp_param(struct clause *cl, struct astnode *an, value_t dest, uint8_t *seen) {
	struct cinstr *ci;
	int vnum, i, is_ref[2];
	value_t sub[2];

	if(an->kind == AN_VARIABLE) {
		if(an->word->name[0]) {
			vnum = findvar(cl, an->word);
			if(seen[vnum]) {
				ci = add_instr(I_UNIFY);
				ci->oper[0] = dest;
				ci->oper[1] = (value_t) {OPER_VAR, vnum};
			} else {
				ci = add_instr(I_ASSIGN);
				ci->oper[0] = (value_t) {OPER_VAR, vnum};
				ci->oper[1] = dest;
				seen[vnum] = 1;
			}
		}
	} else if(an->kind == AN_PAIR) {
		for(i = 0; i < 2; i++) {
			if(an->children[i]->kind == AN_PAIR) {
				sub[i] = (value_t) {OPER_TEMP, ntemp++};
				is_ref[i] = 1;
			} else if(an->children[i]->kind == AN_VARIABLE) {
				if(an->children[i]->word->name[0]) {
					vnum = findvar(cl, an->children[i]->word);
					sub[i] = (value_t) {OPER_VAR, vnum};
					is_ref[i] = !seen[vnum];
					seen[vnum] = 1;
				} else {
					sub[i] = (value_t) {OPER_TEMP, ntemp++};
					is_ref[i] = 1;
				}
			} else {
				sub[i] = comp_tag_simple(an->children[i]);
				is_ref[i] = 0;
			}
		}
		ci = add_instr(I_GET_PAIR_VV + 2 * is_ref[0] + is_ref[1]);
		ci->oper[0] = dest;
		ci->oper[1] = sub[0];
		ci->oper[2] = sub[1];
		for(i = 0; i < 2; i++) {
			if(an->children[i]->kind == AN_PAIR) {
				comp_param(cl, an->children[i], sub[i], seen);
			}
		}
	} else {
		ci = add_instr(I_UNIFY);
		ci->oper[0] = dest;
		ci->oper[1] = comp_tag_simple(an);
	}
}

static void post_rule_trace(struct clause *cl, struct astnode *an, uint8_t *seen) {
#if 1
	struct cinstr *ci;

	if(an->predicate->builtin == BI_LINE
	|| an->predicate->builtin == BI_PAR
	|| an->predicate->builtin == BI_INJECTED_QUERY) {
		return;
	}

	ci = add_instr(I_TRACEPOINT);
	ci->subop = TR_QDONE;
	ci->oper[0] = (value_t) {OPER_FILE, FILENUMPART(an->line)};
	ci->oper[1] = (value_t) {OPER_NUM, LINEPART(an->line)};
	ci->oper[2] = (value_t) {OPER_PRED, an->predicate->pred_id};
#endif
}

static int comp_rule(struct program *prg, struct clause *cl, struct astnode *an, uint8_t *seen, int tail) {
	int i;
	struct cinstr *ci;
	int lab, labloop, labmatch;
	value_t v1, v2, v3;
	int t1, t2;
	int vnum, ch;
	struct astnode *sub;
	int do_trace = 1;

	// returns non-zero if we handled the tail case

	assert(an->kind == AN_RULE || an->kind == AN_NEG_RULE); // but we treat NEG as a normal rule by now

	if(cl->predicate->builtin == BI_INJECTED_QUERY && tail != NO_TAIL && !an->next_in_body) {
		// force a tail-call here, otherwise the trace indentation will keep increasing
		do_trace = 0;
	}

	if(do_trace) {
		for(i = 0; i < an->nchild; i++) {
			comp_value_into(cl, an->children[i], (value_t) {OPER_ARG, i}, seen);
		}

		if(an->predicate->builtin != BI_LINE
		&& an->predicate->builtin != BI_PAR) {
			ci = add_instr(I_TRACEPOINT);
			ci->subop = (an->subkind == RULE_MULTI)? TR_MQUERY : TR_QUERY;
			ci->oper[0] = (value_t) {OPER_FILE, FILENUMPART(an->line)};
			ci->oper[1] = (value_t) {OPER_NUM, LINEPART(an->line)};
			ci->oper[2] = (value_t) {OPER_PRED, an->predicate->pred_id};
		}
	}

	if(an->predicate->pred->flags & PREDF_FAIL) {
		ci = add_instr(I_JUMP);
		ci->oper[0] = (value_t) {OPER_FAIL};
		end_routine_cl(cl);
		if(tail == NO_TAIL) {
			// we have to put the subsequent dead code somewhere
			begin_routine(make_routine_id());
		}
		return 1;
	}

	if(an->predicate->builtin == BI_STOP) {
		ci = add_instr(I_STOP);
		end_routine_cl(cl);
		if(tail == NO_TAIL) {
			// we have to put the subsequent dead code somewhere
			begin_routine(make_routine_id());
		}
		return 1;
	}

	if(an->predicate->builtin == BI_QUIT) {
		ci = add_instr(I_QUIT);
		end_routine_cl(cl);
		if(tail == NO_TAIL) {
			// we have to put the subsequent dead code somewhere
			begin_routine(make_routine_id());
		}
		return 1;
	}

	if(an->predicate->builtin == BI_RESTART) {
		ci = add_instr(I_RESTART);
		end_routine_cl(cl);
		if(tail == NO_TAIL) {
			// we have to put the subsequent dead code somewhere
			begin_routine(make_routine_id());
		}
		return 1;
	}

	if(an->predicate->builtin == BI_UNBOUND) {
		if(do_trace) {
			v1 = (value_t) {OPER_ARG, 0};
		} else {
			v1 = comp_value(cl, an->children[0], seen);
		}
		ci = add_instr(I_IF_BOUND);
		ci->oper[0] = v1;
		ci->oper[1] = (value_t) {OPER_FAIL};
		post_rule_trace(cl, an, seen);
		return 0;
	}

	if(an->predicate->builtin == BI_UNIFY) {
		if(do_trace) {
			v1 = (value_t) {OPER_ARG, 0};
			v2 = (value_t) {OPER_ARG, 1};
		} else {
			v1 = comp_value(cl, an->children[0], seen);
			v2 = comp_value(cl, an->children[1], seen);
		}
		ci = add_instr(I_UNIFY);
		ci->oper[0] = v1;
		ci->oper[1] = v2;
		post_rule_trace(cl, an, seen);
		return 0;
	}

	if(an->predicate->builtin == BI_LESSTHAN) {
		if(do_trace) {
			v1 = (value_t) {OPER_ARG, 0};
			v2 = (value_t) {OPER_ARG, 1};
		} else {
			v1 = comp_value(cl, an->children[0], seen);
			v2 = comp_value(cl, an->children[1], seen);
		}
		ci = add_instr(I_LESSTHAN);
		ci->oper[0] = v1;
		ci->oper[1] = v2;
		post_rule_trace(cl, an, seen);
		return 0;
	}

	if(an->predicate->builtin == BI_GREATERTHAN) {
		if(do_trace) {
			v1 = (value_t) {OPER_ARG, 0};
			v2 = (value_t) {OPER_ARG, 1};
		} else {
			v1 = comp_value(cl, an->children[0], seen);
			v2 = comp_value(cl, an->children[1], seen);
		}
		ci = add_instr(I_LESSTHAN);
		ci->oper[0] = v2;
		ci->oper[1] = v1;
		post_rule_trace(cl, an, seen);
		return 0;
	}

	if(an->predicate->builtin == BI_PLUS
	|| an->predicate->builtin == BI_MINUS
	|| an->predicate->builtin == BI_TIMES
	|| an->predicate->builtin == BI_DIVIDED
	|| an->predicate->builtin == BI_MODULO
	|| an->predicate->builtin == BI_RANDOM) {
		if(do_trace) {
			ci = add_instr(I_COMPUTE_V);
			ci->oper[0] = (value_t) {OPER_ARG, 0};
			ci->oper[1] = (value_t) {OPER_ARG, 1};
			ci->oper[2] = (value_t) {OPER_ARG, 2};
		} else {
			v1 = comp_value(cl, an->children[0], seen);
			v2 = comp_value(cl, an->children[1], seen);
			if(an->children[2]->kind == AN_VARIABLE) {
				if(!an->children[2]->word->name[0]) {
					ci = add_instr(I_COMPUTE_R);
				} else if(seen[(vnum = findvar(cl, an->children[2]->word))]) {
					ci = add_instr(I_COMPUTE_V);
					ci->oper[2] = (value_t) {OPER_VAR, vnum};
				} else {
					ci = add_instr(I_COMPUTE_R);
					ci->oper[2] = (value_t) {OPER_VAR, vnum};
					seen[vnum] = 1;
				}
			} else {
				v3 = comp_value(cl, an->children[2], seen);
				ci = add_instr(I_COMPUTE_V);
				ci->oper[2] = v3;
			}
			ci->oper[0] = v1;
			ci->oper[1] = v2;
		}
		ci->subop = an->predicate->builtin;
		post_rule_trace(cl, an, seen);
		return 0;
	}

	if(an->predicate->builtin == BI_OBJECT
	&& !an->children[0]->unbound) {
		if(do_trace) {
			v1 = (value_t) {OPER_ARG, 0};
		} else {
			v1 = comp_value(cl, an->children[0], seen);
		}
		ci = add_instr(I_BUILTIN);
		ci->oper[0] = v1;
		ci->oper[2] = (value_t) {OPER_PRED, an->predicate->pred_id};
		post_rule_trace(cl, an, seen);
		return 0;
	}

	if(an->predicate->builtin == BI_IS_ONE_OF
	&& an->subkind == RULE_SIMPLE
	&& !an->children[0]->unbound
	&& comp_simple_constant_list(an->children[1])) {
		lab = make_routine_id();
		if(do_trace) {
			v1 = (value_t) {OPER_ARG, 0};
		} else {
			v1 = comp_value(cl, an->children[0], seen);
		}
		ci = add_instr(I_PREPARE_INDEX);
		ci->oper[0] = v1;
		for(sub = an->children[1]; sub->kind != AN_EMPTY_LIST; sub = sub->children[1]) {
			assert(sub->kind == AN_PAIR);
			ci = add_instr(I_CHECK_INDEX);
			ci->oper[0] = comp_tag_simple(sub->children[0]);
			ci->oper[1] = (value_t) {OPER_RLAB, lab};
		}
		ci = add_instr(I_JUMP);
		ci->oper[0] = (value_t) {OPER_FAIL};
		end_routine_cl(cl);
		begin_routine(lab);
		post_rule_trace(cl, an, seen);
		return 0;
	}

	if(an->predicate->builtin == BI_SPLIT
	&& !an->children[0]->unbound
	&& comp_simple_constant_list(an->children[1])) {
		// a0 = input list
		// a1 = repurposed as list iterator
		// a2 = left output
		// a3 = right output
		if(!do_trace) {
			comp_value_into(cl, an->children[0], (value_t) {OPER_ARG, 0}, seen);
			if(an->children[2]->kind != AN_VARIABLE
			|| an->children[2]->word->name[0]) {
				comp_value_into(cl, an->children[2], (value_t) {OPER_ARG, 2}, seen);
			}
			comp_value_into(cl, an->children[3], (value_t) {OPER_ARG, 3}, seen);
		}
		t1 = ntemp++;
		t2 = ntemp++;
		labloop = make_routine_id();
		labmatch = make_routine_id();
		lab = make_routine_id();
		ci = add_instr(I_ASSIGN);
		ci->oper[0] = (value_t) {OPER_ARG, 1};
		ci->oper[1] = (value_t) {OPER_ARG, 0};
		ci = add_instr(I_JUMP);
		ci->oper[0] = (value_t) {OPER_RLAB, labloop};
		end_routine_cl(cl);

		begin_routine(labloop);
		ci = add_instr(I_ASSIGN);
		ci->oper[0] = (value_t) {OPER_TEMP, t2};
		ci->oper[1] = (value_t) {OPER_ARG, 1};
		ci = add_instr(I_GET_PAIR_RR);
		ci->oper[0] = (value_t) {OPER_TEMP, t2};
		ci->oper[1] = (value_t) {OPER_TEMP, t1};
		ci->oper[2] = (value_t) {OPER_ARG, 1};
		ci = add_instr(I_PREPARE_INDEX);
		ci->oper[0] = (value_t) {OPER_TEMP, t1};
		for(sub = an->children[1]; sub->kind != AN_EMPTY_LIST; sub = sub->children[1]) {
			assert(sub->kind == AN_PAIR);
			ci = add_instr(I_CHECK_INDEX);
			ci->oper[0] = comp_tag_simple(sub->children[0]);
			ci->oper[1] = (value_t) {OPER_RLAB, labmatch};
		}
		ci = add_instr(I_JUMP);
		ci->oper[0] = (value_t) {OPER_RLAB, labloop};
		end_routine_cl(cl);

		begin_routine(lab);
		ci = add_instr(I_POP_CHOICE);
		ci->oper[0] = (value_t) {OPER_NUM, 4};
		ci = add_instr(I_JUMP);
		ci->oper[0] = (value_t) {OPER_RLAB, labloop};
		end_routine_cl(cl);

		begin_routine(labmatch);
		ci = add_instr(I_PUSH_CHOICE);
		ci->oper[0] = (value_t) {OPER_NUM, 4};
		ci->oper[1] = (value_t) {OPER_RLAB, lab};
		ci = add_instr(I_UNIFY);
		ci->oper[0] = (value_t) {OPER_ARG, 1};
		ci->oper[1] = (value_t) {OPER_ARG, 3};
		if(do_trace
		|| an->children[2]->kind != AN_VARIABLE
		|| an->children[2]->word->name[0]) {
			ci = add_instr(I_SPLIT_LIST);
			ci->oper[0] = (value_t) {OPER_ARG, 0};
			ci->oper[1] = (value_t) {OPER_TEMP, t2};
			ci->oper[2] = (value_t) {OPER_ARG, 2};
		}
		if(an->subkind == RULE_SIMPLE) {
			ci = add_instr(I_CUT_CHOICE);
		}
		post_rule_trace(cl, an, seen);
		return 0;
	}

	if(an->predicate->builtin == BI_SCRIPT_ON
	|| an->predicate->builtin == BI_SCRIPT_OFF
	|| an->predicate->builtin == BI_TRACE_ON
	|| an->predicate->builtin == BI_TRACE_OFF
	|| an->predicate->builtin == BI_NOSPACE
	|| an->predicate->builtin == BI_SPACE
	|| an->predicate->builtin == BI_LINE
	|| an->predicate->builtin == BI_PAR
	|| an->predicate->builtin == BI_ROMAN
	|| an->predicate->builtin == BI_BOLD
	|| an->predicate->builtin == BI_ITALIC
	|| an->predicate->builtin == BI_REVERSE
	|| an->predicate->builtin == BI_FIXED
	|| an->predicate->builtin == BI_UPPER
	|| an->predicate->builtin == BI_CLEAR
	|| an->predicate->builtin == BI_CLEAR_ALL
	|| an->predicate->builtin == BI_SERIALNUMBER
	|| an->predicate->builtin == BI_COMPILERVERSION
	|| an->predicate->builtin == BI_MEMSTATS
	|| an->predicate->builtin == BI_HAVE_UNDO) {
		ci = add_instr(I_BUILTIN);
		ci->oper[2] = (value_t) {OPER_PRED, an->predicate->pred_id};
		post_rule_trace(cl, an, seen);
		return 0;
	}

	if(an->predicate->builtin == BI_NUMBER
	|| an->predicate->builtin == BI_LIST
	|| an->predicate->builtin == BI_EMPTY
	|| an->predicate->builtin == BI_NONEMPTY
	|| an->predicate->builtin == BI_WORD
	|| an->predicate->builtin == BI_SPACE_N
	|| an->predicate->builtin == BI_PAR_N) {
		if(do_trace) {
			v1 = (value_t) {OPER_ARG, 0};
		} else {
			v1 = comp_value(cl, an->children[0], seen);
		}
		ci = add_instr(I_BUILTIN);
		ci->oper[0] = v1;
		ci->oper[2] = (value_t) {OPER_PRED, an->predicate->pred_id};
		post_rule_trace(cl, an, seen);
		return 0;
	}

	if(an->predicate->builtin == BI_WINDOWWIDTH) {
		if(do_trace) {
			v1 = (value_t) {OPER_ARG, 0};
		} else {
			v1 = comp_value(cl, an->children[0], seen);
		}
		ci = add_instr(I_WIN_WIDTH);
		ci->oper[0] = v1;
		post_rule_trace(cl, an, seen);
		return 0;
	}

	if(an->predicate->builtin == BI_CURSORTO) {
		if(do_trace) {
			v1 = (value_t) {OPER_ARG, 0};
			v2 = (value_t) {OPER_ARG, 1};
		} else {
			v1 = comp_value(cl, an->children[0], seen);
			v2 = comp_value(cl, an->children[1], seen);
		}
		ci = add_instr(I_BUILTIN);
		ci->oper[0] = v1;
		ci->oper[1] = v2;
		ci->oper[2] = (value_t) {OPER_PRED, an->predicate->pred_id};
		post_rule_trace(cl, an, seen);
		return 0;
	}

	if(an->predicate->builtin == BI_WORDREP_RETURN
	|| an->predicate->builtin == BI_WORDREP_SPACE
	|| an->predicate->builtin == BI_WORDREP_BACKSPACE
	|| an->predicate->builtin == BI_WORDREP_UP
	|| an->predicate->builtin == BI_WORDREP_DOWN
	|| an->predicate->builtin == BI_WORDREP_LEFT
	|| an->predicate->builtin == BI_WORDREP_RIGHT) {
		ch =
			(an->predicate->builtin == BI_WORDREP_RETURN)? 13 :
			(an->predicate->builtin == BI_WORDREP_SPACE)? 32 :
			(an->predicate->builtin == BI_WORDREP_BACKSPACE)? 8 :
			(an->predicate->builtin == BI_WORDREP_UP)? 129 :
			(an->predicate->builtin == BI_WORDREP_DOWN)? 130 :
			(an->predicate->builtin == BI_WORDREP_LEFT)? 131 :
			(an->predicate->builtin == BI_WORDREP_RIGHT)? 132 :
			0;
		if(!do_trace && an->children[0]->kind == AN_VARIABLE) {
			if(an->children[0]->word->name[0]) {
				if(seen[(vnum = findvar(cl, an->children[0]->word))]) {
					ci = add_instr(I_UNIFY);
				} else {
					ci = add_instr(I_ASSIGN);
					seen[vnum] = 1;
				}
				ci->oper[0] = (value_t) {OPER_VAR, vnum};
				ci->oper[1] = (value_t) {VAL_DICT, ch};
			}
		} else {
			if(do_trace) {
				v1 = (value_t) {OPER_ARG, 0};
			} else {
				v1 = comp_value(cl, an->children[0], seen);
			}
			ci = add_instr(I_UNIFY);
			ci->oper[0] = v1;
			ci->oper[1] = (value_t) {VAL_DICT, ch};
		}
		post_rule_trace(cl, an, seen);
		return 0;
	}

	if(an->predicate->pred->flags & PREDF_DYNAMIC) {
		if(an->predicate->arity == 0) {
			assert(an->predicate->dyn_id != DYN_NONE);
			ci = add_instr(I_GET_GFLAG);
			ci->oper[0] = (value_t) {OPER_GFLAG, an->predicate->dyn_id};
			post_rule_trace(cl, an, seen);
			return 0;
		} else if(an->predicate->arity == 1) {
			if(an->predicate->pred->flags & PREDF_GLOBAL_VAR) {
				assert(an->predicate->dyn_var_id != DYN_NONE);
				if(!do_trace
				&& an->children[0]->kind == AN_VARIABLE
				&& an->children[0]->word->name[0]
				&& !seen[(vnum = findvar(cl, an->children[0]->word))]) {
					ci = add_instr(I_GET_GVAR_R);
					ci->oper[0] = (value_t) {OPER_GVAR, an->predicate->dyn_var_id};
					ci->oper[1] = (value_t) {OPER_VAR, vnum};
					seen[vnum] = 1;
				} else {
					if(do_trace) {
						v1 = (value_t) {OPER_ARG, 0};
					} else {
						v1 = comp_value(cl, an->children[0], seen);
					}
					ci = add_instr(I_GET_GVAR_V);
					ci->oper[0] = (value_t) {OPER_GVAR, an->predicate->dyn_var_id};
					ci->oper[1] = v1;
				}
				post_rule_trace(cl, an, seen);
				return 0;
			} else if(!an->children[0]->unbound) {
				assert(an->predicate->dyn_id != DYN_NONE);
				if(do_trace) {
					v1 = (value_t) {OPER_ARG, 0};
				} else {
					v1 = comp_value(cl, an->children[0], seen);
				}
				ci = add_instr(I_GET_OFLAG);
				ci->oper[0] = (value_t) {OPER_OFLAG, an->predicate->dyn_id};
				ci->oper[1] = v1;
				post_rule_trace(cl, an, seen);
				return 0;
			} // else compile to a regular query
		} else {
			assert(an->predicate->arity == 2);
			assert(an->predicate->dyn_id != DYN_NONE);
			if(!an->children[0]->unbound) {
				v1 = comp_value(cl, an->children[0], seen);
				if(an->children[1]->kind == AN_VARIABLE
				&& an->children[1]->word->name[0]
				&& !seen[(vnum = findvar(cl, an->children[1]->word))]) {
					ci = add_instr(I_GET_OVAR_R);
					ci->oper[0] = (value_t) {OPER_OVAR, an->predicate->dyn_id};
					ci->oper[1] = v1;
					ci->oper[2] = (value_t) {OPER_VAR, vnum};
					seen[vnum] = 1;
				} else {
					if(do_trace) {
						v2 = (value_t) {OPER_ARG, 1};
					} else {
						v2 = comp_value(cl, an->children[1], seen);
					}
					ci = add_instr(I_GET_OVAR_V);
					ci->oper[0] = (value_t) {OPER_OVAR, an->predicate->dyn_id};
					ci->oper[1] = v1;
					ci->oper[2] = v2;
				}
				post_rule_trace(cl, an, seen);
				return 0;
			} // else compile to a regular query
		}
	}

	if(!do_trace) {
		for(i = 0; i < an->nchild; i++) {
			comp_value_into(cl, an->children[i], (value_t) {OPER_ARG, i}, seen);
		}
	}

	if(!do_trace && tail == CONT_TAIL) {
		ci = add_instr(I_DEALLOCATE);
		ci = add_instr(I_INVOKE_TAIL_ONCE + (an->subkind == RULE_MULTI));
		ci->oper[0] = (value_t) {OPER_PRED, an->predicate->pred_id};
		end_routine_cl(cl);
		return 1;
	} else {
		lab = make_routine_id();
		ci = add_instr(I_SET_CONT);
		ci->oper[0] = (value_t) {OPER_RLAB, lab};
		ci = add_instr(I_INVOKE_ONCE + (an->subkind == RULE_MULTI));
		ci->oper[0] = (value_t) {OPER_PRED, an->predicate->pred_id};
		end_routine_cl(cl);
		begin_routine(lab);
		post_rule_trace(cl, an, seen);
		return 0;
	}
}

static void comp_now(struct program *prg, struct clause *cl, struct astnode *an, uint8_t *seen) {
	value_t v1, v2;
	struct cinstr *ci;

	ci = add_instr(I_TRACEPOINT);
	ci->subop = TR_LINE;
	ci->oper[0] = (value_t) {OPER_FILE, FILENUMPART(an->line)};
	ci->oper[1] = (value_t) {OPER_NUM, LINEPART(an->line)};
	if(an->kind == AN_RULE) {
		if(!(an->predicate->pred->flags & PREDF_DYNAMIC)) {
			report(LVL_ERR, an->line, "Cannot modify non-dynamic predicate.");
			prg->errorflag = 1;
		} else if(an->predicate->arity == 0) {
			assert(an->predicate->dyn_id != DYN_NONE);
			ci = add_instr(I_SET_GFLAG);
			ci->subop = 1;
			ci->oper[0] = (value_t) {OPER_GFLAG, an->predicate->dyn_id};
		} else if(an->predicate->arity == 1) {
			if(an->predicate->pred->flags & PREDF_GLOBAL_VAR) {
				assert(an->predicate->dyn_var_id != DYN_NONE);
				v1 = comp_value(cl, an->children[0], seen);
				ci = add_instr(I_SET_GVAR);
				ci->oper[0] = (value_t) {OPER_GVAR, an->predicate->dyn_var_id};
				ci->oper[1] = v1;
			} else {
				assert(an->predicate->dyn_id != DYN_NONE);
				v1 = comp_value(cl, an->children[0], seen);
				ci = add_instr(I_SET_OFLAG);
				ci->subop = 1;
				ci->oper[0] = (value_t) {OPER_OFLAG, an->predicate->dyn_id};
				ci->oper[1] = v1;
			}
		} else {
			assert(an->predicate->arity == 2);
			assert(an->predicate->dyn_id != DYN_NONE);
			v1 = comp_value(cl, an->children[0], seen);
			v2 = comp_value(cl, an->children[1], seen);
			ci = add_instr(I_SET_OVAR);
			ci->oper[0] = (value_t) {OPER_OVAR, an->predicate->dyn_id};
			ci->oper[1] = v1;
			ci->oper[2] = v2;
		}
	} else if(an->kind == AN_NEG_RULE) {
		if(!(an->predicate->pred->flags & PREDF_DYNAMIC)) {
			report(LVL_ERR, an->line, "Cannot modify non-dynamic predicate.");
			prg->errorflag = 1;
		} else if(an->predicate->arity == 0) {
			assert(an->predicate->dyn_id != DYN_NONE);
			ci = add_instr(I_SET_GFLAG);
			ci->subop = 0;
			ci->oper[0] = (value_t) {OPER_GFLAG, an->predicate->dyn_id};
		} else if(an->predicate->arity == 1) {
			if(an->predicate->pred->flags & PREDF_GLOBAL_VAR) {
				assert(an->predicate->dyn_var_id != DYN_NONE);
				if(an->children[0]->kind == AN_VARIABLE
				&& !an->children[0]->word->name[0]) {
					ci = add_instr(I_SET_GVAR);
					ci->oper[0] = (value_t) {OPER_GVAR, an->predicate->dyn_var_id};
				} else {
					report(LVL_ERR, an->line, "When resetting a global variable, the argument must be anonymous ($).");
					prg->errorflag = 1;
				}
			} else {
				assert(an->predicate->dyn_id != DYN_NONE);
				if(an->children[0]->kind == AN_VARIABLE
				&& !an->children[0]->word->name[0]) {
					ci = add_instr(I_CLRALL_OFLAG);
					ci->oper[0] = (value_t) {OPER_OFLAG, an->predicate->dyn_id};
				} else {
					v1 = comp_value(cl, an->children[0], seen);
					ci = add_instr(I_SET_OFLAG);
					ci->subop = 0;
					ci->oper[0] = (value_t) {OPER_OFLAG, an->predicate->dyn_id};
					ci->oper[1] = v1;
				}
			}
		} else {
			assert(an->predicate->arity == 2);
			assert(an->predicate->dyn_id != DYN_NONE);
			if(an->children[1]->kind == AN_VARIABLE
			&& !an->children[1]->word->name[0]) {
				if(an->children[0]->kind == AN_VARIABLE
				&& !an->children[0]->word->name[0]) {
					ci = add_instr(I_CLRALL_OVAR);
					ci->oper[0] = (value_t) {OPER_OVAR, an->predicate->dyn_id};
				} else {
					v1 = comp_value(cl, an->children[0], seen);
					ci = add_instr(I_SET_OVAR);
					ci->oper[0] = (value_t) {OPER_OVAR, an->predicate->dyn_id};
					ci->oper[1] = v1;
				}
			} else {
				report(LVL_ERR, an->line, "When resetting a per-object variable, the second argument must be anonymous ($).");
				prg->errorflag = 1;
			}
		}
	} else if(an->kind == AN_BLOCK || an->kind == AN_FIRSTRESULT) {
		for(an = an->children[0]; an; an = an->next_in_body) {
			comp_now(prg, cl, an, seen);
		}
	} else {
		assert(0);
	}
}

static void comp_body(struct program *prg, struct clause *cl, struct astnode *an, uint8_t *seen, int tail) {
	value_t v1, v2;
	struct cinstr *ci;
	int i, lab = -1, endlab, vnum;
	int at_tail;

	while(an) {
		at_tail = (tail != NO_TAIL && !an->next_in_body);
		switch(an->kind) {
		case AN_RULE:
			if(comp_rule(prg, cl, an, seen, at_tail? tail : NO_TAIL)) {
				return;
			}
			break;
		case AN_BLOCK:
			comp_body(prg, cl, an->children[0], seen, at_tail? tail : NO_TAIL);
			if(at_tail) return;
			break;
		case AN_OR:
			comp_ensure_seen(cl, an, seen);
			if(at_tail) {
				endlab = tail;
			} else {
				endlab = make_routine_id();
			}
			for(i = 0; i < an->nchild; i++) {
				int last = (i == an->nchild - 1);
				if(!last) {
					lab = make_routine_id();
					ci = add_instr(I_PUSH_CHOICE);
					ci->oper[0] = (value_t) {OPER_NUM, 0};
					ci->oper[1] = (value_t) {OPER_RLAB, lab};
				}
				comp_body(prg, cl, an->children[i], seen, endlab);
				if(!last) {
					begin_routine(lab);
					ci = add_instr(I_POP_CHOICE);
					ci->oper[0] = (value_t) {OPER_NUM, 0};
				}
			}
			if(at_tail) {
				return;
			} else {
				begin_routine(endlab);
			}
			break;
		case AN_IF:
			if(at_tail) {
				endlab = tail;
			} else {
				endlab = make_routine_id();
			}
			if(an->children[0]
			&& an->children[0]->kind == AN_RULE
			&& (an->children[0]->predicate->pred->flags & PREDF_FAIL)) {
				comp_body(prg, cl, an->children[2], seen, endlab);
			} else if(an->children[0]
			&& an->children[0]->kind == AN_RULE
			&& (an->children[0]->predicate->pred->flags & PREDF_SUCCEEDS)) {
				vnum = findvar(cl, an->word);
				ci = add_instr(I_SAVE_CHOICE);
				ci->oper[0] = (value_t) {OPER_VAR, vnum};
				comp_body(prg, cl, an->children[0], seen, NO_TAIL);
				ci = add_instr(I_RESTORE_CHOICE);
				ci->oper[0] = (value_t) {OPER_VAR, vnum};
				comp_body(prg, cl, an->children[1], seen, endlab);
			} else {
				comp_ensure_seen(cl, an, seen);
				vnum = findvar(cl, an->word);
				lab = make_routine_id();
				ci = add_instr(I_SAVE_CHOICE);
				ci->oper[0] = (value_t) {OPER_VAR, vnum};
				ci = add_instr(I_PUSH_CHOICE);
				ci->oper[0] = (value_t) {OPER_NUM, 0};
				ci->oper[1] = (value_t) {OPER_RLAB, lab};
				comp_body(prg, cl, an->children[0], seen, NO_TAIL);
				ci = add_instr(I_RESTORE_CHOICE);
				ci->oper[0] = (value_t) {OPER_VAR, vnum};
				comp_body(prg, cl, an->children[1], seen, endlab);
				begin_routine(lab);
				ci = add_instr(I_POP_CHOICE);
				ci->oper[0] = (value_t) {OPER_NUM, 0};
				comp_body(prg, cl, an->children[2], seen, endlab);
			}
			if(at_tail) {
				return;
			} else {
				begin_routine(endlab);
			}
			break;
		case AN_NEG_RULE:
			if(an->predicate->pred->flags & PREDF_FAIL) {
				break;
			}
			// drop through
		case AN_NEG_BLOCK:
			comp_ensure_seen(cl, an, seen);
			vnum = findvar(cl, an->word);
			endlab = make_routine_id();
			ci = add_instr(I_SAVE_CHOICE);
			ci->oper[0] = (value_t) {OPER_VAR, vnum};
			ci = add_instr(I_PUSH_CHOICE);
			ci->oper[0] = (value_t) {OPER_NUM, 0};
			ci->oper[1] = (value_t) {OPER_RLAB, endlab};
			if(an->kind == AN_NEG_BLOCK) {
				comp_body(prg, cl, an->children[0], seen, NO_TAIL);
			} else {
				comp_rule(prg, cl, an, seen, NO_TAIL);
			}
			ci = add_instr(I_RESTORE_CHOICE);
			ci->oper[0] = (value_t) {OPER_VAR, vnum};
			ci = add_instr(I_JUMP);
			ci->oper[0] = (value_t) {OPER_FAIL};
			end_routine_cl(cl);
			begin_routine(endlab);
			ci = add_instr(I_POP_CHOICE);
			ci->oper[0] = (value_t) {OPER_NUM, 0};
			break;
		case AN_EXHAUST:
			if(!(
				an->children[0]
				&& an->children[0]->kind == AN_RULE
				&& (an->children[0]->predicate->pred->flags & PREDF_FAIL))
			&& !(
				an->children[0]
				&& an->children[0]->kind == AN_BLOCK
				&& an->children[0]->children[0]
				&& an->children[0]->children[0]->kind == AN_RULE
				&& (an->children[0]->children[0]->predicate->pred->flags & PREDF_FAIL)))
			{
				comp_ensure_seen(cl, an, seen);
				endlab = make_routine_id();
				ci = add_instr(I_PUSH_CHOICE);
				ci->oper[0] = (value_t) {OPER_NUM, 0};
				ci->oper[1] = (value_t) {OPER_RLAB, endlab};
				comp_body(prg, cl, an->children[0], seen, NO_TAIL);
				ci = add_instr(I_JUMP);
				ci->oper[0] = (value_t) {OPER_FAIL};
				end_routine_cl(cl);
				begin_routine(endlab);
				ci = add_instr(I_POP_CHOICE);
				ci->oper[0] = (value_t) {OPER_NUM, 0};
			}
			break;
		case AN_FIRSTRESULT:
			vnum = findvar(cl, an->word);
			ci = add_instr(I_SAVE_CHOICE);
			ci->oper[0] = (value_t) {OPER_VAR, vnum};
			comp_body(prg, cl, an->children[0], seen, NO_TAIL);
			ci = add_instr(I_RESTORE_CHOICE);
			ci->oper[0] = (value_t) {OPER_VAR, vnum};
			break;
		case AN_SELECT:
			if(an->nchild == 1) {
				comp_body(prg, cl, an->children[0], seen, at_tail? tail : NO_TAIL);
				if(at_tail) return;
			} else {
				comp_ensure_seen(cl, an, seen);
				if(at_tail) {
					endlab = tail;
				} else {
					endlab = make_routine_id();
				}
				lab = make_routine_block(an->nchild);
				ci = add_instr(I_SELECT);
				ci->subop = an->subkind;
				ci->oper[0] = (value_t) {OPER_NUM, an->nchild};
				ci->oper[1] = (value_t) {OPER_RLAB, lab};
				if(an->subkind != SEL_P_RANDOM) {
					ci->oper[2] = (value_t) {OPER_NUM, an->value};
				}
				end_routine_cl(cl);
				for(i = 0; i < an->nchild; i++) {
					begin_routine(lab + i);
					comp_body(prg, cl, an->children[i], seen, endlab);
				}
				if(at_tail) {
					return;
				} else {
					begin_routine(endlab);
				}
			}
			break;
		case AN_JUST:
			vnum = findvar(cl, find_word(prg, "*just"));
			assert(seen[vnum]);
			ci = add_instr(I_RESTORE_CHOICE);
			ci->oper[0] = (value_t) {OPER_VAR, vnum};
			break;
		case AN_COLLECT:
			comp_ensure_seen(cl, an, seen);
			endlab = make_routine_id();
			ci = add_instr(I_COLLECT_BEGIN);
			ci = add_instr(I_PUSH_CHOICE);
			ci->oper[0] = (value_t) {OPER_NUM, 0};
			ci->oper[1] = (value_t) {OPER_RLAB, endlab};
			comp_body(prg, cl, an->children[0], seen, NO_TAIL);
			v1 = comp_value(cl, an->children[1], seen);
			ci = add_instr(I_COLLECT_PUSH);
			ci->oper[0] = v1;
			ci = add_instr(I_JUMP);
			ci->oper[0] = (value_t) {OPER_FAIL};
			end_routine_cl(cl);
			begin_routine(endlab);
			ci = add_instr(I_POP_CHOICE);
			ci->oper[0] = (value_t) {OPER_NUM, 0};
			v2 = comp_value(cl, an->children[2], seen);
			ci = add_instr(I_COLLECT_END);
			ci->oper[0] = v2;
			break;
		case AN_COLLECT_WORDS:
			comp_ensure_seen(cl, an, seen);
			endlab = make_routine_id();
			ci = add_instr(I_COLLECT_BEGIN);
			ci = add_instr(I_FOR_WORDS);
			ci->subop = 1;
			ci = add_instr(I_PUSH_CHOICE);
			ci->oper[0] = (value_t) {OPER_NUM, 0};
			ci->oper[1] = (value_t) {OPER_RLAB, endlab};
			comp_body(prg, cl, an->children[0], seen, NO_TAIL);
			ci = add_instr(I_JUMP);
			ci->oper[0] = (value_t) {OPER_FAIL};
			end_routine_cl(cl);
			begin_routine(endlab);
			ci = add_instr(I_POP_CHOICE);
			ci->oper[0] = (value_t) {OPER_NUM, 0};
			ci = add_instr(I_FOR_WORDS);
			ci->subop = 0;
			v1 = comp_value(cl, an->children[1], seen);
			ci = add_instr(I_COLLECT_END);
			ci->oper[0] = v1;
			break;
		case AN_COLLECT_WORDS_CHECK:
			comp_ensure_seen(cl, an, seen);
			endlab = make_routine_id();
			ci = add_instr(I_WORDCHECK_BEGIN);
			ci = add_instr(I_FOR_WORDS);
			ci->subop = 1;
			ci = add_instr(I_PUSH_CHOICE);
			ci->oper[0] = (value_t) {OPER_NUM, 0};
			ci->oper[1] = (value_t) {OPER_RLAB, endlab};
			comp_body(prg, cl, an->children[0], seen, NO_TAIL);
			ci = add_instr(I_JUMP);
			ci->oper[0] = (value_t) {OPER_FAIL};
			end_routine_cl(cl);
			begin_routine(endlab);
			ci = add_instr(I_POP_CHOICE);
			ci->oper[0] = (value_t) {OPER_NUM, 0};
			ci = add_instr(I_FOR_WORDS);
			ci->subop = 0;
			v1 = comp_value(cl, an->children[1], seen);
			ci = add_instr(I_WORDCHECK_END);
			ci->oper[0] = v1;
			break;
		case AN_STOPPABLE:
			comp_ensure_seen(cl, an, seen);
			endlab = make_routine_id();
			ci = add_instr(I_PUSH_STOP);
			ci->oper[0] = (value_t) {OPER_RLAB, endlab};
			comp_body(prg, cl, an->children[0], seen, NO_TAIL);
			ci = add_instr(I_STOP);
			end_routine_cl(cl);
			begin_routine(endlab);
			ci = add_instr(I_POP_CHOICE);
			ci->oper[0] = (value_t) {OPER_NUM, 0};
			ci = add_instr(I_POP_STOP);
			break;
		case AN_STATUSBAR:
			comp_ensure_seen(cl, an, seen);
			v1 = comp_value(cl, an->children[0], seen);
			ci = add_instr(I_BEGIN_STATUS);
			ci->oper[0] = v1;
			endlab = make_routine_id();
			ci = add_instr(I_PUSH_STOP);
			ci->oper[0] = (value_t) {OPER_RLAB, endlab};
			comp_body(prg, cl, an->children[1], seen, NO_TAIL);
			ci = add_instr(I_STOP);
			end_routine_cl(cl);
			begin_routine(endlab);
			ci = add_instr(I_POP_CHOICE);
			ci->oper[0] = (value_t) {OPER_NUM, 0};
			ci = add_instr(I_POP_STOP);
			ci = add_instr(I_END_STATUS);
			break;
		case AN_NOW:
			comp_now(prg, cl, an->children[0], seen);
			break;
		case AN_BAREWORD:
			if(ninstr && (ci = &instrbuf[ninstr - 1])->op == I_PRINT_WORDS) {
				if(ci->oper[1].tag == VAL_NONE) {
					ci->oper[1] = (value_t) {OPER_WORD, an->word->word_id};
				} else if(ci->oper[2].tag == VAL_NONE) {
					ci->oper[2] = (value_t) {OPER_WORD, an->word->word_id};
				} else {
					ci = add_instr(I_PRINT_WORDS);
					ci->oper[0] = (value_t) {OPER_WORD, an->word->word_id};
				}
			} else {
				ci = add_instr(I_PRINT_WORDS);
				ci->oper[0] = (value_t) {OPER_WORD, an->word->word_id};
			}
			break;
		case AN_DICTWORD:
		case AN_TAG:
		case AN_VARIABLE:
		case AN_INTEGER:
		case AN_PAIR:
		case AN_EMPTY_LIST:
			v1 = comp_value(cl, an, seen);
			ci = add_instr(I_PRINT_VAL);
			ci->oper[0] = v1;
			break;
		default:
			assert(0);
		}
		an = an->next_in_body;
	}

	if(tail == CONT_TAIL) {
		ci = add_instr(I_DEALLOCATE);
		ci = add_instr(I_PROCEED);
		end_routine_cl(cl);
	} else if(tail != NO_TAIL) {
		ci = add_instr(I_JUMP);
		ci->oper[0] = (value_t) {OPER_RLAB, tail};
		end_routine_cl(cl);
	}
}

static int variable_mentioned_in(struct word *w, struct astnode *an) {
	int i;

	while(an) {
		if(an->kind == AN_VARIABLE) {
			if(an->word == w) return 1;
		} else {
			for(i = 0; i < an->nchild; i++) {
				if(variable_mentioned_in(w, an->children[i])) {
					return 1;
				}
			}
		}
		an = an->next_in_body;
	}

	return 0;
}

struct index_entry {
	value_t		key;
	uint16_t	n_drop_from_arg0;
	uint16_t	n_drop_from_body;
	uint16_t	clause_id;
};

static int cmp_index_entry(const void *a, const void *b) {
	const struct index_entry *aa = a;
	const struct index_entry *bb = b;
	int diff;

	diff = aa->key.tag - bb->key.tag;
	if(diff) return diff;
	diff = aa->key.value - bb->key.value;
	if(diff) return diff;
	diff = aa->clause_id - bb->clause_id; // enforce a stable sort
	return diff;
}

static void parse_index_entry(struct predicate *pred, struct index_entry *entry, struct astnode **arg0, struct astnode **body) {
	struct astnode *an;
	int i;

	assert(pred->predname->arity);
	an = pred->clauses[entry->clause_id]->params[0];
	for(i = 0; i < entry->n_drop_from_arg0; i++) {
		assert(an->kind == AN_PAIR);
		an = an->children[1];
	}
	*arg0 = an;
	an = pred->clauses[entry->clause_id]->body;
	for(i = 0; i < entry->n_drop_from_body; i++) {
		an = an->next_in_body;
	}
	*body = an;
}

static int number_of_identical_keys(struct index_entry *entry, int nentry) {
	int i = 1;

	while(i < nentry
	&& entry[i].key.tag == entry[0].key.tag
	&& entry[i].key.value == entry[0].key.value) {
		i++;
	}

	return i;
}

static void comp_clause(struct program *prg, struct predicate *pred, struct index_entry *entry) {
	struct cinstr *ci;
	int i, j, vnum;
	struct clause *cl = pred->clauses[entry->clause_id];
	uint8_t seen[cl->nvar];
	struct astnode *an, *body;

	memset(seen, 0, cl->nvar);
	ntemp = 0;

	ci = add_instr(I_ALLOCATE);
	ci->oper[0] = (value_t) {OPER_NUM, cl->nvar};
	ci->oper[1] = (value_t) {OPER_NUM, cl->max_call_arity};

	for(i = 0; i < cl->predicate->arity; i++) {
		an = cl->params[i];
		if(!i) {
			for(j = 0; j < entry->n_drop_from_arg0; j++) {
				assert(an->kind == AN_PAIR);
				an = an->children[1];
			}
		}
		comp_param(cl, an, (value_t) {OPER_ARG, i}, seen);
	}

	body = cl->body;
	for(i = 0; i < entry->n_drop_from_body; i++) {
		body = body->next_in_body;
	}

	if((cl->predicate->pred->flags & PREDF_CONTAINS_JUST)
	&& contains_just(body)) {
		vnum = findvar(cl, find_word(prg, "*just"));
		ci = add_instr(I_ASSIGN);
		ci->oper[0] = (value_t) {OPER_VAR, vnum};
		ci->oper[1] = (value_t) {OPER_ARG, cl->predicate->arity};
		seen[vnum] = 1;
	}

	if(cl->predicate->builtin != BI_INJECTED_QUERY) {
		ci = add_instr(I_TRACEPOINT);
		ci->subop = TR_ENTER;
		ci->oper[0] = (value_t) {OPER_FILE, FILENUMPART(cl->line)};
		ci->oper[1] = (value_t) {OPER_NUM, LINEPART(cl->line)};
		ci->oper[2] = (value_t) {OPER_PRED, cl->predicate->pred_id};
	}

	comp_body(prg, cl, body, seen, CONT_TAIL);
}

static int is_indexable_value(struct astnode *param, struct astnode *body, int *nval) {
	struct astnode *an;

	if(param->kind == AN_PAIR) {
		return 0;
	} else if(param->kind == AN_VARIABLE) {
		if(!param->word->name[0]) return 0;
		if(body
		&& body->kind == AN_RULE
		&& body->predicate->builtin == BI_IS_ONE_OF
		&& body->children[0]->kind == AN_VARIABLE
		&& body->children[0]->word == param->word
		&& !variable_mentioned_in(param->word, body->next_in_body)
		&& comp_simple_constant_list(body->children[1])) {
			for(an = body->children[1]; an->kind != AN_EMPTY_LIST; an = an->children[1]) {
				(*nval)++;
			}
			return 1;
		}
		return 0;
	} else {
		(*nval)++;
		return 1;
	}
}

static int can_be_directly_indexed(struct predicate *pred, struct index_entry *entry, int *nval) {
	struct astnode *body, *param;

	if(!pred->predname->arity) return 0;

	parse_index_entry(pred, entry, &param, &body);
	return is_indexable_value(param, body, nval);
}

static int can_be_indirectly_indexed(struct predicate *pred, struct index_entry *entry, int *nval) {
	struct astnode *body, *param;

	if(!pred->predname->arity) return 0;

	parse_index_entry(pred, entry, &param, &body);
	if(param->kind == AN_PAIR) {
		return is_indexable_value(param->children[0], body, nval);
	} else {
		return 0;
	}
}

static void comp_clause_chain_unbound(struct program *prg, struct predicate *pred, struct index_entry *entries, int nentry) {
	int i, last, narg;
	int next = -1;
	struct cinstr *ci;

	assert(nentry);
	narg = pred->predname->arity + !!(pred->flags & PREDF_CONTAINS_JUST);
	for(i = 0; i < nentry; i++) {
		last = (i == nentry - 1);
		if(!last) {
			next = make_routine_id();
			ci = add_instr(I_PUSH_CHOICE);
			ci->oper[0] = (value_t) {OPER_NUM, narg};
			ci->oper[1] = (value_t) {OPER_RLAB, next};
		}
		comp_clause(prg, pred, &entries[i]);
		if(!last) {
			begin_routine(next);
			ci = add_instr(I_POP_CHOICE);
			ci->oper[0] = (value_t) {OPER_NUM, narg};
		}
	}
}

static int comp_direct_index_entry(struct predicate *pred, struct index_entry *src, struct index_entry *dest) {
	struct astnode *param, *body, *an;
	int count;

	parse_index_entry(pred, src, &param, &body);
	if(param->kind == AN_VARIABLE) {
		assert(param->word->name[0]);
		assert(body);
		assert(body->kind == AN_RULE);
		assert(body->predicate->builtin == BI_IS_ONE_OF);
		assert(body->children[0]->kind == AN_VARIABLE);
		assert(body->children[0]->word == param->word);
		count = 0;
		for(an = body->children[1]; an->kind != AN_EMPTY_LIST; an = an->children[1]) {
			assert(an->kind == AN_PAIR);
			dest[count].key = comp_tag_simple(an->children[0]);
			dest[count].n_drop_from_arg0 = src->n_drop_from_arg0;
			dest[count].n_drop_from_body = 1 + src->n_drop_from_body;
			dest[count].clause_id = src->clause_id;
			count++;
		}
		return count;
	} else {
		dest->key = comp_tag_simple(param);
		dest->n_drop_from_arg0 = src->n_drop_from_arg0;
		dest->n_drop_from_body = src->n_drop_from_body;
		dest->clause_id = src->clause_id;
		return 1;
	}
}

static int comp_indirect_index_entry(struct predicate *pred, struct index_entry *src, struct index_entry *dest) {
	int count;
	struct astnode *param, *body, *an;

	parse_index_entry(pred, src, &param, &body);

	assert(param->kind == AN_PAIR);
	param = param->children[0];

	if(param->kind == AN_VARIABLE) {
		assert(param->word->name[0]);
		assert(body);
		assert(body->kind == AN_RULE);
		assert(body->predicate->builtin == BI_IS_ONE_OF);
		assert(body->children[0]->kind == AN_VARIABLE);
		assert(body->children[0]->word == param->word);
		count = 0;
		for(an = body->children[1]; an->kind != AN_EMPTY_LIST; an = an->children[1]) {
			assert(an->kind == AN_PAIR);
			dest[count].key = comp_tag_simple(an->children[0]);
			dest[count].n_drop_from_arg0 = 1 + src->n_drop_from_arg0;
			dest[count].n_drop_from_body = 1 + src->n_drop_from_body;
			dest[count].clause_id = src->clause_id;
			count++;
		}
		return count;
	} else {
		dest->key = comp_tag_simple(param);
		dest->n_drop_from_arg0 = 1 + src->n_drop_from_arg0;
		dest->n_drop_from_body = src->n_drop_from_body;
		dest->clause_id = src->clause_id;
		return 1;
	}

	return count;
}

static void comp_clause(struct program *prg, struct predicate *pred, struct index_entry *entry);
static void comp_clause_chain(struct program *prg, struct predicate *pred, struct index_entry *entries, int nentry);

struct index_target {
	int	first;
	int	nentry;
	int	label;
};

static int find_index_target(struct index_target *table, int *size, struct index_entry *entries, int first, int nentry) {
	int i, j;
	struct index_target *t;

	for(i = 0; i < *size; i++) {
		t = &table[i];
		if(t->nentry == nentry) {
			for(j = 0; j < nentry; j++) {
				if(entries[first + j].clause_id != entries[t->first + j].clause_id
				|| entries[first + j].n_drop_from_arg0 != entries[t->first + j].n_drop_from_arg0
				|| entries[first + j].n_drop_from_body != entries[t->first + j].n_drop_from_body) {
					break;
				}
			}
			if(j == nentry) {
				return i;
			}
		}
	}

	(*size)++;
	t = &table[i];
	t->first = first;
	t->nentry = nentry;
	t->label = make_routine_id();

	return i;
}

static void comp_index_check_and_go(struct program *prg, struct predicate *pred, struct index_entry *entries, int nentry, int nfork, int indirect) {
	int i, j, n;
	struct cinstr *ci;
	struct index_target targets[nfork], *t;
	int ntarget = 0;
	int target_map[nfork];

	j = 0;
	for(i = 0; i < nfork; i++) {
		n = number_of_identical_keys(entries + j, nentry - j);
		target_map[i] = find_index_target(targets, &ntarget, entries, j, n);
		j += n;
	}
	assert(j == nentry);
	assert(ntarget <= nfork);

	j = 0;
	for(i = 0; i < nfork; i++) {
		ci = add_instr(I_CHECK_INDEX);
		ci->oper[0] = entries[j].key;
		ci->oper[1] = (value_t) {OPER_RLAB, targets[target_map[i]].label};
		j += number_of_identical_keys(entries + j, nentry - j);
	}
	assert(j == nentry);
	ci = add_instr(I_JUMP);
	ci->oper[0] = (value_t) {OPER_FAIL};
	end_routine(0xffff, &pred->arena);

	for(i = 0; i < ntarget; i++) {
		t = &targets[i];
		begin_routine(t->label);
		if(indirect) {
			if(t->nentry == 1) {
				comp_clause(prg, pred, &entries[t->first]);
			} else {
				comp_clause_chain(prg, pred, entries + t->first, t->nentry);
			}
		} else {
			comp_clause_chain_unbound(prg, pred, entries + t->first, t->nentry);
		}
		j += n;
	}
}

static void comp_direct_index_block(struct program *prg, struct predicate *pred, struct index_entry *incoming, int nincoming, int nval) {
	int i, j, nfork;
	struct index_entry entries[nval];
	struct cinstr *ci;

	j = 0;
	for(i = 0; i < nincoming; i++) {
		j += comp_direct_index_entry(pred, incoming + i, entries + j);
	}
	assert(j == nval);

	qsort(entries, nval, sizeof(struct index_entry), cmp_index_entry);

	nfork = 0;
	for(i = 0; i < nval; i += number_of_identical_keys(entries + i, nval - i)) {
		nfork++;
	}

	ci = add_instr(I_PREPARE_INDEX);
	ci->oper[0] = (value_t) {OPER_ARG, 0};

	comp_index_check_and_go(prg, pred, entries, nval, nfork, 0);
}

static void comp_indirect_index_block(struct program *prg, struct predicate *pred, struct index_entry *incoming, int nincoming, int nval) {
	int i, j, nfork;
	struct index_entry entries[nval];
	struct cinstr *ci;

	ci = add_instr(I_ASSIGN);
	ci->oper[0] = (value_t) {OPER_ARG, 0};
	ci->oper[1] = (value_t) {OPER_TEMP, 1};

	j = 0;
	for(i = 0; i < nincoming; i++) {
		j += comp_indirect_index_entry(pred, incoming + i, entries + j);
	}
	assert(j == nval);

	qsort(entries, nval, sizeof(struct index_entry), cmp_index_entry);

	nfork = 0;
	for(i = 0; i < nval; i += number_of_identical_keys(entries + i, nval - i)) {
		nfork++;
	}

	ci = add_instr(I_PREPARE_INDEX);
	ci->oper[0] = (value_t) {OPER_TEMP, 0};

	comp_index_check_and_go(prg, pred, entries, nval, nfork, 1);
}

static void comp_clause_chain(struct program *prg, struct predicate *pred, struct index_entry *entries, int nentry) {
	int i, last, narg, chunkcount, nval;
	int next = -1, lab;
	struct cinstr *ci;

	assert(nentry);
	narg = pred->predname->arity + !!(pred->flags & PREDF_CONTAINS_JUST);
	for(i = 0; i < nentry; ) {
		nval = 0;
		if(can_be_directly_indexed(pred, &entries[i], &nval)) {
			chunkcount = 1;
			while(i + chunkcount < nentry && can_be_directly_indexed(pred, &entries[i + chunkcount], &nval)) {
				chunkcount++;
			}
			last = (i + chunkcount == nentry);
			if(!last) {
				next = make_routine_id();
				ci = add_instr(I_PUSH_CHOICE);
				ci->oper[0] = (value_t) {OPER_NUM, narg};
				ci->oper[1] = (value_t) {OPER_RLAB, next};
			}
			if(chunkcount > 1) {
				lab = make_routine_id();
				ci = add_instr(I_IF_BOUND);
				ci->oper[0] = (value_t) {OPER_ARG, 0};
				ci->oper[1] = (value_t) {OPER_RLAB, lab};
				comp_clause_chain_unbound(prg, pred, entries + i, chunkcount);

				begin_routine(lab);
				comp_direct_index_block(prg, pred, entries + i, chunkcount, nval);
			} else {
				comp_clause(prg, pred, &entries[i]);
			}
			if(!last) {
				begin_routine(next);
				ci = add_instr(I_POP_CHOICE);
				ci->oper[0] = (value_t) {OPER_NUM, narg};
			}
			i += chunkcount;
		} else if(can_be_indirectly_indexed(pred, &entries[i], &nval)) {
			chunkcount = 1;
			while(i + chunkcount < nentry && can_be_indirectly_indexed(pred, &entries[i + chunkcount], &nval)) {
				chunkcount++;
			}
			last = (i + chunkcount == nentry);
			if(!last) {
				next = make_routine_id();
				ci = add_instr(I_PUSH_CHOICE);
				ci->oper[0] = (value_t) {OPER_NUM, narg};
				ci->oper[1] = (value_t) {OPER_RLAB, next};
			}
			if(chunkcount > 1) {
				lab = make_routine_id();
				ci = add_instr(I_GET_PAIR_RR);
				ci->oper[0] = (value_t) {OPER_ARG, 0};
				ci->oper[1] = (value_t) {OPER_TEMP, 0};
				ci->oper[2] = (value_t) {OPER_TEMP, 1};
				ci = add_instr(I_IF_BOUND);
				ci->oper[0] = (value_t) {OPER_TEMP, 0};
				ci->oper[1] = (value_t) {OPER_RLAB, lab};
				comp_clause_chain_unbound(prg, pred, entries + i, chunkcount);

				begin_routine(lab);
				comp_indirect_index_block(prg, pred, entries + i, chunkcount, nval);
			} else {
				comp_clause(prg, pred, &entries[i]);
			}
			if(!last) {
				begin_routine(next);
				ci = add_instr(I_POP_CHOICE);
				ci->oper[0] = (value_t) {OPER_NUM, narg};
			}
			i += chunkcount;
		} else {
			last = (i + 1 == nentry);
			if(!last) {
				next = make_routine_id();
				ci = add_instr(I_PUSH_CHOICE);
				ci->oper[0] = (value_t) {OPER_NUM, narg};
				ci->oper[1] = (value_t) {OPER_RLAB, next};
			}
			comp_clause(prg, pred, &entries[i]);
			if(!last) {
				begin_routine(next);
				ci = add_instr(I_POP_CHOICE);
				ci->oper[0] = (value_t) {OPER_NUM, narg};
			}
			i++;
		}
	}
}

void comp_dump_routine(struct program *prg, struct clause *cl, struct comp_routine *r) {
	int i, j;
	value_t v;
	struct opinfo *info;

	for(i = 0; i < r->ninstr; i++) {
		char namebuf[20];

		printf("\t");
		info = &opinfo[r->instr[i].op];
		if(info->flags & OPF_SUBOP) {
			snprintf(namebuf, sizeof(namebuf), "%s %d", info->name, r->instr[i].subop);
		} else {
			snprintf(namebuf, sizeof(namebuf), "%s", info->name);
		}
		printf("%-19s", namebuf);
		for(j = 0; j < 3; j++) {
			char buf[13];

			v = r->instr[i].oper[j];
			switch(v.tag) {
			case VAL_NUM:
			case OPER_NUM:
				snprintf(buf, sizeof(buf), "%d", v.value);
				break;
			case VAL_OBJ:
				assert(v.value < prg->nworldobj);
				snprintf(buf, sizeof(buf), "#%s", prg->worldobjnames[v.value]->name);
				break;
			case VAL_DICT:
				if(v.value < 256) {
					if(v.value > 32 && v.value < 127) {
						snprintf(buf, sizeof(buf), "@%c", v.value);
					} else {
						snprintf(buf, sizeof(buf), "@\\%02x", v.value);
					}
				} else {
					assert(v.value - 256 < prg->ndictword);
					snprintf(buf, sizeof(buf), "@%s", prg->dictwordnames[v.value - 256]->name);
				}
				break;
			case VAL_NIL:
				snprintf(buf, sizeof(buf), "[]");
				break;
			case OPER_ARG:
				snprintf(buf, sizeof(buf), "A%d", v.value);
				break;
			case OPER_TEMP:
				snprintf(buf, sizeof(buf), "X%d", v.value);
				break;
			case OPER_VAR:
				if(cl) {
					assert(v.value < cl->nvar);
					snprintf(buf, sizeof(buf), "V%d/$%s", v.value, cl->varnames[v.value]->name);
				} else {
					snprintf(buf, sizeof(buf), "V%d", v.value);
				}
				break;
			case OPER_RLAB:
				snprintf(buf, sizeof(buf), "R%d", v.value);
				break;
			case OPER_FAIL:
				snprintf(buf, sizeof(buf), "<FAIL>");
				break;
			case OPER_GFLAG:
				snprintf(buf, sizeof(buf), "GF%d", v.value);
				break;
			case OPER_GVAR:
				snprintf(buf, sizeof(buf), "GV%d", v.value);
				break;
			case OPER_OFLAG:
				snprintf(buf, sizeof(buf), "OF%d", v.value);
				break;
			case OPER_OVAR:
				snprintf(buf, sizeof(buf), "OV%d", v.value);
				break;
			case OPER_PRED:
				printf(" %s", prg->predicates[v.value]->printed_name);
				break;
			case OPER_FILE:
				assert(j == 0);
				assert(r->instr[i].oper[1].tag == OPER_NUM);
				printf(" %s:%d", sourcefile[v.value], r->instr[i].oper[1].value);
				break;
			case OPER_WORD:
				assert(v.value < prg->nword);
				snprintf(buf, sizeof(buf), "\"%s\"", prg->allwords[v.value]->name);
				break;
			case VAL_NONE:
				snprintf(buf, sizeof(buf), "-");
				break;
			default:
				assert(0);
			}

			if(v.tag == OPER_PRED || v.tag == OPER_FILE || v.tag == OPER_STR) break;

			printf(" %-12s", buf);
		}
		v = r->instr[i].oper[0];
		if(v.tag == OPER_GFLAG) {
			assert(v.value < prg->nglobalflag);
			printf(" %s", prg->globalflagpred[v.value]->printed_name);
		} else if(v.tag == OPER_GVAR) {
			assert(v.value < prg->nglobalvar);
			printf(" %s", prg->globalvarpred[v.value]->printed_name);
		} else if(v.tag == OPER_OFLAG) {
			assert(v.value < prg->nobjflag);
			printf(" %s", prg->objflagpred[v.value]->printed_name);
		} else if(v.tag == OPER_OVAR) {
			assert(v.value < prg->nobjvar);
			printf(" %s", prg->objvarpred[v.value]->printed_name);
		}
		printf("\n");
	}
}

void comp_dump_predicate(struct program *prg, struct predname *predname) {
	int i;
	struct predicate *pred = predname->pred;
	uint16_t cid;

	printf("Intermediate code for %s: %d %d\n",
		predname->printed_name,
		pred->normal_entry,
		pred->initial_value_entry);

	for(i = 0; i < pred->nroutine; i++) {
		printf("R%d:\n", i);
		cid = pred->routines[i].clause_id;
		comp_dump_routine(prg, (cid == 0xffff)? 0 : pred->clauses[cid], &pred->routines[i]);
	}
}

void comp_builtin(struct program *prg, int builtin) {
	struct predname *predname = find_builtin(prg, builtin);
	struct predicate *pred = predname->pred;
	struct cinstr *ci;
	int lab, labloop, labcheck, labnext, labmatch;
	int i;

	memset(routines, 0, nroutine * sizeof(struct comp_routine));
	nroutine = 0;
	pred->normal_entry = make_routine_id();
	begin_routine(pred->normal_entry);

	switch(builtin) {
	case BI_IS_ONE_OF:
		lab = make_routine_id();
		ci = add_instr(I_GET_PAIR_RR);
		ci->oper[0] = (value_t) {OPER_ARG, 1};
		ci->oper[1] = (value_t) {OPER_TEMP, 0};
		ci->oper[2] = (value_t) {OPER_ARG, 1};
		ci = add_instr(I_PUSH_CHOICE);
		ci->oper[0] = (value_t) {OPER_NUM, 2};
		ci->oper[1] = (value_t) {OPER_RLAB, lab};
		ci = add_instr(I_UNIFY);
		ci->oper[0] = (value_t) {OPER_ARG, 0};
		ci->oper[1] = (value_t) {OPER_TEMP, 0};
		ci = add_instr(I_PROCEED);
		end_routine(0xffff, &pred->arena);
		begin_routine(lab);
		ci = add_instr(I_POP_CHOICE);
		ci->oper[0] = (value_t) {OPER_NUM, 2};
		ci = add_instr(I_GET_PAIR_RR);
		ci->oper[0] = (value_t) {OPER_ARG, 1};
		ci->oper[1] = (value_t) {OPER_TEMP, 0};
		ci->oper[2] = (value_t) {OPER_ARG, 1};
		ci = add_instr(I_PUSH_CHOICE);
		ci->oper[0] = (value_t) {OPER_NUM, 2};
		ci->oper[1] = (value_t) {OPER_RLAB, lab};
		ci = add_instr(I_UNIFY);
		ci->oper[0] = (value_t) {OPER_ARG, 0};
		ci->oper[1] = (value_t) {OPER_TEMP, 0};
		ci = add_instr(I_PROCEED);
		end_routine(0xffff, &pred->arena);
		break;
	case BI_SPLIT:
		// v0 = input list
		// v1 = keyword list
		// v2 = left output
		// v3 = right output
		// v4 = input list next
		// v5 = input list current
		lab = make_routine_id();
		labnext = make_routine_id();
		labloop = make_routine_id();
		labcheck = make_routine_id();
		labmatch = make_routine_id();
		ci = add_instr(I_IF_BOUND);
		ci->oper[0] = (value_t) {OPER_ARG, 0};
		ci->oper[1] = (value_t) {OPER_RLAB, lab};
		ci = add_instr(I_JUMP);
		ci->oper[0] = (value_t) {OPER_FAIL};
		end_routine(0xffff, &pred->arena);

		begin_routine(lab);
		ci = add_instr(I_ALLOCATE);
		ci->oper[0] = (value_t) {OPER_NUM, 6};
		ci->oper[1] = (value_t) {OPER_NUM, 0};
		ci = add_instr(I_ASSIGN);
		ci->oper[0] = (value_t) {OPER_VAR, 0};
		ci->oper[1] = (value_t) {OPER_ARG, 0};
		ci = add_instr(I_ASSIGN);
		ci->oper[0] = (value_t) {OPER_VAR, 1};
		ci->oper[1] = (value_t) {OPER_ARG, 1};
		ci = add_instr(I_ASSIGN);
		ci->oper[0] = (value_t) {OPER_VAR, 2};
		ci->oper[1] = (value_t) {OPER_ARG, 2};
		ci = add_instr(I_ASSIGN);
		ci->oper[0] = (value_t) {OPER_VAR, 3};
		ci->oper[1] = (value_t) {OPER_ARG, 3};
		ci = add_instr(I_ASSIGN);
		ci->oper[0] = (value_t) {OPER_VAR, 4};
		ci->oper[1] = (value_t) {OPER_ARG, 0};
		ci = add_instr(I_IF_BOUND);
		ci->oper[0] = (value_t) {OPER_ARG, 1};
		ci->oper[1] = (value_t) {OPER_RLAB, labloop};
		ci = add_instr(I_JUMP);
		ci->oper[0] = (value_t) {OPER_FAIL};
		end_routine(0xffff, &pred->arena);

		begin_routine(labnext);
		ci = add_instr(I_POP_CHOICE);
		ci->oper[0] = (value_t) {OPER_NUM, 0};
		ci = add_instr(I_JUMP);
		ci->oper[0] = (value_t) {OPER_RLAB, labloop};
		end_routine(0xffff, &pred->arena);

		begin_routine(labloop);
		ci = add_instr(I_ASSIGN);
		ci->oper[0] = (value_t) {OPER_VAR, 5};
		ci->oper[1] = (value_t) {OPER_VAR, 4};
		ci = add_instr(I_GET_PAIR_RR);
		ci->oper[0] = (value_t) {OPER_VAR, 4};
		ci->oper[1] = (value_t) {OPER_TEMP, 1};
		ci->oper[2] = (value_t) {OPER_VAR, 4};
		ci = add_instr(I_ASSIGN);
		ci->oper[0] = (value_t) {OPER_TEMP, 0};
		ci->oper[1] = (value_t) {OPER_VAR, 1};
		ci = add_instr(I_JUMP);
		ci->oper[0] = (value_t) {OPER_RLAB, labcheck};
		end_routine(0xffff, &pred->arena);

		begin_routine(labcheck);
		ci = add_instr(I_IF_NIL);
		ci->oper[0] = (value_t) {OPER_TEMP, 0};
		ci->oper[1] = (value_t) {OPER_RLAB, labloop};
		ci = add_instr(I_GET_PAIR_RR);
		ci->oper[0] = (value_t) {OPER_TEMP, 0};
		ci->oper[1] = (value_t) {OPER_TEMP, 2};
		ci->oper[2] = (value_t) {OPER_TEMP, 0};
		ci = add_instr(I_IF_MATCH);
		ci->oper[0] = (value_t) {OPER_TEMP, 1};
		ci->oper[1] = (value_t) {OPER_TEMP, 2};
		ci->oper[2] = (value_t) {OPER_RLAB, labmatch};
		ci = add_instr(I_JUMP);
		ci->oper[0] = (value_t) {OPER_RLAB, labcheck};
		end_routine(0xffff, &pred->arena);

		begin_routine(labmatch);
		ci = add_instr(I_PUSH_CHOICE);
		ci->oper[0] = (value_t) {OPER_NUM, 0};
		ci->oper[1] = (value_t) {OPER_RLAB, labnext};
		ci = add_instr(I_UNIFY);
		ci->oper[0] = (value_t) {OPER_VAR, 4};
		ci->oper[1] = (value_t) {OPER_VAR, 3};
		ci = add_instr(I_SPLIT_LIST);
		ci->oper[0] = (value_t) {OPER_VAR, 0};
		ci->oper[1] = (value_t) {OPER_VAR, 5};
		ci->oper[2] = (value_t) {OPER_VAR, 2};
		ci = add_instr(I_DEALLOCATE);
		ci = add_instr(I_PROCEED);
		end_routine(0xffff, &pred->arena);
		break;
	case BI_REPEAT:
		lab = make_routine_id();
		ci = add_instr(I_PUSH_CHOICE);
		ci->oper[0] = (value_t) {OPER_NUM, 0};
		ci->oper[1] = (value_t) {OPER_RLAB, lab};
		ci = add_instr(I_PROCEED);
		end_routine(0xffff, &pred->arena);
		begin_routine(lab);
		ci = add_instr(I_POP_CHOICE);
		ci->oper[0] = (value_t) {OPER_NUM, 0};
		ci = add_instr(I_PUSH_CHOICE);
		ci->oper[0] = (value_t) {OPER_NUM, 0};
		ci->oper[1] = (value_t) {OPER_RLAB, lab};
		ci = add_instr(I_PROCEED);
		end_routine(0xffff, &pred->arena);
		break;
	case BI_GETINPUT:
		// this cannot be inlined because the debugger may want to inject queries
		ci = add_instr(I_GET_INPUT);
		end_routine(0xffff, &pred->arena);
		break;
	case BI_GETRAWINPUT:
		// this cannot be inlined because the debugger may want to inject queries
		ci = add_instr(I_GET_RAW_INPUT);
		end_routine(0xffff, &pred->arena);
		break;
	case BI_GETKEY:
		// this cannot be inlined because the debugger may want to suspend
		ci = add_instr(I_GET_KEY);
		end_routine(0xffff, &pred->arena);
		break;
	case BI_BREAK_GETKEY:
		lab = make_routine_id();
		ci = add_instr(I_ALLOCATE);
		ci->oper[0] = (value_t) {OPER_NUM, 1};
		ci->oper[1] = (value_t) {OPER_NUM, 1};
		ci = add_instr(I_ASSIGN);
		ci->oper[0] = (value_t) {OPER_VAR, 0};
		ci->oper[1] = (value_t) {OPER_ARG, 0};
		ci = add_instr(I_SET_CONT);
		ci->oper[0] = (value_t) {OPER_RLAB, lab};
		ci = add_instr(I_BREAKPOINT);
		ci->subop = 0;
		end_routine(0xffff, &pred->arena);
		begin_routine(lab);
		ci = add_instr(I_ASSIGN);
		ci->oper[0] = (value_t) {OPER_ARG, 0};
		ci->oper[1] = (value_t) {OPER_VAR, 0};
		ci = add_instr(I_DEALLOCATE);
		ci = add_instr(I_GET_KEY);
		end_routine(0xffff, &pred->arena);
		break;
	case BI_OBJECT:
		if(prg->nworldobj) {
			labloop = 0; // avoid gcc warning
			if(prg->nworldobj > 1) {
				labloop = make_routine_id();
			}

			lab = make_routine_id();
			ci = add_instr(I_IF_BOUND);
			ci->oper[0] = (value_t) {OPER_ARG, 0};
			ci->oper[1] = (value_t) {OPER_RLAB, lab};
			if(prg->nworldobj > 1) {
				ci = add_instr(I_NEXT_OBJ_PUSH);
				ci->oper[0] = (value_t) {VAL_OBJ, 0};
				ci->oper[1] = (value_t) {OPER_RLAB, labloop};
			}
			ci = add_instr(I_UNIFY);
			ci->oper[0] = (value_t) {OPER_ARG, 0};
			ci->oper[1] = (value_t) {VAL_OBJ, 0};
			ci = add_instr(I_PROCEED);
			end_routine(0xffff, &pred->arena);

			if(prg->nworldobj > 1) {
				begin_routine(labloop);
				ci = add_instr(I_POP_CHOICE);
				ci->oper[0] = (value_t) {OPER_NUM, 2};
				ci = add_instr(I_NEXT_OBJ_PUSH);
				ci->oper[0] = (value_t) {OPER_ARG, 1};
				ci->oper[1] = (value_t) {OPER_RLAB, labloop};
				ci = add_instr(I_UNIFY);
				ci->oper[0] = (value_t) {OPER_ARG, 0};
				ci->oper[1] = (value_t) {OPER_ARG, 1};
				ci = add_instr(I_PROCEED);
				end_routine(0xffff, &pred->arena);
			}

			begin_routine(lab);
			ci = add_instr(I_BUILTIN);
			ci->oper[0] = (value_t) {OPER_ARG, 0};
			ci->oper[2] = (value_t) {OPER_PRED, predname->pred_id};
			ci = add_instr(I_PROCEED);
			end_routine(0xffff, &pred->arena);
		} else {
			ci = add_instr(I_JUMP);
			ci->oper[0] = (value_t) {OPER_FAIL};
			end_routine(0xffff, &pred->arena);
		}
		break;
	case BI_BREAKPOINT:
	case BI_BREAKPOINT_AGAIN:
		// this cannot be inlined because the debugger wants to resume with a continuation
		ci = add_instr(I_BREAKPOINT);
		ci->subop = (builtin == BI_BREAKPOINT_AGAIN);
		end_routine(0xffff, &pred->arena);
		break;
	case BI_BREAK_FAIL:
		lab = make_routine_id();
		ci = add_instr(I_ALLOCATE);
		ci->oper[0] = (value_t) {OPER_NUM, 0};
		ci->oper[1] = (value_t) {OPER_NUM, 0};
		ci = add_instr(I_SET_CONT);
		ci->oper[0] = (value_t) {OPER_RLAB, lab};
		ci = add_instr(I_BREAKPOINT);
		ci->subop = 0;
		end_routine(0xffff, &pred->arena);
		begin_routine(lab);
		ci = add_instr(I_JUMP);
		ci->oper[0] = (value_t) {OPER_FAIL};
		end_routine(0xffff, &pred->arena);
		break;
	case BI_SAVE:
		ci = add_instr(I_SAVE);
		end_routine(0xffff, &pred->arena);
		break;
	case BI_SAVE_UNDO:
		ci = add_instr(I_SAVE_UNDO);
		end_routine(0xffff, &pred->arena);
		break;
	case BI_RESTORE:
		ci = add_instr(I_RESTORE);
		ci = add_instr(I_PROCEED);
		end_routine(0xffff, &pred->arena);
		break;
	case BI_UNDO:
		ci = add_instr(I_UNDO);
		ci = add_instr(I_PROCEED);
		end_routine(0xffff, &pred->arena);
		break;
	case BI_FAIL:
		ci = add_instr(I_JUMP);
		ci->oper[0] = (value_t) {OPER_FAIL};
		end_routine(0xffff, &pred->arena);
		break;
	default:
		assert(0);
	}

	pred->routines = arena_alloc(&pred->arena, nroutine * sizeof(struct comp_routine));
	memcpy(pred->routines, routines, nroutine * sizeof(struct comp_routine));
	pred->nroutine = nroutine;
	if(verbose >= 4) {
		printf("Intermediate code for builtin %d, %s:\n", builtin, predname->printed_name);
		for(i = 0; i < pred->nroutine; i++) {
			printf("R%d:\n", i);
			comp_dump_routine(prg, 0, &pred->routines[i]);
		}
	}
}

void comp_dyn_list(struct program *prg, struct predname *predname) {
	struct predicate *pred = predname->pred;
	int lab1, lab2;
	struct cinstr *ci;

	assert(predname->dyn_id != DYN_NONE);

	lab1 = make_routine_id();
	lab2 = make_routine_id();

	ci = add_instr(I_IF_BOUND);
	ci->oper[0] = (value_t) {OPER_ARG, 0};
	ci->oper[1] = (value_t) {OPER_RLAB, lab1};
	if(pred->flags & PREDF_FIXED_FLAG) {
		ci = add_instr(I_JUMP);
		ci->oper[0] = (value_t) {OPER_RLAB, pred->initial_value_entry};
	} else {
		ci = add_instr(I_FIRST_OFLAG);
		ci->oper[0] = (value_t) {OPER_OFLAG, predname->dyn_id};
		ci->oper[1] = (value_t) {OPER_TEMP, 0};
		ci = add_instr(I_NEXT_OFLAG_PUSH);
		ci->oper[0] = (value_t) {OPER_OFLAG, predname->dyn_id};
		ci->oper[1] = (value_t) {OPER_TEMP, 0};
		ci->oper[2] = (value_t) {OPER_RLAB, lab2};
		ci = add_instr(I_UNIFY);
		ci->oper[0] = (value_t) {OPER_ARG, 0};
		ci->oper[1] = (value_t) {OPER_TEMP, 0};
		ci = add_instr(I_PROCEED);
	}
	end_routine(0xffff, &pred->arena);

	begin_routine(lab1);
	ci = add_instr(I_GET_OFLAG);
	ci->oper[0] = (value_t) {OPER_OFLAG, predname->dyn_id};
	ci->oper[1] = (value_t) {OPER_ARG, 0};
	ci = add_instr(I_PROCEED);
	end_routine(0xffff, &pred->arena);

	begin_routine(lab2);
	ci = add_instr(I_POP_CHOICE);
	ci->oper[0] = (value_t) {OPER_NUM, 2};
	ci = add_instr(I_NEXT_OFLAG_PUSH);
	ci->oper[0] = (value_t) {OPER_OFLAG, predname->dyn_id};
	ci->oper[1] = (value_t) {OPER_ARG, 1};
	ci->oper[2] = (value_t) {OPER_RLAB, lab2};
	ci = add_instr(I_UNIFY);
	ci->oper[0] = (value_t) {OPER_ARG, 0};
	ci->oper[1] = (value_t) {OPER_ARG, 1};
	ci = add_instr(I_PROCEED);
	end_routine(0xffff, &pred->arena);
}

void comp_dyn_var(struct program *prg, struct predname *predname) {
	struct predicate *pred = predname->pred;
	int lab1, lab2;
	struct cinstr *ci;

	assert(predname->dyn_id != DYN_HASPARENT);

	lab1 = make_routine_id();
	lab2 = make_routine_id();

	ci = add_instr(I_IF_BOUND);
	ci->oper[0] = (value_t) {OPER_ARG, 0};
	ci->oper[1] = (value_t) {OPER_RLAB, lab1};
	ci = add_instr(I_ALLOCATE);
	ci->oper[0] = (value_t) {OPER_NUM, 2};
	ci->oper[1] = (value_t) {OPER_NUM, 1};
	ci = add_instr(I_ASSIGN);
	ci->oper[0] = (value_t) {OPER_VAR, 0};
	ci->oper[1] = (value_t) {OPER_ARG, 0};
	ci = add_instr(I_ASSIGN);
	ci->oper[0] = (value_t) {OPER_VAR, 1};
	ci->oper[1] = (value_t) {OPER_ARG, 1};
	ci = add_instr(I_SET_CONT);
	ci->oper[0] = (value_t) {OPER_RLAB, lab2};
	ci = add_instr(I_INVOKE_MULTI);
	ci->oper[0] = (value_t) {OPER_PRED, find_builtin(prg, BI_OBJECT)->pred_id};
	end_routine(0xffff, &pred->arena);

	begin_routine(lab2);
	ci = add_instr(I_GET_OVAR_V);
	ci->oper[0] = (value_t) {OPER_OVAR, predname->dyn_id};
	ci->oper[1] = (value_t) {OPER_VAR, 0};
	ci->oper[2] = (value_t) {OPER_VAR, 1};
	ci = add_instr(I_DEALLOCATE);
	ci = add_instr(I_PROCEED);
	end_routine(0xffff, &pred->arena);

	begin_routine(lab1);
	ci = add_instr(I_GET_OVAR_V);
	ci->oper[0] = (value_t) {OPER_OVAR, predname->dyn_id};
	ci->oper[1] = (value_t) {OPER_ARG, 0};
	ci->oper[2] = (value_t) {OPER_ARG, 1};
	ci = add_instr(I_PROCEED);
	end_routine(0xffff, &pred->arena);
}

void comp_has_parent(struct program *prg, struct predname *predname) {
	struct predicate *pred = predname->pred;
	int lab1, lab2, lab3, lab4;
	struct cinstr *ci;

	assert(predname->dyn_id == DYN_HASPARENT);

	lab1 = make_routine_id();
	lab2 = make_routine_id();
	lab3 = make_routine_id();
	lab4 = make_routine_id();

	ci = add_instr(I_IF_BOUND);
	ci->oper[0] = (value_t) {OPER_ARG, 0};
	ci->oper[1] = (value_t) {OPER_RLAB, lab1};
	ci = add_instr(I_IF_BOUND);
	ci->oper[0] = (value_t) {OPER_ARG, 1};
	ci->oper[1] = (value_t) {OPER_RLAB, lab3};
	ci = add_instr(I_ALLOCATE);
	ci->oper[0] = (value_t) {OPER_NUM, 2};
	ci->oper[1] = (value_t) {OPER_NUM, 1};
	ci = add_instr(I_ASSIGN);
	ci->oper[0] = (value_t) {OPER_VAR, 0};
	ci->oper[1] = (value_t) {OPER_ARG, 0};
	ci = add_instr(I_ASSIGN);
	ci->oper[0] = (value_t) {OPER_VAR, 1};
	ci->oper[1] = (value_t) {OPER_ARG, 1};
	ci = add_instr(I_SET_CONT);
	ci->oper[0] = (value_t) {OPER_RLAB, lab4};
	ci = add_instr(I_INVOKE_MULTI);
	ci->oper[0] = (value_t) {OPER_PRED, find_builtin(prg, BI_OBJECT)->pred_id};
	end_routine(0xffff, &pred->arena);

	begin_routine(lab4);
	ci = add_instr(I_GET_OVAR_V);
	ci->oper[0] = (value_t) {OPER_OVAR, predname->dyn_id};
	ci->oper[1] = (value_t) {OPER_VAR, 0};
	ci->oper[2] = (value_t) {OPER_VAR, 1};
	ci = add_instr(I_DEALLOCATE);
	ci = add_instr(I_PROCEED);
	end_routine(0xffff, &pred->arena);

	begin_routine(lab3);
	ci = add_instr(I_FIRST_CHILD);
	ci->oper[0] = (value_t) {OPER_ARG, 1};
	ci->oper[1] = (value_t) {OPER_TEMP, 0};
	ci = add_instr(I_NEXT_CHILD_PUSH);
	ci->oper[0] = (value_t) {OPER_TEMP, 0};
	ci->oper[1] = (value_t) {OPER_RLAB, lab2};
	ci = add_instr(I_UNIFY);
	ci->oper[0] = (value_t) {OPER_ARG, 0};
	ci->oper[1] = (value_t) {OPER_TEMP, 0};
	ci = add_instr(I_PROCEED);
	end_routine(0xffff, &pred->arena);

	begin_routine(lab1);
	ci = add_instr(I_GET_OVAR_V);
	ci->oper[0] = (value_t) {OPER_OVAR, predname->dyn_id};
	ci->oper[1] = (value_t) {OPER_ARG, 0};
	ci->oper[2] = (value_t) {OPER_ARG, 1};
	ci = add_instr(I_PROCEED);
	end_routine(0xffff, &pred->arena);

	begin_routine(lab2);
	ci = add_instr(I_POP_CHOICE);
	ci->oper[0] = (value_t) {OPER_NUM, 2};
	ci = add_instr(I_NEXT_CHILD_PUSH);
	ci->oper[0] = (value_t) {OPER_ARG, 1};
	ci->oper[1] = (value_t) {OPER_RLAB, lab2};
	ci = add_instr(I_UNIFY);
	ci->oper[0] = (value_t) {OPER_ARG, 0};
	ci->oper[1] = (value_t) {OPER_ARG, 1};
	ci = add_instr(I_PROCEED);
	end_routine(0xffff, &pred->arena);
}

void comp_predicate(struct program *prg, struct predname *predname) {
	struct predicate *pred = predname->pred;
	struct cinstr *ci;
	int i;

	memset(routines, 0, nroutine * sizeof(struct comp_routine));
	nroutine = 0;
	assert(pred->normal_entry == -1);
	pred->normal_entry = make_routine_id();

	begin_routine(pred->normal_entry);
	if(pred->nclause) {
		struct index_entry entries[pred->nclause];

		for(i = 0; i < pred->nclause; i++) {
			pred->clauses[i]->clause_id = i;
			entries[i].key = (value_t) {VAL_NONE, 0};
			entries[i].n_drop_from_arg0 = 0;
			entries[i].n_drop_from_body = 0;
			entries[i].clause_id = i;
		}
		if(pred->flags & PREDF_CONTAINS_JUST) {
			ci = add_instr(I_SAVE_CHOICE);
			ci->oper[0] = (value_t) {OPER_ARG, predname->arity};
		}
		comp_clause_chain(prg, pred, entries, pred->nclause);
	} else {
		ci = add_instr(I_JUMP);
		ci->oper[0] = (value_t) {OPER_FAIL};
		end_routine(0xffff, &pred->arena);
	}

	if(predname->builtin == BI_HASPARENT) {
		assert(pred->initial_value_entry == -1);
		pred->initial_value_entry = pred->normal_entry;
		pred->normal_entry = make_routine_id();
		begin_routine(pred->normal_entry);
		comp_has_parent(prg, predname);
	} else if((pred->flags & PREDF_DYNAMIC) && predname->arity == 2) {
		assert(pred->initial_value_entry == -1);
		pred->initial_value_entry = pred->normal_entry;
		pred->normal_entry = make_routine_id();
		begin_routine(pred->normal_entry);
		comp_dyn_var(prg, predname);
	} else if((pred->flags & PREDF_DYNAMIC) && predname->arity == 1
	&& !(pred->flags & PREDF_GLOBAL_VAR)) {
		assert(pred->initial_value_entry == -1);
		pred->initial_value_entry = pred->normal_entry;
		pred->normal_entry = make_routine_id();
		begin_routine(pred->normal_entry);
		comp_dyn_list(prg, predname);
	}

	pred->routines = arena_alloc(&pred->arena, nroutine * sizeof(struct comp_routine));
	memcpy(pred->routines, routines, nroutine * sizeof(struct comp_routine));
	pred->nroutine = nroutine;

	if(verbose >= 4) {
		comp_dump_predicate(prg, predname);
	}
}

void comp_builtins(struct program *prg) {
	comp_builtin(prg, BI_IS_ONE_OF);
	comp_builtin(prg, BI_SPLIT);
	comp_builtin(prg, BI_REPEAT);
	comp_builtin(prg, BI_GETINPUT);
	comp_builtin(prg, BI_GETRAWINPUT);
	comp_builtin(prg, BI_GETKEY);
	comp_builtin(prg, BI_OBJECT);
	comp_builtin(prg, BI_BREAKPOINT);
	comp_builtin(prg, BI_BREAKPOINT_AGAIN);
	comp_builtin(prg, BI_BREAK_GETKEY);
	comp_builtin(prg, BI_BREAK_FAIL);
	comp_builtin(prg, BI_SAVE);
	comp_builtin(prg, BI_SAVE_UNDO);
	comp_builtin(prg, BI_RESTORE);
	comp_builtin(prg, BI_UNDO);
	comp_builtin(prg, BI_FAIL);
}

void comp_program(struct program *prg) {
	int i;
	struct predname *predname;
	struct predicate *pred;

	for(i = 0; i < prg->npredicate; i++) {
		predname = prg->predicates[i];
		pred = predname->pred;
		if((!predname->builtin || predname->builtin == BI_HASPARENT || (predname->nameflags & PREDNF_DEFINABLE_BI))
		&& !predname->special
		&& !(pred->flags & PREDF_MACRO)) {
			comp_predicate(prg, predname);
		}
	}
}

void comp_init() {
	int i;

	for(i = 0; i < N_OPCODES; i++) {
		opinfo[opinfosrc[i].op].flags = opinfosrc[i].flags;
		opinfo[opinfosrc[i].op].name = opinfosrc[i].name;
	}
}

void comp_cleanup() {
	free(instrbuf);
	free(routines);
	nalloc_instr = 0;
	nalloc_routine = 0;
	instrbuf = 0;
	routines = 0;
	nroutine = 0;
}
