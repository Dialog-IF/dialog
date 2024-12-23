#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
//#include <unistd.h>

#include "common.h"
#include "arena.h"
#include "ast.h"
#include "frontend.h"
#include "compile.h"
#include "eval.h"
#include "debugger.h"
#include "output.h"
#include "report.h"
#include "fs.h"
#include "terminal.h"
#include "unicode.h"

#define MAXINPUT 1024

#define DEBUGGERNAME "Dialog Interactive Debugger (dgdebug) version " VERSION

struct debugger {
	int			nfilename;
	char			**filenames;
	struct eval_state	es;
	struct dyn_state	ds;
	struct program		*prg;
	struct timespec		*timestamps;
	int			randomseed;
	int			status;
	char			**pending_input;
	int			pending_wpos;
	int			pending_rpos;
	int			nalloc_pend;
};

static int force_width = 0;

static void set_oflag(struct dyn_state *ds, int onum, int fnum) {
	struct dyn_obj *o = &ds->obj[onum];

	if(!(o->flag[fnum].value & DF_ON)) {
		assert(o->flag[fnum].next == 0xffff);
		assert(o->flag[fnum].prev == 0xffff);
		o->flag[fnum].value = DF_ON;
		if(ds->first_in_oflag[fnum] != 0xffff) {
			ds->obj[ds->first_in_oflag[fnum]].flag[fnum].prev = onum;
			o->flag[fnum].next = ds->first_in_oflag[fnum];
		}
		ds->first_in_oflag[fnum] = onum;
	}
}

static void reset_oflag(struct dyn_state *ds, int onum, int fnum) {
	struct dyn_obj *o = &ds->obj[onum];

	if(o->flag[fnum].value & DF_ON) {
		if(ds->first_in_oflag[fnum] == onum) {
			assert(o->flag[fnum].prev == 0xffff);
			if(o->flag[fnum].next != 0xffff) {
				ds->obj[o->flag[fnum].next].flag[fnum].prev = 0xffff;
			}
			ds->first_in_oflag[fnum] = o->flag[fnum].next;
		} else {
			assert(o->flag[fnum].prev != 0xffff);
			ds->obj[o->flag[fnum].prev].flag[fnum].next = o->flag[fnum].next;
			if(o->flag[fnum].next != 0xffff) {
				ds->obj[o->flag[fnum].next].flag[fnum].prev = o->flag[fnum].prev;
			}
		}
		o->flag[fnum].value &= ~DF_ON;
		o->flag[fnum].next = 0xffff;
		o->flag[fnum].prev = 0xffff;
	}
}

static void update_oflag(struct eval_state *es, struct dyn_state *ds, int onum, int fnum) {
	value_t arg;

	eval_reinitialize(es);
	arg = (value_t) {VAL_OBJ, onum};
	if(eval_initial(es, es->program->objflagpred[fnum], &arg)) {
		set_oflag(ds, onum, fnum);
	} else {
		reset_oflag(ds, onum, fnum);
	}
}

static void remove_child_from(struct dyn_state *ds, uint16_t cnum, uint16_t pnum) {
	uint16_t c;

	c = ds->obj[pnum].child;
	if(c == cnum) {
		ds->obj[pnum].child = ds->obj[cnum].sibling;
	} else {
		while(c != 0xffff) {
			if(ds->obj[c].sibling == cnum) {
				ds->obj[c].sibling = ds->obj[cnum].sibling;
				return;
			}
			c = ds->obj[c].sibling;
		}
		report(LVL_WARN, 0, "The object tree is inconsistent.");
	}
}

static int set_parent(struct eval_state *es, struct dyn_state *ds, uint16_t onum, value_t parent, int append) {
	struct dyn_var *v = &ds->obj[onum].var[DYN_HASPARENT];
	uint8_t seen[es->program->nworldobj];
	uint16_t *ptr;

	if(parent.tag != VAL_OBJ && parent.tag != VAL_NONE) {
		report(
			LVL_ERR,
			0,
			"Attempting to set the parent of #%s to a non-object.",
			es->program->worldobjnames[onum]->name);
		return 0;
	}

	if(!v->nalloc) {
		v->nalloc = 1;
		v->rendered = calloc(1, sizeof(value_t));
	}

	if(v->size) {
		assert(v->rendered[0].tag == VAL_OBJ);
		remove_child_from(ds, onum, v->rendered[0].value);
	}

	if(parent.tag == VAL_OBJ) {
		v->size = 1;
		v->rendered[0] = parent;
		if(append) {
			ptr = &ds->obj[parent.value].child;
			while(*ptr != 0xffff) {
				ptr = &ds->obj[*ptr].sibling;
			}
			*ptr = onum;
			ds->obj[onum].sibling = 0xffff;
		} else {
			ds->obj[onum].sibling = ds->obj[parent.value].child;
			ds->obj[parent.value].child = onum;
		}

		memset(seen, 0, es->program->nworldobj);
		while(ds->obj[onum].var[DYN_HASPARENT].size) {
			if(seen[onum]) {
				report(
					LVL_WARN,
					0,
					"Illegal object tree state! #%s is currently nested in itself.",
					es->program->worldobjnames[onum]->name);
				break;
			}
			seen[onum] = 1;
			assert(ds->obj[onum].var[DYN_HASPARENT].rendered[0].tag == VAL_OBJ);
			onum = ds->obj[onum].var[DYN_HASPARENT].rendered[0].value;
		}
	} else {
		v->size = 0;
	}

	return 1;
}

static int render_complex_value(struct dyn_var *dv, value_t v, struct eval_state *es, struct predname *predname) {
	int count, new_size;

	// Simple elements are serialized as themselves.
	// Proper lists are serialized as the elements, followed by VAL_PAIR(n).
	// Improper lists are serialized as the elements, followed by the improper tail element, followed by VAL_PAIR(0x8000+n).
	// Extended dictionary words are serialized as the optional part, followed by the mandatory part, followed by VAL_DICTEXT(0);

	switch(v.tag) {
	case VAL_NUM:
	case VAL_OBJ:
	case VAL_DICT:
	case VAL_NIL:
		break;
	case VAL_PAIR:
		count = 0;
		for(;;) {
			if(!render_complex_value(dv, eval_gethead(v, es), es, predname)) {
				return 0;
			}
			count++;
			v = eval_gettail(v, es);
			if(v.tag == VAL_NIL) {
				v = (value_t) {VAL_PAIR, count};
				break;
			} else if(v.tag != VAL_PAIR) {
				if(!render_complex_value(dv, v, es, predname)) {
					return 0;
				}
				v = (value_t) {VAL_PAIR, 0x8000 | count};
				break;
			}
		}
		break;
	case VAL_DICTEXT:
		if(!render_complex_value(dv, es->heap[v.value + 1], es, predname)) {
			return 0;
		}
		if(!render_complex_value(dv, es->heap[v.value + 0], es, predname)) {
			return 0;
		}
		v.value = 0;
		break;
	case VAL_REF:
		report(
			LVL_ERR,
			0,
			"Attempting to set %s to an unbound value.",
			predname->printed_name);
		dv->size = 0;
		return 0;
	default:
		assert(0); exit(1);
	}

	if(dv->size >= dv->nalloc) {
		new_size = dv->size * 2 + 1;
		if(new_size > 0x1fff) new_size = 0x1fff;
		dv->nalloc = new_size;
		dv->rendered = realloc(dv->rendered, new_size * sizeof(value_t));
		if(dv->size >= dv->nalloc) {
			report(
				LVL_ERR,
				0,
				"Attempting to set %s to a value that is too large.",
				predname->printed_name);
			dv->size = 0;
			return 0;
		}
	}

	dv->rendered[dv->size++] = v;
	return 1;
}

