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
#include "output.h"
#include "terminal.h"
#include "report.h"

static volatile int interrupted = 0;

void eval_interrupt() {
	interrupted = 1;
}

static value_t eval_deref(value_t v, struct eval_state *es) {
	int pos;

	while(v.tag == VAL_REF) {
		pos = v.value;
		v = es->heap[pos];
		if(v.tag == VAL_REF && v.value == pos) {
			break;
		}
	}

	return v;
}

static void add_trail(struct eval_state *es, uint16_t index) {
	if(es->trail >= es->nalloc_trail) {
		es->nalloc_trail = 2 * es->nalloc_trail + 8;
		es->trailstack = realloc(es->trailstack, es->nalloc_trail * sizeof(uint16_t));
	}
	es->trailstack[es->trail++] = index;
}

static value_t alloc_heap_pair(struct eval_state *es) {
	value_t v;

	if(es->top + 2 > es->nalloc_heap) {
		int newsize = es->top * 2 + 8;
		if(newsize >= 0xffff) {
			newsize = 0xffff;
			if(es->top + 2 > newsize) {
				report(LVL_ERR, 0, "Heap overflow (perhaps infinite recursion).");
				return (value_t) {VAL_ERROR, 0};
			}
		}
		es->nalloc_heap = newsize;
		es->heap = realloc(es->heap, es->nalloc_heap * sizeof(value_t));
	}

	v = (value_t) {VAL_PAIR, es->top};
	es->top += 2;

	return v;
}

value_t eval_makevar(struct eval_state *es) {
	value_t v;

	if(es->top >= es->nalloc_heap) {
		int newsize = es->top * 2 + 8;
		if(newsize >= 0xffff) {
			newsize = 0xffff;
			if(es->top + 1 > newsize) {
				report(LVL_ERR, 0, "Heap overflow (perhaps infinite recursion).");
				return (value_t) {VAL_ERROR, 0};
			}
		}
		es->nalloc_heap = newsize;
		es->heap = realloc(es->heap, es->nalloc_heap * sizeof(value_t));
	}

	v = (value_t) {VAL_REF, es->top};
	es->heap[es->top++] = v;

	return v;
}

value_t eval_makepair(value_t head, value_t tail, struct eval_state *es) {
	value_t v = alloc_heap_pair(es);

	if(v.tag != VAL_ERROR) {
		es->heap[v.value + 0] = head;
		es->heap[v.value + 1] = tail;
	}

	return v;
}

value_t eval_gethead(value_t v, struct eval_state *es) {
	assert(v.tag == VAL_PAIR);
	return eval_deref(es->heap[v.value + 0], es);
}

value_t eval_gettail(value_t v, struct eval_state *es) {
	assert(v.tag == VAL_PAIR);
	return eval_deref(es->heap[v.value + 1], es);
}

static int envtop(struct eval_state *es) {
	if(es->choice >= 0 && es->choicestack[es->choice].envtop > es->env + 1) {
		return es->choicestack[es->choice].envtop;
	} else {
		return es->env + 1;
	}
}

static void cut_to(struct eval_state *es, int new_choice) {
	struct choice *cho;
	struct env *env;
	int oldtop = envtop(es), newtop;

	assert(new_choice <= es->choice);

	while(es->choice > new_choice) {
		cho = &es->choicestack[es->choice--];
		pred_release(cho->nextcase.pred);
		pred_release(cho->cont.pred);
	}

	newtop = envtop(es);
	while(oldtop > newtop) {
		env = &es->envstack[--oldtop];
		pred_release(env->cont.pred);
		free(env->vars);
		free(env->tracevars);
	}
}

static void revert_env_to(struct eval_state *es, int new_env) {
	struct env *env;
	struct choice *cho = &es->choicestack[es->choice];

	while(es->env > new_env && (es->choice < 0 || es->env >= cho->envtop)) {
		env = &es->envstack[es->env--];
		pred_release(env->cont.pred);
		free(env->vars);
		free(env->tracevars);
	}

	es->env = new_env;
}

static int push_env(struct eval_state *es, int nvar, int ntrace) {
	int top = envtop(es);

	if(top >= es->nalloc_env) {
		int newsize = top * 2 + 8;
		if(newsize >= 0xffff) {
			report(LVL_ERR, 0, "Env stack overflow (perhaps infinite recursion).");
			return 0;
		}
		es->nalloc_env = newsize;
		es->envstack = realloc(es->envstack, es->nalloc_env * sizeof(struct env));
	}

	es->envstack[top].env = es->env;
	es->envstack[top].level = (es->env >= 0)? es->envstack[es->env].level + 1 : 0;
	es->envstack[top].simple = es->simple;
	es->envstack[top].cont = es->cont;
	es->cont.pred = 0;
	es->envstack[top].nvar = nvar;
	es->envstack[top].vars = calloc(nvar, sizeof(value_t));
	es->envstack[top].ntracevar = ntrace;
	es->envstack[top].tracevars = calloc(ntrace, sizeof(value_t));
	es->env = top;

	return 1;
}

static int push_choice(struct eval_state *es, int narg, struct predicate *pred, int routine) {
	struct choice *cho;
	int i;
	int top = envtop(es);

	if(++es->choice >= es->nalloc_choice) {
		int newsize = es->choice * 2 + 8;
		if(newsize >= 0xffff) {
			report(LVL_ERR, 0, "Choice stack overflow (perhaps infinite recursion).");
			return 0;
		}
		es->nalloc_choice = newsize;
		es->choicestack = realloc(es->choicestack, es->nalloc_choice * sizeof(struct choice));
	}

	cho = &es->choicestack[es->choice];
	cho->env = es->env;
	cho->envtop = top;
	cho->trail = es->trail;
	cho->top = es->top;
	cho->simple = es->simple;
	for(i = 0; i < narg; i++) {
		cho->arg[i] = es->arg[i];
	}
	cho->cont = es->cont;
	pred_claim(cho->cont.pred);
	cho->nextcase.pred = pred;
	cho->nextcase.routine = routine;
	pred_claim(pred);

	return 1;
}

static void push_aux(struct eval_state *es, value_t v) {
	if(es->aux >= es->nalloc_aux) {
		es->nalloc_aux = 2 * es->aux + 8;
		es->auxstack = realloc(es->auxstack, es->nalloc_aux * sizeof(value_t));
	}
	es->auxstack[es->aux++] = v;
}

static void collect_push(struct eval_state *es, value_t v) {
	int count;

	// Simple elements are serialised as themselves.
	// Proper lists are serialised as n elements, followed by VAL_PAIR(n).
	// Improper lists are serialised as n elements, followed by the improper tail element, followed by VAL_PAIR(0x8000+n).

	v = eval_deref(v, es);

	switch(v.tag) {
	case VAL_NUM:
	case VAL_OBJ:
	case VAL_DICT:
	case VAL_NIL:
		break;
	case VAL_PAIR:
		count = 0;
		for(;;) {
			collect_push(es, es->heap[v.value + 0]);
			count++;
			v = eval_deref(es->heap[v.value + 1], es);
			if(v.tag == VAL_NIL) {
				v = (value_t) {VAL_PAIR, count};
				break;
			} else if(v.tag != VAL_PAIR) {
				collect_push(es, v);
				v = (value_t) {VAL_PAIR, 0x8000 | count};
				break;
			}
		}
		break;
	case VAL_REF:
		v = (value_t) {VAL_REF, 0};
		break;
	default:
		assert(0); exit(1);
	}

	push_aux(es, v);
}

static value_t collect_pop(struct eval_state *es) {
	value_t v, v1;
	int count;

	assert(es->aux);
	v = es->auxstack[--es->aux];
	if(v.tag == VAL_PAIR) {
		count = v.value;
		if(count & 0x8000) {
			count &= 0x7fff;
			v = collect_pop(es);
			if(v.tag == VAL_ERROR) return v;
		} else {
			v = (value_t) {VAL_NIL, 0};
		}
		while(count--) {
			v1 = collect_pop(es);
			if(v1.tag == VAL_ERROR) return v1;
			v = eval_makepair(v1, v, es);
			if(v.tag == VAL_ERROR) return v;
		}
	} else if(v.tag == VAL_REF) {
		v = eval_makevar(es);
	}

	return v;
}

