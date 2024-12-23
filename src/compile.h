
enum {
	// F = can fail
	// E = ends the current routine

	I_NOP,			// --
	I_NOP_DEBUG,		// --

	I_ASSIGN,		// -- dest, value

	I_ALLOCATE,		// -- number of variables, number of trace variables, subop = use orig_arg0
	I_DEALLOCATE,		// -- subop = restore args flag
	I_PROCEED,		// -E subop = query type (0 unknown, 1 simple, 2 multi)
	I_SET_CONT,		// -- routine

	I_MAKE_PAIR_VV,		// -- dest, head value, tail value
	I_MAKE_PAIR_VR,		// -- dest, head value, tail arg/temp number
	I_MAKE_PAIR_RV,		// -- dest, head arg/temp number, tail value
	I_MAKE_PAIR_RR,		// -- dest, head arg/temp number, tail arg/temp number

	I_GET_PAIR_VV,		// F- value, head value, tail value
	I_GET_PAIR_VR,		// F- value, head value, tail arg/temp number
	I_GET_PAIR_RV,		// F- value, head arg/temp number, tail value
	I_GET_PAIR_RR,		// F- value, head arg/temp number, tail arg/temp number

	I_MAKE_VAR,		// -- dest

	I_SPLIT_LIST,		// -- input value, end marker, output value

	I_INVOKE_ONCE,		// -E predicate id
	I_INVOKE_MULTI,		// -E predicate id
	I_INVOKE_TAIL_ONCE,	// -E predicate id, subop = parent query type (0 unknown, 1 simple, 2 multi)
	I_INVOKE_TAIL_MULTI,	// -E predicate id

	I_PUSH_CHOICE,		// -- number of args, routine
	I_POP_CHOICE,		// -- number of args
	I_CUT_CHOICE,		// --
	I_SAVE_CHOICE,		// -- dest
	I_RESTORE_CHOICE,	// -- value

	I_SELECT,		// -- number of choices, select id (unless purely random), subop = select type
	I_JUMP,			// -E routine

	I_PRINT_WORDS,		// -- up to three word ids, subop = method (1 = print, 2 = gather words, 3 = either)
	I_PRINT_VAL,		// -- value
	I_FOR_WORDS,		// -- subop = 1 for inc, 0 for dec

	I_UNIFY,		// F- value 1, value 2

	I_COMPUTE_V,		// F- value 1, value 2, dest value, subop = BI_*
	I_COMPUTE_R,		// F- value 1, value 2, dest ref, subop = BI_*
	I_BUILTIN,		// -- optional value 1, optional value 2, predicate id

	I_QUIT,			// -E
	I_RESTART,		// -E
	I_SAVE,			// FE unifies arg0 with comingback value, then proceeds
	I_SAVE_UNDO,		// FE unifies arg0 with comingback value, then proceeds
	I_RESTORE,		// --
	I_UNDO,			// F-
	I_GET_INPUT,		// FE unifies arg0 with input, then proceeds
	I_GET_RAW_INPUT,	// FE unifies arg0 with input, then proceeds
	I_GET_KEY,		// FE unifies arg0 with input, then proceeds

	I_PREPARE_INDEX,	// -- value known to be bound (deref'd and unboxed)
	I_CHECK_INDEX,		// -- simple value, routine
	I_CHECK_WORDMAP,	// -- wordmap number, routine, predicate

	I_COLLECT_BEGIN,	// --
	I_COLLECT_PUSH,		// -- value
	I_COLLECT_END,		// F- dest value (pop all into list, then unify)
	I_COLLECT_CHECK,	// F- value (pop all, all must be simple, fail if value not present)
	I_COLLECT_MATCH_ALL,	// F- input list

	I_IF_BOUND,		// -- value, implicit routine, subop = negate
	I_IF_NIL,		// -- value, implicit routine, subop = negate
	I_IF_NUM,		// -- value, implicit routine, subop = negate
	I_IF_OBJ,		// -- value, implicit routine, subop = negate
	I_IF_PAIR,		// -- value, implicit routine, subop = negate
	I_IF_WORD,		// -- value, implicit routine, subop = negate
	I_IF_MATCH,		// -- value, simple value, implicit routine, subop = negate (derefs and unboxes first, doesn't handle lists)
	I_IF_UNIFY,		// -- value, value, implicit routine, subop = negate (derefs both, doesn't bind variables)
	I_IF_GREATER,		// -- value, value, implicit routine, subop = negate
	I_IF_GFLAG,		// -- global flag number, implicit routine, subop = negate
	I_IF_OFLAG,		// -- object flag number, object, implicit routine, subop = negate
	I_IF_GVAR_EQ,		// -- global var number, simple value, implicit routine, subop = negate
	I_IF_OVAR_EQ,		// -- object var number, object, simple value, implicit routine, subop = negate
	I_IF_HAVE_UNDO,		// -- implicit routine, subop = negate
	I_IF_HAVE_LINK,		// -- implicit routine, subop = negate