static value_t rebuild_complex_value(value_t *src, int *pos, struct eval_state *es) {
	value_t v, v1;
	int count;

	switch((v = src[(*pos)--]).tag) {
	case VAL_NUM:
	case VAL_OBJ:
	case VAL_DICT:
	case VAL_NIL:
		return v;
	case VAL_PAIR:
		count = v.value & 0x7fff;
		if(v.value & 0x8000) {
			v = rebuild_complex_value(src, pos, es);
			if(v.tag == VAL_ERROR) return v;
		} else {
			v = (value_t) {VAL_NIL};
		}
		while(count--) {
			v1 = rebuild_complex_value(src, pos, es);
			if(v1.tag == VAL_ERROR) return v1;
			v = eval_makepair(v1, v, es);
			if(v.tag == VAL_ERROR) return v;
		}
		return v;
	case VAL_DICTEXT:
		v = rebuild_complex_value(src, pos, es);
		v1 = rebuild_complex_value(src, pos, es);
		v = eval_makepair(v, v1, es);
		if(v.tag == VAL_ERROR) return v;
		v.tag = VAL_DICTEXT;
		return v;
	default:
		assert(0); exit(1);
	}
}

static int update_ovar(struct eval_state *es, struct dyn_state *ds, int onum, int vnum) {
	value_t args[2];
	struct dyn_var *v = &ds->obj[onum].var[vnum];

	eval_reinitialize(es);
	args[0] = (value_t) {VAL_OBJ, onum};
	args[1] = eval_makevar(es);
	if(eval_initial(es, es->program->objvarpred[vnum], args)) {
		assert(vnum != DYN_HASPARENT);
		v->size = 0;
		return render_complex_value(v, args[1], es, es->program->objvarpred[vnum]);
	} else {
		assert(vnum != DYN_HASPARENT);
		v->size = 0;
		return 1;
	}
}

static int refresh_parents(struct eval_state *es, struct dyn_state *ds) {
	value_t args[2], obj;
	int more, success = 1;
	struct predname *predname = es->program->objvarpred[DYN_HASPARENT];
	int onum;

	for(onum = 0; onum < ds->nobj; onum++) {
		if(!ds->obj[onum].var[DYN_HASPARENT].changed) {
			(void) set_parent(es, ds, onum, (value_t) {VAL_NONE, 0}, 1);
		}
	}

	eval_reinitialize(es);
	args[0] = eval_makevar(es);
	args[1] = eval_makevar(es);
	more = eval_initial_multi(es, predname, args);
	while(more) {
		obj = eval_deref(args[0], es);
		if(obj.tag == VAL_OBJ) {
			onum = obj.value;
			if(!ds->obj[onum].var[DYN_HASPARENT].changed
			&& !ds->obj[onum].var[DYN_HASPARENT].size) {
				success &= set_parent(es, ds, onum, eval_deref(args[1], es), 1);
			}
		} else {
			report(
				LVL_ERR,
				0,
				"Initial parent defined with a non-object as the first parameter.");
			success = 0;
		}
		more = eval_initial_next(es);
	}

	return success;
}

static int grow_dyn_state(struct dyn_state *ds, struct program *prg) {
	struct eval_state es;
	struct dyn_obj *o;
	struct dyn_var *v;
	int onum, fnum, vnum;
	value_t arg;
	int success = 1;
	int need_refresh_parents = 0;

	init_evalstate(&es, prg);

	if(ds->ngflag < prg->nglobalflag) {
		ds->gflag = realloc(ds->gflag, prg->nglobalflag);
		while(ds->ngflag < prg->nglobalflag) {
			ds->gflag[ds->ngflag] = 0;
			eval_reinitialize(&es);
			if(eval_initial(&es, prg->globalflagpred[ds->ngflag], 0)) {
				ds->gflag[ds->ngflag] = DF_ON;
			}
			ds->ngflag++;
		}
	}

	if(ds->ngvar < prg->nglobalvar) {
		ds->gvar = realloc(ds->gvar, prg->nglobalvar * sizeof(struct dyn_var));
		while(ds->ngvar < prg->nglobalvar) {
			v = &ds->gvar[ds->ngvar];
			memset(v, 0, sizeof(*v));
			eval_reinitialize(&es);
			arg = eval_makevar(&es);
			if(eval_initial(&es, prg->globalvarpred[ds->ngvar], &arg)) {
				success &= render_complex_value(
					v,
					arg,
					&es,
					prg->globalvarpred[ds->ngvar]);
			}
			ds->ngvar++;
		}
	}

	if(ds->nobj < prg->nworldobj) {
		ds->obj = realloc(ds->obj, prg->nworldobj * sizeof(struct dyn_obj));
		while(ds->nobj < prg->nworldobj) {
			o = &ds->obj[ds->nobj];
			o->sibling = 0xffff;
			o->child = 0xffff;
			if(ds->nobjflag) {
				o->flag = malloc(ds->nobjflag * sizeof(struct dyn_flag));
				for(fnum = 0; fnum < ds->nobjflag; fnum++) {
					o->flag[fnum].value = 0;
					o->flag[fnum].next = 0xffff;
					o->flag[fnum].prev = 0xffff;
					update_oflag(&es, ds, ds->nobj, fnum);
				}
			} else {
				o->flag = 0;
			}
			if(ds->nobjvar) {
				o->var = calloc(ds->nobjvar, sizeof(struct dyn_var));
				need_refresh_parents = 1;
				for(vnum = 1; vnum < ds->nobjvar; vnum++) {
					success &= update_ovar(&es, ds, ds->nobj, vnum);
				}
			} else {
				o->var = 0;
			}
			ds->nobj++;
		}
	}

	if(ds->nobjflag < prg->nobjflag) {
		ds->first_in_oflag = realloc(ds->first_in_oflag, prg->nobjflag * sizeof(uint16_t));
		for(onum = 0; onum < ds->nobj; onum++) {
			o = &ds->obj[onum];
			o->flag = realloc(o->flag, prg->nobjflag * sizeof(struct dyn_flag));
		}
		while(ds->nobjflag < prg->nobjflag) {
			ds->first_in_oflag[ds->nobjflag] = 0xffff;
			for(onum = 0; onum < ds->nobj; onum++) {
				o = &ds->obj[onum];
				o->flag[ds->nobjflag].value = 0;
				o->flag[ds->nobjflag].next = 0xffff;
				o->flag[ds->nobjflag].prev = 0xffff;
				update_oflag(&es, ds, onum, ds->nobjflag);
			}
			ds->nobjflag++;
		}
	}

	if(ds->nobjvar < prg->nobjvar) {
		for(onum = 0; onum < ds->nobj; onum++) {
			o = &ds->obj[onum];
			o->var = realloc(o->var, prg->nobjvar * sizeof(struct dyn_var));
			for(vnum = ds->nobjvar; vnum < prg->nobjvar; vnum++) {
				v = &o->var[vnum];
				memset(v, 0, sizeof(*v));
			}
		}
		while(ds->nobjvar < prg->nobjvar) {
			if(ds->nobjvar == DYN_HASPARENT) {
				need_refresh_parents = 1;
			} else {
				for(onum = 0; onum < ds->nobj; onum++) {
					success &= update_ovar(&es, ds, onum, ds->nobjvar);
				}
			}
			ds->nobjvar++;
		}
	}

	if(need_refresh_parents) {
		success &= refresh_parents(&es, ds);
	}

	free_evalstate(&es);

	return success;
}