static void eval_push_undo(struct eval_state *es) {
	struct arena *a;
	struct eval_undo *u;
	int etop;
	int i;

	if(es->nundo >= es->nalloc_undo) {
		es->nalloc_undo = 2 * es->nundo + 8;
		es->undostack = realloc(es->undostack, es->nalloc_undo * sizeof(struct eval_undo));
	}

	u = &es->undostack[es->nundo];
	a = &u->arena;
	arena_init(a, 512);

	etop = envtop(es);
	u->envstack = arena_alloc(a, etop * sizeof(struct env));
	for(i = 0; i < etop; i++) {
		u->envstack[i].vars = malloc(es->envstack[i].nvar * sizeof(value_t));
		memcpy(u->envstack[i].vars, es->envstack[i].vars, es->envstack[i].nvar * sizeof(value_t));
		u->envstack[i].nvar = es->envstack[i].nvar;
		u->envstack[i].tracevars = malloc(es->envstack[i].ntracevar * sizeof(value_t));
		memcpy(u->envstack[i].tracevars, es->envstack[i].tracevars, es->envstack[i].ntracevar * sizeof(value_t));
		pred_claim(es->envstack[i].cont.pred);
		u->envstack[i].cont.pred = es->envstack[i].cont.pred;
		u->envstack[i].cont.routine = es->envstack[i].cont.routine;
		u->envstack[i].ntracevar = es->envstack[i].ntracevar;
		u->envstack[i].env = es->envstack[i].env;
		u->envstack[i].simple = es->envstack[i].simple;
		u->envstack[i].level = es->envstack[i].level;
	}
	u->env = es->env;

	u->choicestack = arena_alloc(a, (es->choice + 1) * sizeof(struct choice));
	memcpy(u->choicestack, es->choicestack, (es->choice + 1) * sizeof(struct choice));
	for(i = 0; i <= es->choice; i++) {
		pred_claim(es->choicestack[i].cont.pred);
		pred_claim(es->choicestack[i].nextcase.pred);
	}
	u->choice = es->choice;
	u->stopchoice = es->stopchoice;

	u->auxstack = arena_alloc(a, es->aux * sizeof(value_t));
	memcpy(u->auxstack, es->auxstack, es->aux * sizeof(value_t));
	u->aux = es->aux;
	u->stopaux = es->stopaux;

	u->trailstack = arena_alloc(a, es->trail * sizeof(uint16_t));
	memcpy(u->trailstack, es->trailstack, es->trail * sizeof(uint16_t));
	u->trail = es->trail;

	u->heap = arena_alloc(a, es->top * sizeof(value_t));
	memcpy(u->heap, es->heap, es->top * sizeof(value_t));
	u->top = es->top;

	pred_claim(es->cont.pred);
	u->cont = es->cont;

	u->select = arena_alloc(a, es->program->nselect);
	memcpy(u->select, es->program->select, es->program->nselect);
	u->nselect = es->program->nselect;

	u->randomseed = es->randomseed;
	u->arg0 = es->arg[0];

	es->nundo++;
}

static int eval_pop_undo(struct eval_state *es) {
	struct eval_undo *u;
	int etop;
	int i;

	if(!es->nundo) return 0;

	u = &es->undostack[--es->nundo];

	es->arg[0] = u->arg0;
	//es->randomseed = u->randomseed;

	assert(u->nselect <= es->program->nselect);
	memcpy(es->program->select, u->select, u->nselect);

	pred_release(es->cont.pred);
	es->cont = u->cont;

	assert(u->top <= es->nalloc_heap);
	memcpy(es->heap, u->heap, u->top * sizeof(value_t));
	es->top = u->top;

	assert(u->trail <= es->nalloc_trail);
	memcpy(es->trailstack, u->trailstack, u->trail * sizeof(uint16_t));
	es->trail = u->trail;

	assert(u->aux <= es->nalloc_aux);
	memcpy(es->auxstack, u->auxstack, u->aux * sizeof(value_t));
	es->aux = u->aux;
	es->stopaux = u->stopaux;

	etop = envtop(es);
	for(i = 0; i < etop; i++) {
		pred_release(es->envstack[i].cont.pred);
		free(es->envstack[i].vars);
		free(es->envstack[i].tracevars);
	}
	for(i = 0; i <= es->choice; i++) {
		pred_release(es->choicestack[i].cont.pred);
		pred_release(es->choicestack[i].nextcase.pred);
	}

	es->choice = u->choice;
	es->stopchoice = u->stopchoice;

	es->env = u->env;
	etop = envtop(es);

	assert(u->choice < es->nalloc_choice);
	assert(etop <= es->nalloc_env);

	memcpy(es->choicestack, u->choicestack, (u->choice + 1) * sizeof(struct choice));
	memcpy(es->envstack, u->envstack, etop * sizeof(struct env));

	arena_free(&u->arena);

	return 1;
}

static value_t value_of(value_t v, struct eval_state *es) {
	struct env *env;

	switch(v.tag) {
	case OPER_ARG:
		assert(v.value < MAXPARAM + 1);
		return es->arg[v.value];
	case OPER_TEMP:
		assert(v.value < es->nalloc_temp);
		return es->temp[v.value];
	case OPER_VAR:
		env = &es->envstack[es->env];
		assert(v.value < env->nvar);
		return env->vars[v.value];
	case VAL_NUM:
	case VAL_OBJ:
	case VAL_DICT:
	case VAL_NIL:
	case VAL_PAIR:
	case VAL_REF:
	case VAL_NONE:
		return v;
	}
	assert(0); exit(1);
}

static void set_by_ref(value_t dest, value_t v, struct eval_state *es) {
	struct env *env;

	switch(dest.tag) {
	case OPER_ARG:
		assert(dest.value < MAXPARAM + 1);
		es->arg[dest.value] = v;
		break;
	case OPER_TEMP:
		if(dest.value >= es->nalloc_temp) {
			es->nalloc_temp = 2 * dest.value + 8;
			es->temp = realloc(es->temp, es->nalloc_temp * sizeof(value_t));
		}
		es->temp[dest.value] = v;
		break;
	case OPER_VAR:
		env = &es->envstack[es->env];
		assert(dest.value < env->nvar);
		env->vars[dest.value] = v;
		break;
	default:
		assert(0);
	}
}

static void set_heap_ref(struct eval_state *es, int ref, value_t v) {
	es->heap[ref] = v;
	add_trail(es, ref);
}

static int unify(struct eval_state *es, value_t v1, value_t v2) {
	v1 = eval_deref(v1, es);
	v2 = eval_deref(v2, es);
	for(;;) {
		if(v1.tag == VAL_REF) {
			if(v2.tag == VAL_REF) {
				if(v1.value > v2.value) {
					add_trail(es, v1.value);
					es->heap[v1.value] = v2;
				} else {
					add_trail(es, v2.value);
					es->heap[v2.value] = v1;
				}
			} else {
				add_trail(es, v1.value);
				es->heap[v1.value] = v2;
			}
			return 1;
		} else if(v2.tag == VAL_REF) {
			add_trail(es, v2.value);
			es->heap[v2.value] = v1;
			return 1;
		} else if(v1.tag == VAL_PAIR) {
			if(v2.tag != VAL_PAIR) return 0;
			if(!unify(es, es->heap[v1.value], es->heap[v2.value])) return 0;
			v1 = eval_deref(es->heap[v1.value + 1], es);
			v2 = eval_deref(es->heap[v2.value + 1], es);
		} else {
			return (v1.tag == v2.tag) && (v1.value == v2.value);
		}
	}
}

void pp_tail(struct eval_state *es, value_t v) {
	v = eval_deref(v, es);
	while(v.tag == VAL_PAIR) {
		o_print_word(" ");
		pp_value(es, es->heap[v.value], 0);
		v = eval_deref(es->heap[v.value + 1], es);
	}
	if(v.tag == VAL_NIL) {
		o_print_word("]");
	} else {
		o_print_word(" | ");
		pp_value(es, v, 0);
		o_print_word("]");
	}
}

void pp_value(struct eval_state *es, value_t v, int with_at) {
	char buf[16];

	v = eval_deref(v, es);
	switch(v.tag) {
	case VAL_NUM:
		snprintf(buf, sizeof(buf), "%d", v.value);
		o_print_word(buf);
		break;
	case VAL_OBJ:
		assert(v.value < es->program->nworldobj);
		o_print_word("#");
		o_print_word(es->program->worldobjnames[v.value]->name);
		break;
	case VAL_DICT:
		if(with_at) {
			o_print_word("@");
		}
		if(v.value < 256) {
			if((v.value >= 32 && v.value < 127) || (v.value > 132)) {
				buf[0] = v.value;
				buf[1] = 0;
			} else {
				snprintf(buf, sizeof(buf), "\\%02x", v.value);
			}
			o_print_word(buf);
		} else {
			assert(v.value - 256 < es->program->ndictword);
			o_print_word(es->program->dictwordnames[v.value - 256]->name);
		}
		break;
	case VAL_NIL:
		o_print_word("[]");
		break;
	case VAL_PAIR:
		o_print_word("[");
		pp_value(es, es->heap[v.value], 0);
		pp_tail(es, es->heap[v.value + 1]);
		break;
	case VAL_REF:
	case VAL_NONE: /* used when tracing negated now-statements */
		o_print_word("$");
		break;
	default:
		assert(0); exit(1);
	}
}