	I_GET_GVAR_R,		// F- global var number, dest ref
	I_GET_GVAR_V,		// F- global var number, dest value
	I_GET_OVAR_R,		// F- object var number, object, dest ref
	I_GET_OVAR_V,		// F- object var number, object, dest value
	I_SET_GFLAG,		// -- global flag number, subop = set/clear
	I_SET_GVAR,		// -- global var number, value or none
	I_SET_OFLAG,		// -- object flag number, object, subop = set/clear
	I_SET_OVAR,		// -- object var number, object, value or none
	I_CLRALL_OFLAG,		// -- object flag number
	I_CLRALL_OVAR,		// -- object var number
	I_FIRST_OFLAG,		// F- object flag number, dest ref
	I_NEXT_OFLAG_PUSH,	// -- object flag number, object, routine
	I_FIRST_CHILD,		// F- object, dest ref
	I_NEXT_CHILD_PUSH,	// -- object, routine
	I_NEXT_OBJ_PUSH,	// -- object, routine

	I_PUSH_STOP,		// -- routine id
	I_STOP,			// -E
	I_POP_STOP,		// --

	I_BEGIN_BOX,		// F- word id, subop = status
	I_END_BOX,		// -- word id, subop = status
	I_BEGIN_LINK,		// -- value
	I_END_LINK,		// --

	I_TRANSCRIPT,		// F- subop = enable/disable

	I_BREAKPOINT,		// -E
	I_TRACEPOINT,		// -- file number, line number, predicate id, subop = kind

	N_OPCODES
};

struct opinfo {
	uint8_t		flags;
	char		*name;
};

#define OPF_SUBOP		1
#define OPF_ENDS_ROUTINE	2
#define OPF_BRANCH		4
#define OPF_CAN_FAIL		8

extern struct opinfo opinfo[N_OPCODES];

enum {
	TR_ENTER,
	TR_QUERY,
	TR_MQUERY,
	TR_QDONE,
	TR_NOW,
	TR_NOTNOW,
	TR_DETOBJ,
	TR_REPORT,
	TR_LINE,
	N_TR_KIND,
};

typedef struct value {
	int			tag:8;
	int			value:24;
} value_t;

enum {
	VAL_NONE,
	VAL_NUM,
	VAL_OBJ,
	VAL_DICT,
	VAL_DICTEXT,
	VAL_NIL,
	VAL_PAIR,
	VAL_REF,
	VAL_RAW,		// used when switching on select values and remapped dictionary words
	VAL_ERROR,		// heap overflow etc.
	OPER_ARG,
	OPER_TEMP,
	OPER_VAR,
	OPER_NUM,
	OPER_RLAB,
	OPER_FAIL,		// appears instead of label
	OPER_GFLAG,
	OPER_GVAR,
	OPER_OFLAG,
	OPER_OVAR,
	OPER_PRED,
	OPER_BOX,
	OPER_STR,
	OPER_FILE,
	OPER_WORD
};

struct cinstr {
	uint8_t			op;
	uint8_t			subop;
	uint16_t		implicit;
	value_t			oper[3];
};

struct comp_routine {
	struct cinstr		*instr;
	uint16_t		ninstr;
	uint16_t		clause_id;	// so the debugger can print variable names
	uint16_t		diverted;
	uint16_t		reftrack;	// ffff = unvisited, self = group leader, other = part of group
};

void comp_init();
void comp_dump_routine(struct program *prg, struct clause *cl, struct comp_routine *r);
void comp_dump_predicate(struct program *prg, struct predname *predname);
void comp_predicate(struct program *prg, struct predname *predname);
void comp_builtins(struct program *prg);
void comp_program(struct program *prg);
void comp_cleanup(void);