static void maybe_grow_dyn_state(struct dyn_state *ds, struct program *prg) {
	if(ds->ngflag < prg->nglobalflag
	|| ds->ngvar < prg->nglobalvar
	|| ds->nobj < prg->nworldobj
	|| ds->nobjflag < prg->nobjflag
	|| ds->nobjvar < prg->nobjvar) {
		(void) grow_dyn_state(ds, prg);
	}
}

static void dump_obj_tree(struct dyn_state *ds, struct program *prg, uint16_t onum, int tabs) {
	o_line();
	o_space_n(6 * tabs);
	o_print_word("#");
	o_nospace();
	o_print_word(prg->worldobjnames[onum]->name);
	o_line();

	for(onum = ds->obj[onum].child; onum != 0xffff; onum = ds->obj[onum].sibling) {
		dump_obj_tree(ds, prg, onum, tabs + 1);
	}
}

void dump_dyn_state(struct eval_state *orig_es, void *userdata) {
	struct dyn_state *ds = userdata;
	struct program *prg = orig_es->program;
	struct eval_state my_es, *es = &my_es;
	int i, any, pos;
	uint16_t onum;
	static const char *flagstate[] = {"off", "on", "off (changed)", "on (changed)"};
	struct dyn_var *v;
	char buf[256];

	maybe_grow_dyn_state(ds, prg);

	init_evalstate(es, prg);

	o_line();
	o_set_style(STYLE_BOLD);
	o_print_word("GLOBAL FLAGS");
	o_set_style(STYLE_ROMAN);
	o_line();
	o_set_style(STYLE_FIXED);
	for(i = 0; i < prg->nglobalflag; i++) {
		snprintf(buf, sizeof(buf), "        %-40s %s", prg->globalflagpred[i]->printed_name, flagstate[ds->gflag[i]]);
		o_print_word(buf);
		o_line();
	}
	o_set_style(STYLE_ROMAN);

	o_par();
	o_set_style(STYLE_BOLD);
	o_print_word("PER-OBJECT FLAGS");
	o_set_style(STYLE_ROMAN);
	o_line();
	o_set_style(STYLE_FIXED);
	for(i = 0; i < prg->nobjflag; i++) {
		if(!(prg->objflagpred[i]->pred->flags & PREDF_FIXED_FLAG)) {
			snprintf(buf, sizeof(buf), "        %-40s", prg->objflagpred[i]->printed_name);
			o_print_word(buf);
			o_line();
			any = 0;
			for(onum = ds->first_in_oflag[i]; onum != 0xffff; onum = ds->obj[onum].flag[i].next) {
				if(!any) {
					o_print_word("                ");
					any = 1;
				}
				o_print_word("#");
				o_nospace();
				o_print_word(prg->worldobjnames[onum]->name);
			}
			o_line();
		}
	}
	o_set_style(STYLE_ROMAN);

	o_par();
	o_set_style(STYLE_BOLD);
	o_print_word("GLOBAL VARIABLES");
	o_set_style(STYLE_ROMAN);
	o_line();
	o_set_style(STYLE_FIXED);
	for(i = 0; i < prg->nglobalvar; i++) {
		snprintf(buf, sizeof(buf), "        %-40s", prg->globalvarpred[i]->printed_name);
		o_print_word(buf);
		if(ds->gvar[i].size) {
			pos = ds->gvar[i].size - 1;
			pp_value(es, rebuild_complex_value(ds->gvar[i].rendered, &pos, es), 1, 1);
		} else {
			o_print_word("<unset>");
		}
		o_line();
	}
	o_set_style(STYLE_ROMAN);

	o_par();
	o_set_style(STYLE_BOLD);
	o_print_word("PER-OBJECT VARIABLES");
	o_set_style(STYLE_ROMAN);
	o_line();
	o_set_style(STYLE_FIXED);
	for(i = 0; i < prg->nobjvar; i++) {
		snprintf(buf, sizeof(buf), "        %-40s", prg->objvarpred[i]->printed_name);
		o_print_word(buf);
		o_line();
		for(onum = 0; onum < prg->nworldobj; onum++) {
			v = &ds->obj[onum].var[i];
			if(v->size) {
				snprintf(buf, sizeof(buf), "                #%-30s ", prg->worldobjnames[onum]->name);
				o_print_word(buf);
				pos = v->size - 1;
				pp_value(es, rebuild_complex_value(v->rendered, &pos, es), 1, 1);
				o_line();
			}
		}
	}
	o_set_style(STYLE_ROMAN);

	free_evalstate(es);
}

static void dump_tree(struct dyn_state *ds, struct program *prg) {
	int i;

	maybe_grow_dyn_state(ds, prg);

	o_line();
	o_set_style(STYLE_BOLD);
	o_print_word("OBJECT TREE");
	o_set_style(STYLE_ROMAN);
	o_line();

	o_set_style(STYLE_FIXED);
	for(i = 0; i < prg->nworldobj; i++) {
		if(prg->nobjvar && !ds->obj[i].var[DYN_HASPARENT].size) {
			dump_obj_tree(ds, prg, i, 1);
		}
	}
	o_set_style(STYLE_ROMAN);
}

static value_t get_globalvar(struct eval_state *es, void *userdata, int dyn_id) {
	struct dyn_state *ds = userdata;
	struct dyn_var *v;
	int pos;

	maybe_grow_dyn_state(ds, es->program);
	v = &ds->gvar[dyn_id];
	if(v->size) {
		pos = v->size - 1;
		return rebuild_complex_value(v->rendered, &pos, es);
	} else {
		return (value_t) {VAL_NONE};
	}
}

static int set_globalvar(struct eval_state *es, void *userdata, int dyn_id, value_t val) {
	struct dyn_state *ds = userdata;
	struct dyn_var *v;

	maybe_grow_dyn_state(ds, es->program);
	v = &ds->gvar[dyn_id];
	v->changed = 1;
	v->size = 0;
	if(val.tag == VAL_NONE) {
		return 1;
	} else {
		return render_complex_value(
			v,
			val,
			es,
			es->program->globalvarpred[dyn_id]);
	}
}

static int get_globalflag(struct eval_state *es, void *userdata, int dyn_id) {
	struct dyn_state *ds = userdata;

	maybe_grow_dyn_state(ds, es->program);
	assert(dyn_id < ds->ngflag);
	return !!(ds->gflag[dyn_id] & DF_ON);
}

static void set_globalflag(struct eval_state *es, void *userdata, int dyn_id, int val) {
	struct dyn_state *ds = userdata;

	maybe_grow_dyn_state(ds, es->program);
	assert(dyn_id < ds->ngflag);
	ds->gflag[dyn_id] = DF_CHANGED | (val? DF_ON : 0);
}

static int get_objflag(struct eval_state *es, void *userdata, int dyn_id, int onum) {
	struct dyn_state *ds = userdata;

	maybe_grow_dyn_state(ds, es->program);
	assert(onum < ds->nobj);
	assert(dyn_id < ds->nobjflag);

	return !!(ds->obj[onum].flag[dyn_id].value & DF_ON);
}

static void set_objflag(struct eval_state *es, void *userdata, int dyn_id, int onum, int val) {
	struct dyn_state *ds = userdata;

	maybe_grow_dyn_state(ds, es->program);
	if(val) {
		set_oflag(ds, onum, dyn_id);
	} else {
		reset_oflag(ds, onum, dyn_id);
	}
	ds->obj[onum].flag[dyn_id].value |= DF_CHANGED;
}