void trace(struct eval_state *es, int kind, struct predname *predname, value_t *args, line_t line) {
	int i, j, level;
	static const char *tracename[] = {
		"ENTER (",
		"QUERY (",
		"QUERY *(",
		"FOUND (",
		"NOW (",
		"NOW ~(",
		"Query succeeded: ("
	};
	char buf[128];

	if(es->trace || kind == TR_REPORT) {
		o_begin_box("trace");
		assert(es->env >= 0);
		if(kind != TR_REPORT) {
			level = es->envstack[es->env].level;
			for(i = 0; i < level; i++) {
				o_print_word("| ");
			}
		}
		o_print_word(tracename[kind]);
		j = 0;
		for(i = 0; i < predname->nword; i++) {
			if(i) o_print_word(" ");
			if(predname->words[i]) {
				o_print_word(predname->words[i]->name);
			} else {
				pp_value(es, args[j++], 1);
			}
		}
		assert(j == predname->arity);
		if(line) {
			snprintf(buf, sizeof(buf), ") %s:%d", FILEPART(line), LINEPART(line));
			o_print_word(buf);
		} else {
			o_print_word(")");
		}
		o_end_box();
	}
}

void ensure_fixed_values(struct eval_state *es, struct program *prg, struct predname *predname) {
	struct eval_state my_es;
	int j;
	value_t eval_arg;

	if(predname->nfixedvalue < prg->nworldobj) {
		predname->fixedvalues = realloc(predname->fixedvalues, prg->nworldobj);
		if(!es) init_evalstate(&my_es, prg);
		for(j = predname->nfixedvalue; j < prg->nworldobj; j++) {
			eval_reinitialize(es? es : &my_es);
			eval_arg = (value_t) {VAL_OBJ, j};
			predname->fixedvalues[j] = !!eval_initial(es? es : &my_es, predname, &eval_arg);
		}
		predname->nfixedvalue = j;
		if(!es) free_evalstate(&my_es);
	}
}

static void do_fail(struct eval_state *es, prgpoint_t *pp) {
	struct choice *cho = &es->choicestack[es->choice];
	struct predicate *pred;

	pred_release(pp->pred);
	if(es->program->eval_ticker) es->program->eval_ticker();
	if(interrupted) {
		pred = find_builtin(es->program, BI_BREAK_FAIL)->pred;
		pred_claim(pred);
		pp->pred = pred;
		pp->routine = pred->normal_entry;
	} else {
		*pp = cho->nextcase;
		cho->nextcase.pred = 0;
	}
}

static int check_dyn_dependency(struct eval_state *es, prgpoint_t pp, struct predname *target) {
	if(es->dyn_callbacks) {
		return 1;
	} else {
		uint16_t cid = pp.pred->routines[pp.routine].clause_id;
		report(
			LVL_ERR,
			(cid == 0xffff)? 0 : pp.pred->clauses[cid]->line,
			"Initial state of dynamic predicate depends on the state of another dynamic predicate, %s.",
			target->printed_name);
		return 0;
	}
}

static int compatible_random(struct eval_state *es, int from, int to) {
	int r;

	es->randomseed = 0x15a4e35L * es->randomseed + 1;
	r = (es->randomseed >> 16) & 0x7fff;

	assert(to >= from);
	r = from + (r % (to - from + 1));
	return r;
}

static int eval_compute(struct eval_state *es, int op, int a, int b, int *res) {
	int32_t r;

	switch(op) {
	case BI_PLUS:
		r = a + b;
		if(r >= 0 && r < 16384) {
			*res = r;
			return 1;
		}
		break;
	case BI_MINUS:
		r = a - b;
		if(r >= 0 && r < 16384) {
			*res = r;
			return 1;
		}
		break;
	case BI_TIMES:
		*res = (a * b) & 16383;
		return 1;
		break;
	case BI_DIVIDED:
		if(b) {
			*res = a / b;
			return 1;
		}
		break;
	case BI_MODULO:
		if(b) {
			*res = a % b;
			return 1;
		}
		break;
	case BI_RANDOM:
		if(b >= a) {
			*res = compatible_random(es, a, b);
			return 1;
		}
		break;
	default:
		printf("unimplemented computation %d\n", op);
		assert(0); exit(1);
	}

	return 0;
}

static int eval_builtin(struct eval_state *es, int builtin, value_t o1, value_t o2) {
	assert(builtin);
	switch(builtin) {
	case BI_BOLD:
		if(!es->forwords) {
			o_set_style(STYLE_BOLD);
		}
		return 1;
	case BI_CLEAR:
		o_clear(0);
		return 1;
	case BI_CLEAR_ALL:
		o_clear(1);
		return 1;
	case BI_COMPILERVERSION:
		if(!es->forwords) {
			o_print_str("Dialog Interactive Debugger (dgdebug) version " VERSION);
		}
		return 1;
	case BI_CURSORTO:
		return 1;
	case BI_EMPTY:
		o1 = eval_deref(o1, es);
		return o1.tag == VAL_NIL;
	case BI_FIXED:
		if(!es->forwords) {
			o_set_style(STYLE_FIXED);
		}
		return 1;
	case BI_HAVE_UNDO:
		return 1;
	case BI_ITALIC:
		if(!es->forwords) {
			o_set_style(STYLE_ITALIC);
		}
		return 1;
	case BI_LINE:
		if(!es->forwords) {
			o_line();
		}
		return 1;
	case BI_LIST:
		o1 = eval_deref(o1, es);
		return o1.tag == VAL_PAIR || o1.tag == VAL_NIL;
	case BI_MEMSTATS:
		return 1;
	case BI_NONEMPTY:
		o1 = eval_deref(o1, es);
		return o1.tag == VAL_PAIR;
	case BI_NOSPACE:
		if(!es->forwords) {
			o_nospace();
		}
		return 1;
	case BI_NUMBER:
		o1 = eval_deref(o1, es);
		return o1.tag == VAL_NUM;
	case BI_OBJECT:
		o1 = eval_deref(o1, es);
		return o1.tag == VAL_OBJ;
	case BI_PAR:
		if(!es->forwords) {
			o_par();
		}
		return 1;
	case BI_PAR_N:
		o1 = eval_deref(o1, es);
		if(!es->forwords && o1.tag == VAL_NUM) {
			o_par_n(o1.value);
		}
		return 1;
	case BI_REVERSE:
		if(!es->forwords) {
			o_set_style(STYLE_REVERSE);
		}
		return 1;
	case BI_ROMAN:
		if(!es->forwords) {
			o_set_style(STYLE_ROMAN);
		}
		return 1;
	case BI_SERIALNUMBER:
		if(!es->forwords) {
			o_print_word("DEBUG");
		}
		return 1;
	case BI_SPACE:
		if(!es->forwords) {
			o_space();
		}
		return 1;
	case BI_SPACE_N:
		o1 = eval_deref(o1, es);
		if(!es->forwords && o1.tag == VAL_NUM) {
			o_space_n(o1.value);
		}
		return 1;
	case BI_SCRIPT_ON:
		return 0;
	case BI_SCRIPT_OFF:
		return 1;
	case BI_TRACE_OFF:
		es->trace = 0;
		return 1;
	case BI_TRACE_ON:
		es->trace = 1;
		return 1;
	case BI_UPPER:
		if(!es->forwords) {
			o_set_upper();
		}
		return 1;
	case BI_WORD:
		o1 = eval_deref(o1, es);
		return o1.tag == VAL_DICT;
	default:
		printf("unimplemented builtin %d\n", builtin); exit(1);
	}
}

static int eval_run(struct eval_state *es) {
	prgpoint_t pp;
	int i, j, n, res;
	int pc;
	struct cinstr *ci;
	struct env *env;
	struct choice *cho;
	struct predname *predname;
	value_t v, v0, v1, v2, args[2];
	uint16_t cid;
	int onum;
	line_t tr_line = 0;
	struct word *w;

	pp = es->resume;
	es->resume.pred = 0;
	pc = 0;

	while(pp.pred) {
		assert(pp.routine < pp.pred->nroutine);
		assert(pc < pp.pred->routines[pp.routine].ninstr);
		ci = &pp.pred->routines[pp.routine].instr[pc];
		pc++;
		//printf("%s %d\n", pp.pred->predname->printed_name, ci->op);
		switch(ci->op) {
		case I_ALLOCATE:
			assert(ci->oper[0].tag == OPER_NUM);
			assert(ci->oper[1].tag == OPER_NUM);
			if(!push_env(es, ci->oper[0].value, ci->oper[1].value)) {
				pred_release(pp.pred);
				return ESTATUS_ERR_HEAP;
			}
			break;
		case I_ASSIGN:
			set_by_ref(ci->oper[0], value_of(ci->oper[1], es), es);
			break;
		case I_BEGIN_STATUS:
			o_begin_box("status");
			break;
		case I_BREAKPOINT:
			if(!ci->subop) report(LVL_NOTE, tr_line, "Query made to (breakpoint)");
			pred_release(pp.pred);
			es->resume = es->cont;
			es->cont.pred = 0;
			return ci->subop? ESTATUS_DEBUGGER : ESTATUS_SUSPENDED;
		case I_BUILTIN:
			assert(ci->oper[2].tag == OPER_PRED);
			if(!eval_builtin(
				es,
				es->program->predicates[ci->oper[2].value]->builtin,
				value_of(ci->oper[0], es),
				value_of(ci->oper[1], es)))
			{
				do_fail(es, &pp);
				pc = 0;
			}
			break;
		case I_CHECK_INDEX:
			if(es->index.tag == ci->oper[0].tag
			&& es->index.value == ci->oper[0].value) {
				if(ci->oper[1].tag == OPER_RLAB) {
					pp.routine = ci->oper[1].value;
					pc = 0;
				} else {
					assert(ci->oper[1].tag == OPER_FAIL);
					do_fail(es, &pp);
					pc = 0;
				}
			}
			break;
		case I_CLRALL_OFLAG:
			assert(ci->oper[0].tag == OPER_OFLAG);
			predname = es->program->objflagpred[ci->oper[0].value];
			if(!check_dyn_dependency(es, pp, predname)) {
				pred_release(pp.pred);
				return ESTATUS_ERR_DYN;
			}
			v = (value_t) {VAL_NONE, 0};
			trace(
				es,
				TR_NOTNOW,
				predname,
				&v,
				tr_line);
			es->dyn_callbacks->clrall_objflag(
				es,
				es->dyn_callback_data,
				ci->oper[0].value);
			break;
		case I_CLRALL_OVAR:
			assert(ci->oper[0].tag == OPER_OVAR);
			predname = es->program->objvarpred[ci->oper[0].value];
			if(!check_dyn_dependency(es, pp, predname)) {
				pred_release(pp.pred);
				return ESTATUS_ERR_DYN;
			}
			args[0] = (value_t) {VAL_NONE, 0};
			args[1] = (value_t) {VAL_NONE, 0};
			trace(
				es,
				TR_NOTNOW,
				predname,
				args,
				tr_line);
			if(!es->dyn_callbacks->clrall_objvar(
				es,
				es->dyn_callback_data,
				ci->oper[0].value))
			{
				pred_release(pp.pred);
				return ESTATUS_ERR_DYN;
			}
			break;
		case I_COLLECT_BEGIN:
			push_aux(es, (value_t) {VAL_NONE});
			break;
		case I_COLLECT_END:
			v = (value_t) {VAL_NIL, 0};
			while((v1 = collect_pop(es)).tag != VAL_NONE) {
				if(v1.tag == VAL_ERROR) {
					pred_release(pp.pred);
					return ESTATUS_ERR_HEAP;
				}
				v = eval_makepair(v1, v, es);
				if(v.tag == VAL_ERROR) {
					pred_release(pp.pred);
					return ESTATUS_ERR_HEAP;
				}
			}
			if(!unify(es, v, value_of(ci->oper[0], es))) {
				do_fail(es, &pp);
				pc = 0;
			}
			break;
		case I_COLLECT_PUSH:
			collect_push(es, value_of(ci->oper[0], es));
			break;
		case I_COMPUTE_R:
			v0 = eval_deref(value_of(ci->oper[0], es), es);
			v1 = eval_deref(value_of(ci->oper[1], es), es);
			if(v0.tag != VAL_NUM
			|| v1.tag != VAL_NUM
			|| !eval_compute(es, ci->subop, v0.value, v1.value, &res)) {
				do_fail(es, &pp);
				pc = 0;
			} else {
				set_by_ref(ci->oper[2], (value_t) {VAL_NUM, res}, es);
			}
			break;
		case I_COMPUTE_V:
			v0 = eval_deref(value_of(ci->oper[0], es), es);
			v1 = eval_deref(value_of(ci->oper[1], es), es);
			v2 = value_of(ci->oper[2], es);
			if(v0.tag != VAL_NUM
			|| v1.tag != VAL_NUM
			|| !eval_compute(es, ci->subop, v0.value, v1.value, &res)
			|| !unify(es, (value_t) {VAL_NUM, res}, v2)) {
				do_fail(es, &pp);
				pc = 0;
			}
			break;
		case I_CUT_CHOICE:
			assert(es->choice);
			cut_to(es, es->choice - 1);
			break;
		case I_DEALLOCATE:
			assert(es->env > 0);
			env = &es->envstack[es->env];
			pred_release(es->cont.pred);
			es->simple = env->simple;
			es->cont = env->cont;
			pred_claim(es->cont.pred);
			revert_env_to(es, env->env);
			break;
		case I_END_STATUS:
			o_end_box();
			break;
		case I_FIRST_CHILD:
			predname = es->program->objvarpred[DYN_HASPARENT];
			if(!check_dyn_dependency(es, pp, predname)) {
				pred_release(pp.pred);
				return ESTATUS_ERR_DYN;
			}
			v = eval_deref(value_of(ci->oper[0], es), es);
			assert(v.tag != VAL_REF);
			if(v.tag != VAL_OBJ
			|| (onum = es->dyn_callbacks->get_first_child(es, es->dyn_callback_data, v.value)) < 0) {
				do_fail(es, &pp);
				pc = 0;
			} else {
				set_by_ref(ci->oper[1], (value_t) {VAL_OBJ, onum}, es);
			}
			break;
		case I_FIRST_OFLAG:
			assert(ci->oper[0].tag == OPER_OFLAG);
			predname = es->program->objflagpred[ci->oper[0].value];
			if(!check_dyn_dependency(es, pp, predname)) {
				pred_release(pp.pred);
				return ESTATUS_ERR_DYN;
			}
			onum = es->dyn_callbacks->get_first_oflag(es, es->dyn_callback_data, ci->oper[0].value);
			if(onum < 0) {
				do_fail(es, &pp);
				pc = 0;
			} else {
				set_by_ref(ci->oper[1], (value_t) {VAL_OBJ, onum}, es);
			}
			break;
		case I_FOR_WORDS:
			if(ci->subop) {
				es->forwords++;
			} else {
				assert(es->forwords);
				es->forwords--;
			}
			break;
		case I_GET_GFLAG:
			assert(ci->oper[0].tag == OPER_GFLAG);
			predname = es->program->globalflagpred[ci->oper[0].value];
			if(!check_dyn_dependency(es, pp, predname)) {
				pred_release(pp.pred);
				return ESTATUS_ERR_DYN;
			}
			if(!es->dyn_callbacks->get_globalflag(es, es->dyn_callback_data, ci->oper[0].value)) {
				do_fail(es, &pp);
				pc = 0;
			}
			break;
		case I_GET_GVAR_R:
			assert(ci->oper[0].tag == OPER_GVAR);
			predname = es->program->globalvarpred[ci->oper[0].value];
			if(!check_dyn_dependency(es, pp, predname)) {
				pred_release(pp.pred);
				return ESTATUS_ERR_DYN;
			}
			v = es->dyn_callbacks->get_globalvar(es, es->dyn_callback_data, ci->oper[0].value);
			if(v.tag == VAL_ERROR) {
				pred_release(pp.pred);
				return ESTATUS_ERR_HEAP;
			} else if(v.tag == VAL_NONE) {
				do_fail(es, &pp);
				pc = 0;
			} else {
				set_by_ref(ci->oper[1], v, es);
			}
			break;
		case I_GET_GVAR_V:
			assert(ci->oper[0].tag == OPER_GVAR);
			predname = es->program->globalvarpred[ci->oper[0].value];
			if(!check_dyn_dependency(es, pp, predname)) {
				pred_release(pp.pred);
				return ESTATUS_ERR_DYN;
			}
			v = es->dyn_callbacks->get_globalvar(es, es->dyn_callback_data, ci->oper[0].value);
			if(v.tag == VAL_ERROR) {
				pred_release(pp.pred);
				return ESTATUS_ERR_DYN;
			}
			if(v.tag == VAL_NONE
			|| !unify(es, v, value_of(ci->oper[1], es))) {
				do_fail(es, &pp);
				pc = 0;
			}
			break;
		case I_GET_INPUT:
			pred_release(pp.pred);
			es->resume = es->cont;
			es->cont.pred = 0;
			return ESTATUS_GET_INPUT;
		case I_GET_KEY:
			pred_release(pp.pred);
			es->resume = es->cont;
			es->cont.pred = 0;
			return ESTATUS_GET_KEY;
		case I_GET_OFLAG:
			assert(ci->oper[0].tag == OPER_OFLAG);
			predname = es->program->objflagpred[ci->oper[0].value];
			v = eval_deref(value_of(ci->oper[1], es), es);
			assert(v.tag != VAL_REF);
			if(predname->pred->flags & PREDF_FIXED_FLAG) {
				ensure_fixed_values(0, es->program, predname);
				assert(predname->nfixedvalue >= es->program->nworldobj);
				assert(predname->fixedvalues);
				if(v.tag != VAL_OBJ
				|| !predname->fixedvalues[v.value]) {
					do_fail(es, &pp);
					pc = 0;
				}
			} else {
				if(!check_dyn_dependency(es, pp, predname)) {
					pred_release(pp.pred);
					return ESTATUS_ERR_DYN;
				}
				if(v.tag != VAL_OBJ
				|| !es->dyn_callbacks->get_objflag(es, es->dyn_callback_data, ci->oper[0].value, v.value)) {
					do_fail(es, &pp);
					pc = 0;
				}
			}
			break;
		case I_GET_OVAR_R:
			assert(ci->oper[0].tag == OPER_OVAR);
			predname = es->program->objvarpred[ci->oper[0].value];
			if(!check_dyn_dependency(es, pp, predname)) {
				pred_release(pp.pred);
				return ESTATUS_ERR_DYN;
			}
			v1 = eval_deref(value_of(ci->oper[1], es), es);
			assert(v1.tag != VAL_REF);
			if(v1.tag != VAL_OBJ) {
				do_fail(es, &pp);
				pc = 0;
			} else {
				v2 = es->dyn_callbacks->get_objvar(
					es,
					es->dyn_callback_data,
					ci->oper[0].value,
					v1.value);
				if(v2.tag == VAL_ERROR) {
					pred_release(pp.pred);
					return ESTATUS_ERR_HEAP;
				} else if(v2.tag == VAL_NONE) {
					do_fail(es, &pp);
					pc = 0;
				} else {
					set_by_ref(ci->oper[2], v2, es);
				}
			}
			break;
		case I_GET_OVAR_V:
			assert(ci->oper[0].tag == OPER_OVAR);
			predname = es->program->objvarpred[ci->oper[0].value];
			if(!check_dyn_dependency(es, pp, predname)) {
				pred_release(pp.pred);
				return ESTATUS_ERR_DYN;
			}
			v1 = eval_deref(value_of(ci->oper[1], es), es);
			assert(v1.tag != VAL_REF);
			if(v1.tag != VAL_OBJ) {
				do_fail(es, &pp);
				pc = 0;
			} else {
				v2 = es->dyn_callbacks->get_objvar(
					es,
					es->dyn_callback_data,
					ci->oper[0].value,
					v1.value);
				if(v2.tag == VAL_ERROR) {
					pred_release(pp.pred);
					return ESTATUS_ERR_HEAP;
				} else if(v2.tag == VAL_NONE
				|| !unify(es, v2, value_of(ci->oper[2], es))) {
					do_fail(es, &pp);
					pc = 0;
				}
			}
			break;
		case I_GET_PAIR_RR:
			v0 = eval_deref(value_of(ci->oper[0], es), es);
			if(v0.tag == VAL_REF) {
				v = alloc_heap_pair(es);
				if(v.tag == VAL_ERROR) {
					pred_release(pp.pred);
					return ESTATUS_ERR_HEAP;
				}
				set_heap_ref(es, v0.value, v);
				es->heap[v.value + 0] = v1 = (value_t) {VAL_REF, v.value + 0};
				set_by_ref(ci->oper[1], v1, es);
				es->heap[v.value + 1] = v2 = (value_t) {VAL_REF, v.value + 1};
				set_by_ref(ci->oper[2], v2, es);
			} else {
				if(v0.tag != VAL_PAIR) {
					do_fail(es, &pp);
					pc = 0;
				} else {
					set_by_ref(ci->oper[1], es->heap[v0.value + 0], es);
					set_by_ref(ci->oper[2], es->heap[v0.value + 1], es);
				}
			}
			break;
		case I_GET_PAIR_RV:
			v0 = eval_deref(value_of(ci->oper[0], es), es);
			if(v0.tag == VAL_REF) {
				v = alloc_heap_pair(es);
				if(v.tag == VAL_ERROR) {
					pred_release(pp.pred);
					return ESTATUS_ERR_HEAP;
				}
				set_heap_ref(es, v0.value, v);
				es->heap[v.value + 0] = v1 = (value_t) {VAL_REF, v.value + 0};
				set_by_ref(ci->oper[1], v1, es);
				es->heap[v.value + 1] = value_of(ci->oper[2], es);
			} else {
				if(v0.tag != VAL_PAIR
				|| !unify(es, es->heap[v0.value + 1], value_of(ci->oper[2], es))) {
					do_fail(es, &pp);
					pc = 0;
				} else {
					set_by_ref(ci->oper[1], es->heap[v0.value + 0], es);
				}
			}
			break;
		case I_GET_PAIR_VR:
			v0 = eval_deref(value_of(ci->oper[0], es), es);
			if(v0.tag == VAL_REF) {
				v = alloc_heap_pair(es);
				if(v.tag == VAL_ERROR) {
					pred_release(pp.pred);
					return ESTATUS_ERR_HEAP;
				}
				set_heap_ref(es, v0.value, v);
				es->heap[v.value + 0] = value_of(ci->oper[1], es);
				es->heap[v.value + 1] = v2 = (value_t) {VAL_REF, v.value + 1};
				set_by_ref(ci->oper[2], v2, es);
			} else {
				if(v0.tag != VAL_PAIR
				|| !unify(es, es->heap[v0.value + 0], value_of(ci->oper[1], es))) {
					do_fail(es, &pp);
					pc = 0;
				} else {
					set_by_ref(ci->oper[2], es->heap[v0.value + 1], es);
				}
			}
			break;
		case I_GET_PAIR_VV:
			v0 = eval_deref(value_of(ci->oper[0], es), es);
			if(v0.tag == VAL_REF) {
				v = alloc_heap_pair(es);
				if(v.tag == VAL_ERROR) {
					pred_release(pp.pred);
					return ESTATUS_ERR_HEAP;
				}
				set_heap_ref(es, v0.value, v);
				es->heap[v.value + 0] = value_of(ci->oper[1], es);
				es->heap[v.value + 1] = value_of(ci->oper[2], es);
			} else {
				if(v0.tag != VAL_PAIR
				|| !unify(es, es->heap[v0.value + 0], value_of(ci->oper[1], es))
				|| !unify(es, es->heap[v0.value + 1], value_of(ci->oper[2], es))) {
					do_fail(es, &pp);
					pc = 0;
				}
			}
			break;
		case I_GET_RAW_INPUT:
			pred_release(pp.pred);
			es->resume = es->cont;
			es->cont.pred = 0;
			return ESTATUS_GET_RAW_INPUT;
		case I_IF_BOUND:
			v = eval_deref(value_of(ci->oper[0], es), es);
			if(v.tag != VAL_REF) {
				if(ci->oper[1].tag == OPER_RLAB) {
					pp.routine = ci->oper[1].value;
					pc = 0;
				} else {
					assert(ci->oper[1].tag == OPER_FAIL);
					do_fail(es, &pp);
					pc = 0;
				}
			}
			break;
#if 0
		case I_IF_BOUND_SIMPLE:
			v = eval_deref(value_of(ci->oper[0], es), es);
			if(v.tag != VAL_REF && es->simple != EVAL_MULTI) {
				if(ci->oper[1].tag == OPER_RLAB) {
					pp.routine = ci->oper[1].value;
					pc = 0;
				} else {
					assert(ci->oper[1].tag == OPER_FAIL);
					do_fail(es, &pp);
					pc = 0;
				}
			}
			break;
#endif
		case I_IF_MATCH:
			v0 = eval_deref(value_of(ci->oper[0], es), es);
			v1 = eval_deref(value_of(ci->oper[1], es), es);
			if(v0.tag == v1.tag
			&& v0.value == v1.value) {
				if(ci->oper[2].tag == OPER_RLAB) {
					pp.routine = ci->oper[2].value;
					pc = 0;
				} else {
					assert(ci->oper[2].tag == OPER_FAIL);
					do_fail(es, &pp);
					pc = 0;
				}
			}
			break;
		case I_IF_NIL:
			v = eval_deref(value_of(ci->oper[0], es), es);
			if(v.tag == VAL_NIL) {
				if(ci->oper[1].tag == OPER_RLAB) {
					pp.routine = ci->oper[1].value;
					pc = 0;
				} else {
					assert(ci->oper[1].tag == OPER_FAIL);
					do_fail(es, &pp);
					pc = 0;
				}
			}
			break;
		case I_INVOKE_MULTI:
			assert(ci->oper[0].tag == OPER_PRED);
			assert(ci->oper[0].value < es->program->npredicate);
			predname = es->program->predicates[ci->oper[0].value];
			es->simple = EVAL_MULTI;
			pred_release(pp.pred);
			pp.pred = predname->pred;
			pred_claim(pp.pred);
			if(!es->dyn_callbacks
			&& pp.pred->initial_value_entry >= 0) {
				pp.routine = pp.pred->initial_value_entry;
			} else {
				pp.routine = pp.pred->normal_entry;
			}
			if(pp.routine == 0xffff && !pp.pred->predname->builtin) {
				do_fail(es, &pp);
			}
			if(es->program->eval_ticker) es->program->eval_ticker();
			if(interrupted) {
				if(!push_env(es, 0, 0)) {
					pred_release(pp.pred);
					return ESTATUS_ERR_HEAP;
				}
				es->resume = pp;
				return ESTATUS_SUSPENDED;
			}
			pc = 0;
			break;
		case I_INVOKE_ONCE:
			assert(ci->oper[0].tag == OPER_PRED);
			assert(ci->oper[0].value < es->program->npredicate);
			predname = es->program->predicates[ci->oper[0].value];
			es->simple = es->choice;
			pred_release(pp.pred);
			pp.pred = predname->pred;
			pred_claim(pp.pred);
			if(!es->dyn_callbacks
			&& pp.pred->initial_value_entry >= 0) {
				pp.routine = pp.pred->initial_value_entry;
			} else {
				pp.routine = pp.pred->normal_entry;
			}
			if(pp.routine == 0xffff && !pp.pred->predname->builtin) {
				do_fail(es, &pp);
			}
			if(es->program->eval_ticker) es->program->eval_ticker();
			if(interrupted) {
				if(!push_env(es, 0, 0)) {
					pred_release(pp.pred);
					return ESTATUS_ERR_HEAP;
				}
				es->resume = pp;
				return ESTATUS_SUSPENDED;
			}
			pc = 0;
			break;
		case I_INVOKE_TAIL_ONCE:
			if(es->simple == EVAL_MULTI) {
				es->simple = es->choice;
			}
			// drop through
		case I_INVOKE_TAIL_MULTI:
			assert(ci->oper[0].tag == OPER_PRED);
			assert(ci->oper[0].value < es->program->npredicate);
			predname = es->program->predicates[ci->oper[0].value];
			pred_release(pp.pred);
			pp.pred = predname->pred;
			pred_claim(pp.pred);
			if(!es->dyn_callbacks
			&& pp.pred->initial_value_entry >= 0) {
				pp.routine = pp.pred->initial_value_entry;
			} else {
				pp.routine = pp.pred->normal_entry;
			}
			if(pp.routine == 0xffff && !pp.pred->predname->builtin) {
				do_fail(es, &pp);
			}
			if(es->program->eval_ticker) es->program->eval_ticker();
			if(interrupted) {
				if(!push_env(es, 0, 0)) {
					pred_release(pp.pred);
					return ESTATUS_ERR_HEAP;
				}
				es->resume = pp;
				return ESTATUS_SUSPENDED;
			}
			pc = 0;
			break;
		case I_JUMP:
			if(ci->oper[0].tag == OPER_RLAB) {
				pp.routine = ci->oper[0].value;
				pc = 0;
			} else {
				assert(ci->oper[0].tag == OPER_FAIL);
				do_fail(es, &pp);
				pc = 0;
			}
			break;
		case I_LESSTHAN:
			v0 = eval_deref(value_of(ci->oper[0], es), es);
			v1 = eval_deref(value_of(ci->oper[1], es), es);
			if(v0.tag != VAL_NUM
			|| v1.tag != VAL_NUM
			|| v0.value >= v1.value) {
				do_fail(es, &pp);
				pc = 0;
			}
			break;
		case I_MAKE_PAIR_RR:
			v = alloc_heap_pair(es);
			if(v.tag == VAL_ERROR) {
				pred_release(pp.pred);
				return ESTATUS_ERR_HEAP;
			}
			es->heap[v.value + 0] = v1 = (value_t) {VAL_REF, v.value + 0};
			set_by_ref(ci->oper[1], v1, es);
			es->heap[v.value + 1] = v1 = (value_t) {VAL_REF, v.value + 1};
			set_by_ref(ci->oper[2], v1, es);
			set_by_ref(ci->oper[0], v, es);
			break;
		case I_MAKE_PAIR_RV:
			v = alloc_heap_pair(es);
			if(v.tag == VAL_ERROR) {
				pred_release(pp.pred);
				return ESTATUS_ERR_HEAP;
			}
			es->heap[v.value + 0] = v1 = (value_t) {VAL_REF, v.value + 0};
			set_by_ref(ci->oper[1], v1, es);
			es->heap[v.value + 1] = value_of(ci->oper[2], es);
			set_by_ref(ci->oper[0], v, es);
			break;
		case I_MAKE_PAIR_VR:
			v = alloc_heap_pair(es);
			if(v.tag == VAL_ERROR) {
				pred_release(pp.pred);
				return ESTATUS_ERR_HEAP;
			}
			es->heap[v.value + 0] = value_of(ci->oper[1], es);
			es->heap[v.value + 1] = v1 = (value_t) {VAL_REF, v.value + 1};
			set_by_ref(ci->oper[2], v1, es);
			set_by_ref(ci->oper[0], v, es);
			break;
		case I_MAKE_PAIR_VV:
			v = alloc_heap_pair(es);
			if(v.tag == VAL_ERROR) {
				pred_release(pp.pred);
				return ESTATUS_ERR_HEAP;
			}
			es->heap[v.value + 0] = value_of(ci->oper[1], es);
			es->heap[v.value + 1] = value_of(ci->oper[2], es);
			set_by_ref(ci->oper[0], v, es);
			break;
		case I_MAKE_VAR:
			v = eval_makevar(es);
			if(v.tag == VAL_ERROR) {
				pred_release(pp.pred);
				return ESTATUS_ERR_HEAP;
			}
			set_by_ref(ci->oper[0], v, es);
			break;
		case I_NEXT_CHILD_PUSH:
			assert(ci->oper[1].tag == OPER_RLAB);
			assert(es->dyn_callbacks);
			v = value_of(ci->oper[0], es); // no need to deref
			assert(v.tag == VAL_OBJ);
			onum = es->dyn_callbacks->get_next_child(es, es->dyn_callback_data, v.value);
			if(onum >= 0) {
				if(!push_choice(es, 2, pp.pred, ci->oper[1].value)) {
					pred_release(pp.pred);
					return ESTATUS_ERR_HEAP;
				}
				es->choicestack[es->choice].arg[1] = (value_t) {VAL_OBJ, onum};
			}
			break;
		case I_NEXT_OBJ_PUSH:
			assert(ci->oper[1].tag == OPER_RLAB);
			v = value_of(ci->oper[0], es); // no need to deref
			assert(v.tag == VAL_OBJ);
			if(v.value < es->program->nworldobj - 1) {
				if(!push_choice(es, 2, pp.pred, ci->oper[1].value)) {
					pred_release(pp.pred);
					return ESTATUS_ERR_HEAP;
				}
				es->choicestack[es->choice].arg[1] = (value_t) {VAL_OBJ, v.value + 1};
			}
			break;
		case I_NEXT_OFLAG_PUSH:
			assert(ci->oper[0].tag == OPER_OFLAG);
			assert(ci->oper[2].tag == OPER_RLAB);
			predname = es->program->objflagpred[ci->oper[0].value];
			if(!check_dyn_dependency(es, pp, predname)) {
				pred_release(pp.pred);
				return ESTATUS_ERR_DYN;
			}
			assert(es->dyn_callbacks);
			v = value_of(ci->oper[1], es); // no need to deref
			assert(v.tag == VAL_OBJ);
			onum = es->dyn_callbacks->get_next_oflag(es, es->dyn_callback_data, ci->oper[0].value, v.value);
			if(onum >= 0) {
				if(!push_choice(es, 2, pp.pred, ci->oper[2].value)) {
					pred_release(pp.pred);
					return ESTATUS_ERR_HEAP;
				}
				es->choicestack[es->choice].arg[1] = (value_t) {VAL_OBJ, onum};
			}
			break;
		case I_POP_CHOICE:
			assert(ci->oper[0].tag == OPER_NUM);
			assert(es->choice > 0);
			cho = &es->choicestack[es->choice];
			assert(!cho->nextcase.pred);
			pred_release(es->cont.pred);
			es->cont = cho->cont;
			cho->cont.pred = 0;
			while(es->trail > cho->trail) {
				i = es->trailstack[--es->trail];
				es->heap[i] = (value_t) {VAL_REF, i};
			}
			es->top = cho->top;
			es->simple = cho->simple;
			for(i = 0; i < ci->oper[0].value; i++) {
				es->arg[i] = cho->arg[i];
			}
			es->choice--;
			revert_env_to(es, cho->env);
			break;
		case I_POP_STOP:
			es->aux = es->stopaux;
			assert(es->aux >= 3);
			v = es->auxstack[--es->aux];
			assert(v.tag == VAL_NUM);
			es->wordcheckaux = v.value;
			v = es->auxstack[--es->aux];
			assert(v.tag == VAL_NUM);
			es->stopaux = v.value;
			v = es->auxstack[--es->aux];
			assert(v.tag == VAL_NUM);
			es->stopchoice = v.value;
			break;
		case I_PREPARE_INDEX:
			es->index = eval_deref(value_of(ci->oper[0], es), es);
			break;
		case I_PRINT_VAL:
			v0 = value_of(ci->oper[0], es);
			if(es->forwords) {
				v0 = eval_deref(v0, es);
				if(v0.tag == VAL_DICT
				|| v0.tag == VAL_NUM) {
					push_aux(es, v0);
				}
			} else {
				pp_value(es, v0, 0);
			}
			break;
		case I_PRINT_WORDS:
			for(i = 0; i < 3; i++) {
				if(ci->oper[i].tag == OPER_WORD) {
					w = es->program->allwords[ci->oper[i].value];
					if(es->forwords) {
						assert(w->flags & WORDF_DICT);
						push_aux(es, (value_t) {VAL_DICT, w->dict_id});
					} else {
						o_print_word(w->name);
					}
				} else break;
			}
			break;
		case I_PROCEED:
			if(es->simple != EVAL_MULTI) {
				cut_to(es, es->simple);
			}
			pred_release(pp.pred);
			if(es->program->eval_ticker) es->program->eval_ticker();
			if(interrupted) {
				es->resume = es->cont;
				es->cont.pred = 0;
				return ESTATUS_SUSPENDED;
			} else {
				pp = es->cont;
				es->cont.pred = 0;
				if(!pp.pred) return ESTATUS_SUCCESS;
				pc = 0;
			}
			break;
		case I_PUSH_CHOICE:
			assert(ci->oper[0].tag == OPER_NUM);
			assert(ci->oper[1].tag == OPER_RLAB);
			if(!push_choice(es, ci->oper[0].value, pp.pred, ci->oper[1].value)) {
				pred_release(pp.pred);
				return ESTATUS_ERR_HEAP;
			}
			break;
		case I_PUSH_STOP:
			assert(ci->oper[0].tag == OPER_RLAB);
			push_aux(es, (value_t) {VAL_NUM, es->stopchoice});
			push_aux(es, (value_t) {VAL_NUM, es->stopaux});
			push_aux(es, (value_t) {VAL_NUM, es->wordcheckaux});
			es->stopaux = es->aux;
			if(!push_choice(es, 0, pp.pred, ci->oper[0].value)) {
				pred_release(pp.pred);
				return ESTATUS_ERR_HEAP;
			}
			es->stopchoice = es->choice;
			break;
		case I_QUIT:
			pred_release(pp.pred);
			return ESTATUS_QUIT;
		case I_RESTART:
			pred_release(pp.pred);
			return ESTATUS_RESTART;
		case I_RESTORE:
			if(es->dyn_callbacks) {
				pred_release(pp.pred);
				es->resume = es->cont;
				es->cont.pred = 0;
				return ESTATUS_RESTORE;
			}
			break;
		case I_RESTORE_CHOICE:
			v = value_of(ci->oper[0], es);
			assert(v.tag == VAL_NUM);
			cut_to(es, v.value);
			break;
		case I_SAVE:
			if(es->dyn_callbacks) {
				pred_release(pp.pred);
				es->resume = es->cont;
				es->cont.pred = 0;
				return ESTATUS_SAVE;
			} else {
				do_fail(es, &pp);
				pc = 0;
			}
			break;
		case I_SAVE_CHOICE:
			set_by_ref(ci->oper[0], (value_t) {VAL_NUM, es->choice}, es);
			break;
		case I_SAVE_UNDO:
			if(es->dyn_callbacks) {
				es->dyn_callbacks->push_undo(es->dyn_callback_data);
				pred_release(pp.pred);
				pp.pred = 0;
				eval_push_undo(es);
				if(!unify(es, es->arg[0], (value_t) {VAL_NUM, 0})) {
					do_fail(es, &pp);
					pc = 0;
				} else {
					pp = es->cont;
					es->cont.pred = 0;
					pc = 0;
					if(!pp.pred) return ESTATUS_SUCCESS;
				}
			} else {
				do_fail(es, &pp);
				pc = 0;
			}
			break;
		case I_SELECT:
			assert(ci->oper[0].tag == OPER_NUM);
			assert(ci->oper[1].tag == OPER_RLAB);
			n = ci->oper[0].value;
			assert(n > 1);
			if(ci->subop == SEL_P_RANDOM) {
				i = compatible_random(es, 0, n - 1);
			} else {
				assert(ci->oper[2].tag == OPER_NUM);
				assert(ci->oper[2].value < es->program->nselect);
				i = es->program->select[ci->oper[2].value];
				switch(ci->subop) {
				case SEL_STOPPING:
					if(i + 1 < n) {
						es->program->select[ci->oper[2].value] = i + 1;
					}
					break;
				case SEL_RANDOM:
					if(i) {
						j = compatible_random(es, 0, n - 2);
						if(j >= i - 1) j++;
						i = j;
					} else {
						i = compatible_random(es, 0, n - 1);
					}
					es->program->select[ci->oper[2].value] = i + 1;
					break;
				case SEL_T_RANDOM:
					if(i < n) {
						if(i < n - 1) {
							es->program->select[ci->oper[2].value] = i + 1;
						} else {
							es->program->select[ci->oper[2].value] = n + n - 1;
						}
					} else {
						j = compatible_random(es, 0, n - 2);
						if(j >= i - n) j++;
						i = j;
						es->program->select[ci->oper[2].value] = n + i;
					}
					break;
				case SEL_T_P_RANDOM:
					if(i < n) {
						es->program->select[ci->oper[2].value] = i + 1;
					} else {
						i = compatible_random(es, 0, n - 1);
					}
					break;
				case SEL_CYCLING:
					if(i < n - 1) {
						es->program->select[ci->oper[2].value] = i + 1;
					} else {
						es->program->select[ci->oper[2].value] = 0;
					}
					break;
				default:
					printf("unimplemented select case %d\n", ci->subop); exit(1);
				}
			}
			assert(i < n);
			pp.routine = ci->oper[1].value + i;
			pc = 0;
			break;
		case I_SET_CONT:
			assert(ci->oper[0].tag == OPER_RLAB);
			assert(!es->cont.pred);
			es->cont.pred = pp.pred;
			pred_claim(es->cont.pred);
			es->cont.routine = ci->oper[0].value;
			break;
		case I_SET_GFLAG:
			assert(ci->oper[0].tag == OPER_GFLAG);
			predname = es->program->globalflagpred[ci->oper[0].value];
			if(!check_dyn_dependency(es, pp, predname)) {
				pred_release(pp.pred);
				return ESTATUS_ERR_DYN;
			}
			trace(
				es,
				ci->subop? TR_NOW : TR_NOTNOW,
				predname,
				0,
				tr_line);
			es->dyn_callbacks->set_globalflag(
				es,
				es->dyn_callback_data,
				ci->oper[0].value,
				ci->subop);
			break;
		case I_SET_GVAR:
			assert(ci->oper[0].tag == OPER_GVAR);
			predname = es->program->globalvarpred[ci->oper[0].value];
			if(!check_dyn_dependency(es, pp, predname)) {
				pred_release(pp.pred);
				return ESTATUS_ERR_DYN;
			}
			v = eval_deref(value_of(ci->oper[1], es), es);
			trace(
				es,
				(v.tag == VAL_NONE)? TR_NOTNOW : TR_NOW,
				predname,
				&v,
				tr_line);
			if(!es->dyn_callbacks->set_globalvar(
				es,
				es->dyn_callback_data,
				ci->oper[0].value,
				v))
			{
				pred_release(pp.pred);
				return ESTATUS_ERR_DYN;
			}
			break;
		case I_SET_OFLAG:
			assert(ci->oper[0].tag == OPER_OFLAG);
			predname = es->program->objflagpred[ci->oper[0].value];
			if(!check_dyn_dependency(es, pp, predname)) {
				pred_release(pp.pred);
				return ESTATUS_ERR_DYN;
			}
			v = eval_deref(value_of(ci->oper[1], es), es);
			trace(
				es,
				ci->subop? TR_NOW : TR_NOTNOW,
				predname,
				&v,
				tr_line);
			if(v.tag != VAL_OBJ) {
				report(
					LVL_ERR,
					tr_line,
					"Attempting to set per-object flag %s for non-object.",
					predname->printed_name);
				pred_release(pp.pred);
				return ESTATUS_ERR_OBJ;
			}
			es->dyn_callbacks->set_objflag(
				es,
				es->dyn_callback_data,
				ci->oper[0].value,
				v.value,
				ci->subop);
			break;
		case I_SET_OVAR:
			assert(ci->oper[0].tag == OPER_OVAR);
			predname = es->program->objvarpred[ci->oper[0].value];
			if(!check_dyn_dependency(es, pp, predname)) {
				pred_release(pp.pred);
				return ESTATUS_ERR_DYN;
			}
			args[0] = v1 = eval_deref(value_of(ci->oper[1], es), es);
			args[1] = v2 = eval_deref(value_of(ci->oper[2], es), es);
			trace(
				es,
				(v2.tag == VAL_NONE)? TR_NOTNOW : TR_NOW,
				predname,
				args,
				tr_line);
			if(v1.tag != VAL_OBJ) {
				report(
					LVL_ERR,
					tr_line,
					"Attempting to set per-object variable %s for non-object.",
					predname->printed_name);
				pred_release(pp.pred);
				return ESTATUS_ERR_OBJ;
			}
			if(!es->dyn_callbacks->set_objvar(
				es,
				es->dyn_callback_data,
				ci->oper[0].value,
				v1.value,
				v2))
			{
				pred_release(pp.pred);
				return ESTATUS_ERR_DYN;
			}
			break;
		case I_SPLIT_LIST:
			v0 = value_of(ci->oper[0], es);
			v1 = eval_deref(value_of(ci->oper[1], es), es);
			v2 = value_of(ci->oper[2], es);
			if(v1.tag == VAL_NIL) {
				if(!unify(es, v0, v2)) {
					do_fail(es, &pp);
					pc = 0;
				}
			} else {
				assert(v1.tag == VAL_PAIR);
				for(;;) {
					v0 = eval_deref(v0, es);
					assert(v0.tag == VAL_PAIR);
					if(v0.value == v1.value) {
						break;
					} else {
						v = alloc_heap_pair(es);
						if(v.tag == VAL_ERROR) {
							pred_release(pp.pred);
							return ESTATUS_ERR_HEAP;
						}
						es->heap[v.value + 0] = es->heap[v0.value + 0];
						es->heap[v.value + 1] = (value_t) {VAL_REF, v.value + 1};
						if(!unify(es, v, v2)) {
							do_fail(es, &pp);
							pc = 0;
						}
						v0 = es->heap[v0.value + 1];
						v2 = es->heap[v.value + 1];
					}
				}
				if(!unify(es, (value_t) {VAL_NIL}, v2)) {
					do_fail(es, &pp);
					pc = 0;
				}
			}
			break;
		case I_STOP:
			cut_to(es, es->stopchoice);
			do_fail(es, &pp);
			pc = 0;
			break;
		case I_TRACEPOINT:
			assert(ci->oper[0].tag == OPER_FILE);
			assert(ci->oper[1].tag == OPER_NUM);
			if(ci->subop == TR_LINE) {
				tr_line = MKLINE(ci->oper[0].value, ci->oper[1].value);
			} else if(ci->subop == TR_QUERY || ci->subop == TR_MQUERY) {
				assert(ci->oper[2].tag == OPER_PRED);
				tr_line = MKLINE(ci->oper[0].value, ci->oper[1].value);
				predname = es->program->predicates[ci->oper[2].value];
				for(i = 0; i < predname->arity; i++) {
					es->envstack[es->env].tracevars[i] = es->arg[i];
				}
				if(predname->builtin != BI_GETINPUT
				&& predname->builtin != BI_GETRAWINPUT) {
					// tracing the input predicates would mess up the prompt
					trace(
						es,
						ci->subop,
						predname,
						es->arg,
						tr_line);
				}
			} else if(ci->subop == TR_ENTER) {
				assert(ci->oper[2].tag == OPER_PRED);
				predname = es->program->predicates[ci->oper[2].value];
				trace(
					es,
					TR_ENTER,
					predname,
					es->envstack[es->envstack[es->env].env].tracevars,
					MKLINE(ci->oper[0].value, ci->oper[1].value));
			} else if(ci->subop == TR_QDONE) {
				assert(ci->oper[2].tag == OPER_PRED);
				predname = es->program->predicates[ci->oper[2].value];
				trace(
					es,
					TR_QDONE,
					predname,
					es->envstack[es->env].tracevars,
					MKLINE(ci->oper[0].value, ci->oper[1].value));
				if(pp.pred->predname->builtin == BI_INJECTED_QUERY) {
					trace(
						es,
						TR_REPORT,
						predname,
						es->envstack[es->env].tracevars,
						0);
				}
			} else {
				assert(0);
			}
			break;
		case I_UNDO:
			pred_release(pp.pred);
			pp.pred = 0;
			if(eval_pop_undo(es)) {
				assert(es->dyn_callbacks);
				es->dyn_callbacks->pop_undo(es, es->dyn_callback_data);
				if(!unify(es, es->arg[0], (value_t) {VAL_NUM, 1})) {
					do_fail(es, &pp);
					pc = 0;
				} else {
					pp = es->cont;
					es->cont.pred = 0;
					pc = 0;
					if(!pp.pred) return ESTATUS_SUCCESS;
				}
			} else {
				do_fail(es, &pp);
				pc = 0;
			}
			break;
		case I_UNIFY:
			if(!unify(es, value_of(ci->oper[0], es), value_of(ci->oper[1], es))) {
				do_fail(es, &pp);
				pc = 0;
			}
			break;
		case I_WIN_WIDTH:
			set_by_ref(ci->oper[0], (value_t) {VAL_NUM, o_get_width()}, es);
			break;
		case I_WORDCHECK_BEGIN:
			push_aux(es, (value_t) {VAL_NUM, es->wordcheckaux});
			es->wordcheckaux = es->aux;
			break;
		case I_WORDCHECK_END:
			n = es->aux - es->wordcheckaux;
			v = eval_deref(value_of(ci->oper[0], es), es);
			while(v.tag == VAL_PAIR) {
				v1 = eval_deref(es->heap[v.value + 0], es);
				for(i = 0; i < n; i++) {
					if(v1.tag == es->auxstack[es->wordcheckaux + i].tag
					&& v1.value == es->auxstack[es->wordcheckaux + i].value) {
						break;
					}
				}
				if(i == n) {
					do_fail(es, &pp);
					pc = 0;
					break;
				}
				v = eval_deref(es->heap[v.value + 1], es);
			}
			es->aux = es->wordcheckaux;
			assert(es->aux);
			v = es->auxstack[--es->aux];
			assert(v.tag == VAL_NUM);
			es->wordcheckaux = v.value;
			break;
		default:
			printf("unimplemented cinstr! %d\n", ci->op);
			cid = pp.pred->routines[pp.routine].clause_id;
			comp_dump_routine(
				es->program,
				(cid == 0xffff)? 0 : pp.pred->clauses[cid],
				&pp.pred->routines[pp.routine]);
			exit(1);
		}
	}

	return ESTATUS_FAILURE;
}