static value_t get_objvar(struct eval_state *es, void *userdata, int dyn_id, int onum) {
	struct dyn_state *ds = userdata;
	struct dyn_var *v;
	int pos;

	maybe_grow_dyn_state(ds, es->program);
	v = &ds->obj[onum].var[dyn_id];
	if(v->size) {
		pos = v->size - 1;
		return rebuild_complex_value(v->rendered, &pos, es);
	} else {
		return (value_t) {VAL_NONE};
	}
}

static int set_objvar(struct eval_state *es, void *userdata, int dyn_id, int obj_id, value_t val) {
	struct dyn_state *ds = userdata;
	struct dyn_var *v;

	maybe_grow_dyn_state(ds, es->program);
	v = &ds->obj[obj_id].var[dyn_id];
	v->changed = 1;
	if(dyn_id == DYN_HASPARENT) {
		return set_parent(es, ds, obj_id, val, 0);
	} else {
		v->size = 0;
		if(val.tag == VAL_NONE) {
			return 1;
		} else {
			return render_complex_value(
				v,
				val,
				es,
				es->program->objvarpred[dyn_id]);
		}
	}
}

static int get_first_child(struct eval_state *es, void *userdata, int obj_id) {
	struct dyn_state *ds = userdata;

	maybe_grow_dyn_state(ds, es->program);
	if(ds->obj[obj_id].child == 0xffff) {
		return -1;
	} else {
		return ds->obj[obj_id].child;
	}
}

static int get_next_child(struct eval_state *es, void *userdata, int obj_id) {
	struct dyn_state *ds = userdata;

	maybe_grow_dyn_state(ds, es->program);
	if(ds->obj[obj_id].sibling == 0xffff) {
		return -1;
	} else {
		return ds->obj[obj_id].sibling;
	}
}

static int get_first_oflag(struct eval_state *es, void *userdata, int dyn_id) {
	struct dyn_state *ds = userdata;

	maybe_grow_dyn_state(ds, es->program);
	assert(dyn_id < ds->nobjflag);
	if(ds->first_in_oflag[dyn_id] == 0xffff) {
		return -1;
	} else {
		return ds->first_in_oflag[dyn_id];
	}
}

static int get_next_oflag(struct eval_state *es, void *userdata, int dyn_id, int obj_id) {
	struct dyn_state *ds = userdata;

	maybe_grow_dyn_state(ds, es->program);
	assert(dyn_id < ds->nobjflag);
	assert(obj_id < ds->nobj);
	if(ds->obj[obj_id].flag[dyn_id].next == 0xffff) {
		return -1;
	} else {
		return ds->obj[obj_id].flag[dyn_id].next;
	}
}

static void clrall_objflag(struct eval_state *es, void *userdata, int dyn_id) {
	struct dyn_state *ds = userdata;
	uint16_t onum, next;
	struct dyn_flag *f;

	maybe_grow_dyn_state(ds, es->program);
	for(onum = ds->first_in_oflag[dyn_id]; onum != 0xffff; onum = next) {
		f = &ds->obj[onum].flag[dyn_id];
		next = f->next;
		f->next = 0xffff;
		f->prev = 0xffff;
		f->value = DF_CHANGED;
	}
	ds->first_in_oflag[dyn_id] = 0xffff;
}

static int clrall_objvar(struct eval_state *es, void *userdata, int dyn_id) {
	struct dyn_state *ds = userdata;
	uint16_t onum;
	struct dyn_var *v;

	if(dyn_id == DYN_HASPARENT) {
		report(LVL_ERR, 0, "Clearing all parents is disallowed.");
		return 0;
	}

	maybe_grow_dyn_state(ds, es->program);
	for(onum = 0; onum < es->program->nworldobj; onum++) {
		v = &ds->obj[onum].var[dyn_id];
		v->changed = 1;
		v->size = 0;
	}
	return 1;
}

static void update_initial_values(struct program *prg, struct dyn_state *ds) {
	struct eval_state es;
	int i;
	value_t arg;
	struct dyn_var *v;
	int onum;

	maybe_grow_dyn_state(ds, prg);
	init_evalstate(&es, prg);

	for(i = 0; i < ds->ngflag; i++) {
		if(!(ds->gflag[i] & DF_CHANGED)) {
			eval_reinitialize(&es);
			assert(i < prg->nglobalflag);
			if(eval_initial(&es, prg->globalflagpred[i], 0)) {
				ds->gflag[i] = DF_ON;
			} else {
				ds->gflag[i] = 0;
			}
		}
	}

	for(i = 0; i < ds->ngvar; i++) {
		v = &ds->gvar[i];
		assert(i < prg->nglobalvar);
		if(!v->changed) {
			v->size = 0;
			eval_reinitialize(&es);
			arg = eval_makevar(&es);
			if(eval_initial(&es, prg->globalvarpred[i], &arg)) {
				(void) render_complex_value(
					v,
					arg,
					&es,
					prg->globalvarpred[i]);
			}
		}
	}

	for(onum = 0; onum < ds->nobj; onum++) {
		for(i = 0; i < ds->nobjflag; i++) {
			if(!(ds->obj[onum].flag[i].value & DF_CHANGED)) {
				update_oflag(&es, ds, onum, i);
			}
		}
		for(i = 1; i < ds->nobjvar; i++) {
			if(!ds->obj[onum].var[i].changed) {
				(void) update_ovar(&es, ds, onum, i);
			}
		}
	}

	(void) refresh_parents(&es, ds);

	free_evalstate(&es);
}

static void copy_var(struct dyn_var *dest, struct dyn_var *src) {
	memcpy(dest->rendered, src->rendered, src->size * sizeof(value_t));
	dest->size = src->size;
	dest->changed = src->changed;
}

static void push_undo(void *userdata) {
	struct dyn_state *ds = userdata;
	struct dyn_undo *u;
	struct arena *a;
	int i, j;

	if(ds->nundo >= ds->nalloc_undo) {
		arena_free(&ds->undo[0].arena);
		memmove(ds->undo, ds->undo + 1, (ds->nalloc_undo - 1) * sizeof(struct dyn_undo));
		ds->nundo--;
		ds->did_prune_undo = 1;
	}

	u = &ds->undo[ds->nundo++];
	a = &u->arena;
	arena_init(a, 512);

	u->gflag = arena_alloc(a, ds->ngflag);
	memcpy(u->gflag, ds->gflag, ds->ngflag);

	u->gvar = arena_alloc(a, ds->ngvar * sizeof(struct dyn_var));
	for(i = 0; i < ds->ngvar; i++) {
		u->gvar[i].rendered = arena_alloc(a, ds->gvar[i].size * sizeof(value_t));
		u->gvar[i].nalloc = ds->gvar[i].size;
		copy_var(&u->gvar[i], &ds->gvar[i]);
	}

	u->obj = arena_alloc(a, ds->nobj * sizeof(struct dyn_obj));
	for(i = 0; i < ds->nobj; i++) {
		u->obj[i].flag = arena_alloc(a, ds->nobjflag * sizeof(struct dyn_flag));
		memcpy(u->obj[i].flag, ds->obj[i].flag, ds->nobjflag * sizeof(struct dyn_flag));
		u->obj[i].var = arena_alloc(a, ds->nobjvar * sizeof(struct dyn_var));
		for(j = 0; j < ds->nobjvar; j++) {
			u->obj[i].var[j].rendered = arena_alloc(a, ds->obj[i].var[j].size * sizeof(value_t));
			u->obj[i].var[j].nalloc = ds->obj[i].var[j].size;
			copy_var(&u->obj[i].var[j], &ds->obj[i].var[j]);
		}
		u->obj[i].sibling = ds->obj[i].sibling;
		u->obj[i].child = ds->obj[i].child;
	}

	u->first_in_oflag = arena_alloc(a, ds->nobjflag * sizeof(uint16_t));
	memcpy(u->first_in_oflag, ds->first_in_oflag, ds->nobjflag * sizeof(uint16_t));

	u->ngflag = ds->ngflag;
	u->ngvar = ds->ngvar;
	u->nobj = ds->nobj;
	u->nobjflag = ds->nobjflag;
	u->nobjvar = ds->nobjvar;

	// The library reads input, checks for 'undo', pushes a new undo state,
	// then acts on the input.

	// In case of 'undo', it pops the undo state, but then ignores the line
	// of input.

	// Therefore, for maintaining a command transcript, we have to record
	// the position *before* the most recent line of input.

	u->ninput = ds->ninput - 1;
	if(u->ninput < 0) u->ninput = 0;
}