void eval_reinitialize(struct eval_state *es) {
	cut_to(es, -1);
	revert_env_to(es, -1);
	assert(envtop(es) == 0);

	pred_release(es->cont.pred);
	es->cont.pred = 0;
	pred_release(es->resume.pred);
	es->resume.pred = 0;

	es->aux = 0;
	es->wordcheckaux = 0;
	es->trail = 0;
	es->top = 0;
	es->simple = EVAL_MULTI;

	es->forwords = 0;

	(void) push_env(es, 0, 0);
	(void) push_choice(es, 0, 0, 0);
	es->stopchoice = es->choice;
	es->stopaux = es->aux;
}

int eval_initial(struct eval_state *es, struct predname *predname, value_t *args) {
	int i, status;

	assert(!es->dyn_callbacks);

	pred_claim(predname->pred);
	es->resume.pred = predname->pred;
	if(predname->pred->initial_value_entry >= 0) {
		es->resume.routine = predname->pred->initial_value_entry;
	} else {
		es->resume.routine = predname->pred->normal_entry;
	}

	for(i = 0; i < predname->arity; i++) {
		es->arg[i] = args[i];
	}

	es->simple = es->choice;
	trace(es, TR_QUERY, predname, args, 0);

	interrupted = 0;
	status = eval_run(es);

	for(i = 0; i < predname->arity; i++) {
		args[i] = eval_deref(args[i], es);
	}

	return status == ESTATUS_SUCCESS;
}