static void pop_undo(struct eval_state *es, void *userdata) {
	struct dyn_state *ds = userdata;
	struct dyn_undo *u;
	int i, j;

	assert(ds->nundo);
	u = &ds->undo[--ds->nundo];

	assert(ds->ngflag >= u->ngflag);
	assert(ds->ngvar >= u->ngvar);
	assert(ds->nobj >= u->nobj);
	assert(ds->nobjflag >= u->nobjflag);
	assert(ds->nobjvar >= u->nobjvar);

	memcpy(ds->gflag, u->gflag, u->ngflag);
	memset(ds->gflag + u->ngflag, 0, ds->ngflag - u->ngflag);

	for(i = 0; i < u->ngvar; i++) {
		assert(ds->gvar[i].nalloc >= u->gvar[i].nalloc);
		copy_var(&ds->gvar[i], &u->gvar[i]);
	}
	for(i = u->ngvar; i < ds->ngvar; i++) {
		ds->gvar[i].size = 0;
	}

	for(i = 0; i < u->nobj; i++) {
		// Copy all values and prev/next pointers.
		memcpy(ds->obj[i].flag, u->obj[i].flag, u->nobjflag * sizeof(struct dyn_flag));
		for(j = u->nobjflag; j < ds->nobjflag; j++) {
			ds->obj[i].flag[j].value = 0;
			ds->obj[i].flag[j].prev = 0xffff;
			ds->obj[i].flag[j].next = 0xffff;
		}
		for(j = 0; j < u->nobjvar; j++) {
			assert(ds->obj[i].var[j].nalloc >= u->obj[i].var[j].nalloc);
			copy_var(&ds->obj[i].var[j], &u->obj[i].var[j]);
		}
		for(j = u->nobjvar; j < ds->nobjvar; j++) {
			assert(j != DYN_HASPARENT);
			ds->obj[i].var[j].changed = 0;
			ds->obj[i].var[j].size = 0;
		}
		ds->obj[i].sibling = u->obj[i].sibling;
		ds->obj[i].child = u->obj[i].child;
	}
	memcpy(ds->first_in_oflag, u->first_in_oflag, u->nobjflag * sizeof(uint16_t));
	memset(ds->first_in_oflag + u->nobjflag, 0xff, (ds->nobjflag - u->nobjflag) * sizeof(uint16_t));
	for(i = u->nobj; i < ds->nobj; i++) {
		for(j = 0; j < ds->nobjflag; j++) {
			ds->obj[i].flag[j].value = 0;
			ds->obj[i].flag[j].prev = 0xffff;
			ds->obj[i].flag[j].next = 0xffff;
		}
		for(j = 0; j < ds->nobjvar; j++) {
			ds->obj[i].var[j].changed = 0;
		}
		ds->obj[i].sibling = 0xffff;
		ds->obj[i].child = 0xffff;
	}

	update_initial_values(es->program, ds);

	while(ds->ninput > u->ninput) {
		free(ds->inputlog[--ds->ninput]);
	}
	arena_free(&u->arena);
}

static int init_dynstate(struct dyn_state *ds, struct program *prg) {
	memset(ds, 0, sizeof(*ds));
	ds->nalloc_undo = EVAL_MAX_UNDO;
	ds->undo = malloc(ds->nalloc_undo * sizeof(struct dyn_undo));
	return grow_dyn_state(ds, prg);
}

static void free_dyn_state(struct dyn_state *ds) {
	int i, j;

	free(ds->gflag);
	for(i = 0; i < ds->ngvar; i++) {
		free(ds->gvar[i].rendered);
	}
	free(ds->gvar);
	for(i = 0; i < ds->nobj; i++) {
		free(ds->obj[i].flag);
		for(j = 0; j < ds->nobjvar; j++) {
			free(ds->obj[i].var[j].rendered);
		}
		free(ds->obj[i].var);
	}
	free(ds->obj);
	free(ds->first_in_oflag);
	for(i = 0; i < ds->nundo; i++) {
		arena_free(&ds->undo[i].arena);
	}
	free(ds->undo);
	for(i = 0; i < ds->ninput; i++) {
		free(ds->inputlog[i]);
	}
	free(ds->inputlog);
}

static void dyn_add_inputlog(struct dyn_state *ds, uint8_t *str) {
	if(ds->ninput >= ds->nalloc_input) {
		ds->nalloc_input = ds->ninput * 2 + 8;
		ds->inputlog = realloc(ds->inputlog, ds->nalloc_input * sizeof(char *));
	}
	ds->inputlog[ds->ninput++] = strdup((char *) str);
}

static struct eval_dyn_cb dyn_callbacks = {
	.get_globalvar = get_globalvar,
	.set_globalvar = set_globalvar,
	.get_globalflag = get_globalflag,
	.set_globalflag = set_globalflag,
	.get_objflag = get_objflag,
	.set_objflag = set_objflag,
	.get_objvar = get_objvar,
	.set_objvar = set_objvar,
	.get_first_child = get_first_child,
	.get_next_child = get_next_child,
	.get_first_oflag = get_first_oflag,
	.get_next_oflag = get_next_oflag,
	.clrall_objflag = clrall_objflag,
	.clrall_objvar = clrall_objvar,
	.dump_state = dump_dyn_state,
	.push_undo = push_undo,
	.pop_undo = pop_undo,
};

static void inject_input_line(struct debugger *dbg, char *line) {
	if(dbg->pending_wpos >= dbg->nalloc_pend) {
		dbg->nalloc_pend = 2 * dbg->pending_wpos + 8;
		dbg->pending_input = realloc(dbg->pending_input, dbg->nalloc_pend * sizeof(uint8_t *));
	}
	dbg->pending_input[dbg->pending_wpos++] = strdup(line);
}

static void cmd_help(struct debugger *dbg);

static void cmd_again(struct debugger *dbg) {
	while(dbg->pending_wpos > dbg->pending_rpos) {
		free(dbg->pending_input[--dbg->pending_wpos]);
	}
	dbg->pending_rpos = dbg->pending_wpos = 0;

	if(!dbg->ds.nundo) {
		report(LVL_ERR, 0, "Nothing more to undo.");
	} else if(dbg->ds.undo[dbg->ds.nundo - 1].ninput == dbg->ds.ninput) {
		report(LVL_ERR, 0, "Cannot determine what line of input to reproduce.");
	} else {
		inject_input_line(dbg, dbg->ds.inputlog[dbg->ds.undo[dbg->ds.nundo - 1].ninput]);
		dbg->status = ESTATUS_UNDO;
	}
}

static void cmd_tree(struct debugger *dbg) {
	dump_tree(&dbg->ds, dbg->prg);
}

static void cmd_dyn(struct debugger *dbg) {
	dump_dyn_state(&dbg->es, &dbg->ds);
}

static void cmd_quit(struct debugger *dbg) {
	o_cleanup();
	term_quit();
}