int eval_program_entry(struct eval_state *es, struct predname *predname, value_t *args) {
	int i;

	assert(es->dyn_callbacks);
	assert(!es->resume.pred);

	pred_claim(predname->pred);
	es->resume.pred = predname->pred;
	es->resume.routine = predname->pred->normal_entry;

	for(i = 0; i < predname->arity; i++) {
		es->arg[i] = args[i];
	}

	es->simple = es->choice;
	trace(es, TR_QUERY, predname, args, 0);
	interrupted = 0;
	return eval_run(es);
}

int eval_resume(struct eval_state *es, value_t arg) {
	if(arg.tag != VAL_NONE && !unify(es, es->arg[0], arg)) {
		do_fail(es, &es->resume);
	}

	interrupted = 0;
	return eval_run(es);
}

int eval_injected_query(struct eval_state *es, struct predname *predname) {
	assert(!es->cont.pred);
	es->cont = es->resume;

	pred_claim(predname->pred);
	es->resume.pred = predname->pred;
	es->resume.routine = predname->pred->normal_entry;

	interrupted = 0;
	return eval_run(es);
}

void init_evalstate(struct eval_state *es, struct program *prg) {
	memset(es, 0, sizeof(*es));
	es->program = prg;
	es->env = -1;
	es->choice = -1;
	es->randomseed = 1;
	eval_reinitialize(es);
}

void free_evalstate(struct eval_state *es) {
	int i, j, etop;
	struct eval_undo *u;

	cut_to(es, -1);
	revert_env_to(es, -1);
	assert(envtop(es) == 0);

	pred_release(es->cont.pred);
	es->cont.pred = 0;
	pred_release(es->resume.pred);
	es->resume.pred = 0;

	for(i = 0; i < es->nundo; i++) {
		u = &es->undostack[i];
		if(u->choice >= 0 && u->choicestack[u->choice].envtop > u->env + 1) {
			etop = u->choicestack[u->choice].envtop;
		} else {
			etop = u->env + 1;
		}
		for(j = 0; j < etop; j++) {
			free(u->envstack[j].vars);
			free(u->envstack[j].tracevars);
			pred_release(u->envstack[j].cont.pred);
		}
		for(j = 0; j <= u->choice; j++) {
			pred_release(u->choicestack[j].cont.pred);
			pred_release(u->choicestack[j].nextcase.pred);
		}
		pred_release(u->cont.pred);
		arena_free(&u->arena);
	}
	free(es->undostack);

	free(es->envstack);
	free(es->choicestack);
	free(es->auxstack);
	free(es->trailstack);
	free(es->heap);
	free(es->temp);
}