static void cmd_save(struct debugger *dbg) {
	fs_writefile(dbg->ds.inputlog, dbg->ds.ninput, "input history");
}

static void cmd_replay(struct debugger *dbg) {
	while(dbg->pending_wpos > dbg->pending_rpos) {
		free(dbg->pending_input[--dbg->pending_wpos]);
	}
	dbg->pending_rpos = dbg->pending_wpos = 0;
	free(dbg->pending_input);

	dbg->pending_input = dbg->ds.inputlog;
	dbg->pending_wpos = dbg->nalloc_pend = dbg->ds.ninput;

	dbg->ds.inputlog = 0;
	dbg->ds.ninput = 0;

	dbg->status = ESTATUS_RESTART;
}

static void cmd_restore(struct debugger *dbg) {
	char **lines;
	int nline;

	while(dbg->pending_wpos > dbg->pending_rpos) {
		free(dbg->pending_input[--dbg->pending_wpos]);
	}
	dbg->pending_rpos = dbg->pending_wpos = 0;

	if((lines = fs_readfile(&nline, "input"))) {
		free(dbg->pending_input);
		dbg->pending_input = lines;
		dbg->nalloc_pend = dbg->pending_wpos = nline;
		dbg->status = ESTATUS_RESTART;
	}
}

struct debugcmd {
	char	*name;
	void	(*invoke)(struct debugger *dbg);
	char	*helptext;
} debugcmd[] = {
	{"again",	cmd_again,	"Undo, then re-enter the last line of game input."},
	{"dynamic",	cmd_dyn,	"Show the current state of all dynamic predicates."},
	{"g",		cmd_again,	"Same as @again."},
	{"help",	cmd_help,	"Display this help text."},
	{"quit",	cmd_quit,	"Quit the debugger."},
	{"replay",	cmd_replay,	"Restart, then replay the accumulated game input."},
	{"restore",	cmd_restore,	"Restart and read game input from a file."},
	{"save",	cmd_save,	"Save accumulated game input to a file."},
	{"tree",	cmd_tree,	"Show the current state of the object tree."},
};

#define NDEBUGCMD (sizeof(debugcmd)/sizeof(*debugcmd))

static void cmd_help(struct debugger *dbg) {
	int i, j;

	o_par();
	o_set_style(STYLE_BOLD);
	o_print_str(DEBUGGERNAME);
	o_set_style(STYLE_ROMAN);
	o_line();
	o_print_str("When the program is waiting for a line of input, you may:");
	o_line();
	o_print_str("* Type a line of input.");
	o_line();
	o_print_str("* Type a query, multi-query, or (now)-statement.");
	o_line();
	o_print_str("* Type one of the debugging commands listed below.");
	o_par();
	o_print_str("Some useful built-in predicates are: (trace on), (trace off), (restart), and (undo).");
	o_line();
	o_print_str("The standard library provides, among others: (try $), (enter $), (actions on), and (actions off).");
	o_par();
	o_print_str(term_suspend_hint());
	o_par();
	o_set_style(STYLE_BOLD);
	o_print_str("Debugging commands");
	o_set_style(STYLE_ROMAN);
	o_line();
	o_print_str("These can be abbreviated to any unambiguous prefix, e.g. @rep for @replay:");
	o_par();
	o_set_style(STYLE_FIXED);
	for(i = 0; i < NDEBUGCMD; i++) {
		o_line();
		o_print_word("    @");
		o_print_word(debugcmd[i].name);
		for(j = strlen(debugcmd[i].name); j < 10; j++) {
			o_print_word(" ");
		}
		o_print_str(debugcmd[i].helptext);
	}
	o_set_style(STYLE_ROMAN);
	o_par();
	o_print_str(term_quit_hint());
	o_par();
	o_print_str("For more information, please refer to");
	o_set_style(STYLE_ITALIC);
	o_print_str("The Dialog Manual.");
	o_set_style(STYLE_ROMAN);
	o_par();
}

int check_modification_times(struct debugger *dbg) {
	int i;
	struct stat st;
	int flag = 0;

	for(i = 0; i < dbg->nfilename; i++) {
		if(!stat(dbg->filenames[i], &st)) {
			if(memcmp(&st.st_mtime, &dbg->timestamps[i], sizeof(struct timespec))) {
				flag = 1;
			}
			memcpy(&dbg->timestamps[i], &st.st_mtime, sizeof(struct timespec));
		}
	}

	return flag;
}

static int recompile(struct program *prg, int argc, char **argv) {
	uint8_t termbuf[1];

	o_begin_box("debugger");
	while(!frontend(prg, argc, argv, 0)) {
		o_line();
		o_print_str("Please fix the errors, and then press RETURN to proceed.");
		o_print_str(term_quit_hint());
		o_begin_box("debuginput");
		o_print_word("recompile>");
		o_sync();
		if(!term_getline("recompile> ", termbuf, 1, 0)) {
			return 0;
		}
		o_post_input(1);
		o_end_box();
	}
	o_end_box();

	return 1;
}

static int restart(struct debugger *dbg) {
	struct timeval tv;
	int old_trace = dbg->es.trace;
	int old_hidelinks = dbg->es.hide_links;

	free_dyn_state(&dbg->ds);
	free_evalstate(&dbg->es);
	free_program(dbg->prg);

	dbg->prg = new_program();
	dbg->prg->eval_ticker = term_ticker;
	frontend_add_builtins(dbg->prg);
	if(!recompile(dbg->prg, dbg->nfilename, dbg->filenames)) {
		return 0;
	}
	(void) check_modification_times(dbg);
	init_evalstate(&dbg->es, dbg->prg);
	(void) init_dynstate(&dbg->ds, dbg->prg);
	dbg->es.trace = old_trace;
	dbg->es.hide_links = old_hidelinks;
	dbg->es.dyn_callbacks = &dyn_callbacks;
	dbg->es.dyn_callback_data = &dbg->ds;
	if(dbg->randomseed) {
		dbg->es.randomseed = dbg->randomseed;
	} else if(!gettimeofday(&tv, 0)) {
		dbg->es.randomseed = tv.tv_sec ^ tv.tv_usec;
	}

	o_begin_box("intdebugger");
	o_set_style(STYLE_BOLD);
	o_print_word("Program restarted.");
	o_set_style(STYLE_ROMAN);
	o_end_box();

	return 1;
}

void usage(char *prgname) {
	fprintf(stderr, DEBUGGERNAME ".\n");
	fprintf(stderr, "Copyright 2018-2020 Linus Akesson.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage: %s [options] [source code filename ...]\n", prgname);
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "--version   -V    Display the program version.\n");
	fprintf(stderr, "--help      -h    Display this information.\n");
	fprintf(stderr, "--verbose   -v    Increase verbosity (may be used multiple times).\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "--trace     -t    Enable tracing from the beginning.\n");
	fprintf(stderr, "--no-entry  -n    Don't query '(program entry point)'.\n");
	fprintf(stderr, "--quit      -q    Quit the debugger when the program terminates.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "--width     -w    Specify output width, in characters.\n");
	fprintf(stderr, "--seed      -s    Specify random seed.\n");
	fprintf(stderr, "--no-links  -L    Don't show hyperlinks in the output.\n");
	fprintf(stderr, "--dfquirks  -D    Activate the dumbfrotz-compatible quirks mode.\n");
}

int debugger(int argc, char **argv) {
	struct option longopts[] = {
		{"help", 0, 0, 'h'},
		{"version", 0, 0, 'V'},
		{"verbose", 0, 0, 'v'},
		{"trace", 0, 0, 't'},
		{"no-entry", 0, 0, 'n'},
		{"quit", 0, 0, 'q'},
		{"width", 1, 0, 'w'},
		{"seed", 1, 0, 's'},
		{"no-links", 0, 0, 'L'},
		{"dfquirks", 0, 0, 'D'},
		{0, 0, 0, 0}
	};

	char *prgname = argv[0];
	int opt;
	struct debugger dbg = {0};
	int running = 1;
	uint8_t termbuf[MAXINPUT];
	uint16_t wtermbuf[MAXINPUT];
	value_t tail, v;
	int i, j, success;
	struct predname *predname;
	int initial_trace = 0, no_entry = 0, quitopt = 0;
	struct timeval tv;
	int dfrotz_quirks = 0, hide_links = 0;
	char numbuf[8], chbuf[8];
	struct word *w;
	uint16_t unibuf[2];

	dbg.timestamps = calloc(argc, sizeof(struct timespec));

	do {
		opt = getopt_long(argc, argv, "?hVvtnqw:s:LD", longopts, 0);
		switch(opt) {
			case 0:
			case '?':
			case 'h':
				usage(prgname);
				return 1;
			case 'V':
				fprintf(stderr, DEBUGGERNAME "\n");
				return 0;
			case 'v':
				verbose++;
				break;
			case 'n':
				no_entry = 1;
				break;
			case 't':
				initial_trace = 1;
				break;
			case 'q':
				quitopt = 1;
				break;
			case 'w':
				force_width = strtol(optarg, 0, 10);
				break;
			case 's':
				dbg.randomseed = strtol(optarg, 0, 10);
				break;
			case 'L':
				hide_links = 1;
				break;
			case 'D':
				dfrotz_quirks = 1;
				break;
			default:
				if(opt >= 0) {
					fprintf(stderr, "Unimplemented option '%c'\n", opt);
					return 1;
				}
				break;
		}
	} while(opt >= 0);

	dbg.nfilename = argc - optind;
	dbg.filenames = argv + optind;

	term_init(eval_interrupt);
	o_reset(force_width, dfrotz_quirks);
	comp_init();

	o_begin_box("intdebugger");
	o_set_style(STYLE_BOLD);
	o_print_str(DEBUGGERNAME ".");
	o_set_style(STYLE_ROMAN);
	o_line();
	o_print_str("Type @help at the game prompt for a brief introduction.");
	o_end_box();

	if(dfrotz_quirks) {
		o_sync();
		o_post_input(1);
	}

	if(!dbg.nfilename) {
		report(LVL_NOTE, 0, "No source code filenames given. Queries are limited to the built-in predicates.");
	}

	dbg.prg = new_program();
	dbg.prg->eval_ticker = term_ticker;
	frontend_add_builtins(dbg.prg);
	(void) check_modification_times(&dbg);
	if(!frontend(dbg.prg, dbg.nfilename, dbg.filenames, 0)) {
		free_program(dbg.prg);
		term_cleanup();
		return 1;
	}
	if(!init_dynstate(&dbg.ds, dbg.prg)) {
		free_dyn_state(&dbg.ds);
		free_program(dbg.prg);
		term_cleanup();
		return 1;
	}
	init_evalstate(&dbg.es, dbg.prg);
	dbg.es.trace = initial_trace;
	dbg.es.hide_links = hide_links;
	dbg.es.dyn_callbacks = &dyn_callbacks;
	dbg.es.dyn_callback_data = &dbg.ds;
	if(dbg.randomseed) {
		dbg.es.randomseed = dbg.randomseed;
	} else if(!gettimeofday(&tv, 0)) {
		dbg.es.randomseed = tv.tv_sec ^ tv.tv_usec;
	}

	if(no_entry) {
		dbg.status = ESTATUS_DEBUGGER;
	} else {
		dbg.status = eval_program_entry(&dbg.es, find_builtin(dbg.prg, BI_PROGRAM_ENTRY), 0);
	}
	while(running) {
		switch(dbg.status) {
		case ESTATUS_SUCCESS:
		case ESTATUS_FAILURE:
		case ESTATUS_QUIT:
			if(quitopt) {
				o_sync();
				running = 0;
				break;
			}
			eval_reinitialize(&dbg.es);
			o_begin_box("debugger");
			o_set_style(STYLE_BOLD);
			o_print_word("Program terminated.");
			o_set_style(STYLE_ROMAN);
			o_line();
			o_print_str("You can still enter debugging commands such as @help or @quit,");
			o_print_str("as well as arbitrary queries, including (restart) and (undo).");
			o_line();
			o_print_str(term_quit_hint());
			o_end_box();
			dbg.status = ESTATUS_DEBUGGER;
			break;
		case ESTATUS_SUSPENDED:
			o_begin_box("debugger");
			o_set_style(STYLE_BOLD);
			o_print_word("Program execution suspended.");
			o_set_style(STYLE_ROMAN);
			o_line();
			o_print_str("You can enter debugging commands such as @help, arbitrary");
			o_print_str("queries such as (trace off) or (stop), as well as (now)-statements.");
			o_print_str("To resume execution, enter a blank line.");
			o_end_box();
			dbg.status = ESTATUS_DEBUGGER;
			break;
		case ESTATUS_RESTART:
			o_reset(force_width, dfrotz_quirks);
			if(!restart(&dbg)) {
				running = 0;
				break;
			}
			if(dfrotz_quirks) {
				o_sync();
				o_post_input(1);
			}
			dbg.status = eval_program_entry(&dbg.es, find_builtin(dbg.prg, BI_PROGRAM_ENTRY), 0);
			break;
		case ESTATUS_ERR_HEAP:
		case ESTATUS_ERR_AUX:
		case ESTATUS_ERR_OBJ:
		case ESTATUS_ERR_SIMPLE:
		case ESTATUS_ERR_DYN:
		case ESTATUS_ERR_IO:
			o_begin_box("debugger");
			o_print_str("Restarting program from: (error");
			snprintf(numbuf, sizeof(numbuf), "%d", dbg.status);
			o_print_word(numbuf);
			o_print_str("entry point)");
			o_end_box();
			eval_reinitialize(&dbg.es);
			o_reset(force_width, dfrotz_quirks);
			v = (value_t) {VAL_NUM, dbg.status};
			dbg.status = eval_program_entry(&dbg.es, find_builtin(dbg.prg, BI_ERROR_ENTRY), &v);
			break;
		case ESTATUS_GET_KEY:
			if(dbg.pending_rpos < dbg.pending_wpos) {
				free(dbg.pending_input[dbg.pending_rpos++]);
				if(dbg.pending_rpos == dbg.pending_wpos) {
					dbg.pending_rpos = dbg.pending_wpos = 0;
				}
				o_line();
				dyn_add_inputlog(&dbg.ds, (uint8_t *) "");
				w = find_word(dbg.prg, "\r");
				ensure_dict_word(dbg.prg, w);
				dbg.status = eval_resume(&dbg.es, (value_t) {VAL_DICT, w->dict_id});
			} else {
				o_sync();
				i = term_getkey(0);
				o_post_input(0);
				if(i == '\n') i = '\r';
				if(i == 127 || i == TERM_DELETE) i = 8;
				if(i >= 129 && i <= 132) {
					i -= 129 - 16;
				}
				if(i < 0 || i == 4) {
					o_line();
					running = 0;
				} else if(i == 3) {
					dbg.status = eval_injected_query(&dbg.es, find_builtin(dbg.prg, BI_BREAK_GETKEY));
				} else if(i == 8 || i == 13 || (i >= 16 && i <= 19) || i >= 32) {
					if(i >= 'A' && i <= 'Z') {
						i = i - 'A' + 'a';
					} else if(i >= 0x80) {
						i = unicode_to_lower(i);
					}
					unibuf[0] = i;
					unibuf[1] = 0;
					if(unicode_to_utf8((uint8_t *) chbuf, sizeof(chbuf), unibuf) == 1) {
						w = find_word(dbg.prg, chbuf);
						ensure_dict_word(dbg.prg, w);
						dyn_add_inputlog(&dbg.ds, (uint8_t *) "");
						dbg.status = eval_resume(&dbg.es, (value_t) {VAL_DICT, w->dict_id});
					}
				}
			}
			break;
		case ESTATUS_DEBUGGER:
		case ESTATUS_GET_INPUT:
		case ESTATUS_GET_RAW_INPUT:
			if(dbg.status == ESTATUS_DEBUGGER) {
				o_begin_box("debuginput");
				o_print_word("suspended>");
				o_sync();
				success = term_getline("suspended> ", termbuf, MAXINPUT, 0);
				o_post_input(1);
				o_end_box();
			} else {
				if(dbg.pending_rpos < dbg.pending_wpos) {
					snprintf((char *) termbuf, sizeof(termbuf), "%s", dbg.pending_input[dbg.pending_rpos]);
					free(dbg.pending_input[dbg.pending_rpos++]);
					if(dbg.pending_rpos == dbg.pending_wpos) {
						dbg.pending_rpos = dbg.pending_wpos = 0;
					}
					o_set_style(STYLE_ROMAN);
					o_set_style(STYLE_INPUT);
					o_print_word((char *) termbuf);
					o_set_style(STYLE_ROMAN);
					o_line();
					success = 1;
				} else {
					o_sync();
					success = term_getline("> ", termbuf, MAXINPUT, 0);
					o_post_input(1);
				}
			}
			if(!success) {
				running = 0;
				break;
			}
			if(check_modification_times(&dbg)) {
				o_begin_box("debugger");
				o_print_str("The source code has been modified. Merging changes into the running program.");
				o_end_box();
				if(!recompile(dbg.prg, dbg.nfilename, dbg.filenames)) {
					running = 0;
					break;
				}
				(void) check_modification_times(&dbg);
				update_initial_values(dbg.prg, &dbg.ds);
			}
			if(*termbuf == '@') {
				o_begin_box("debugger");
				j = -1;
				for(i = 0; i < NDEBUGCMD; i++) {
					if(!strncmp((char *) termbuf + 1, debugcmd[i].name, strlen((char *) termbuf + 1))) {
						if(j != -1) {
							j = -2;
							break;
						}
						j = i;
					}
				}
				if(j == -1) {
					o_print_str("Unknown debugging command. Type @help for a list.");
				} else if(j == -2) {
					o_print_str("Ambiguous debugging command. Type @help for a list.");
				} else {
					debugcmd[j].invoke(&dbg);
				}
				o_end_box();
				if(dbg.status == ESTATUS_GET_INPUT) {
					o_print_word(">");
				}
			} else if(dbg.status == ESTATUS_DEBUGGER && !*termbuf) {
				dbg.status = eval_resume(&dbg.es, (value_t) {VAL_NONE, 0});
			} else if(termbuf[0] == '(' || (termbuf[0] == '*' && termbuf[1] == '(')) {
				predname = find_builtin(dbg.prg, BI_INJECTED_QUERY);
				if(dbg.status == ESTATUS_DEBUGGER) {
					dbg.es.arg[0] = (value_t) {VAL_NUM, 0};
					if(frontend_inject_query(dbg.prg, predname, find_builtin(dbg.prg, BI_BREAKPOINT_AGAIN), 0, termbuf)) {
						dbg.status = eval_injected_query(&dbg.es, predname);
					}
				} else {
					if(frontend_inject_query(dbg.prg, predname, find_builtin(dbg.prg, BI_GETINPUT), find_word(dbg.prg, ">"), termbuf)) {
						dbg.status = eval_injected_query(&dbg.es, predname);
					} else {
						o_print_word(">");
					}
				}
			} else if(dbg.status == ESTATUS_GET_INPUT || dbg.status == ESTATUS_GET_RAW_INPUT) {
				utf8_to_unicode(wtermbuf, MAXINPUT, termbuf);
				for(i = 0; wtermbuf[i]; i++) {
					if(wtermbuf[i] >= 'A' && wtermbuf[i] <= 'Z') {
						wtermbuf[i] = wtermbuf[i] - 'A' + 'a';
					} else if(wtermbuf[i] >= 0x80) {
						wtermbuf[i] = unicode_to_lower(wtermbuf[i]);
					}
				}
				unicode_to_utf8(termbuf, MAXINPUT, wtermbuf);
				dyn_add_inputlog(&dbg.ds, termbuf);
				tail = (value_t) {VAL_NIL};
				if(dbg.status == ESTATUS_GET_RAW_INPUT) {
					assert(0); exit(1);
				} else {
					i = strlen((char *) termbuf);
					while(i >= 0) {
						i--;
						if(i < 0 || strchr(STOPCHARS " ", termbuf[i])) {
							if(termbuf[i + 1]) {
								v = parse_input_word(&dbg.es, termbuf + i + 1);
								if(v.tag == VAL_ERROR) break;
								tail = eval_makepair(v, tail, &dbg.es);
								if(tail.tag == VAL_ERROR) break;
							}
							if(i >= 0 && termbuf[i] != ' ') {
								chbuf[0] = termbuf[i];
								chbuf[1] = 0;
								w = find_word(dbg.prg, chbuf);
								ensure_dict_word(dbg.prg, w);
								tail = eval_makepair(
									(value_t) {VAL_DICT, w->dict_id},
									tail,
									&dbg.es);
								if(tail.tag == VAL_ERROR) break;
							}
							if(i >= 0) termbuf[i] = 0;
						}
					}
					if(i >= 0) {
						dbg.status = ESTATUS_ERR_HEAP;
						break;
					}
				}
				dbg.status = eval_resume(&dbg.es, tail);
			} else {
				report(LVL_ERR, 0, "The program is not currently waiting for a line of input.");
			}
			break;
		case ESTATUS_SAVE:
			report(LVL_ERR, 0, "Saving and restoring from within the debugged program is not supported.");
			report(LVL_NOTE, 0, "You may wish to undo, and then use @save instead. See @help.");
			dbg.status = eval_injected_query(&dbg.es, find_builtin(dbg.prg, BI_FAIL));
			break;
		case ESTATUS_RESTORE:
			report(LVL_ERR, 0, "Saving and restoring from within the debugged program is not supported.");
			report(LVL_NOTE, 0, "You may wish to use @restore instead. See @help.");
			dbg.status = eval_resume(&dbg.es, (value_t) {VAL_NONE, 0});
			break;
		case ESTATUS_RESUME:
			dbg.status = eval_resume(&dbg.es, (value_t) {VAL_NONE, 0});
			break;
		case ESTATUS_UNDO:
			dbg.status = eval_injected_query(&dbg.es, find_builtin(dbg.prg, BI_UNDO));
			break;
		default:
			assert(0); exit(1);
			break;
		}
	}

	if(dfrotz_quirks) {
		o_par();
	}
	o_sync();

	while(dbg.pending_wpos > dbg.pending_rpos) {
		free(dbg.pending_input[--dbg.pending_wpos]);
	}
	free(dbg.pending_input);
	free_dyn_state(&dbg.ds);
	free_evalstate(&dbg.es);
	free_program(dbg.prg);
	free(dbg.timestamps);
	o_cleanup();
	term_cleanup();
	return 0;
}
