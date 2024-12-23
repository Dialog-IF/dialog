
enum {
	// F = can fail, always to <FAIL>
	// R = can fail by branching to a routine, possibly <FAIL>
	// E = ends the current routine

	I_ASSIGN,		// -- dest, value

	I_ALLOCATE,		// -- number of variables, number of trace variables
	I_DEALLOCATE,		// -- subop = restore args flag
	I_PROCEED,		// -E
	I_SET_CONT,		// -- routine id

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
	I_INVOKE_TAIL_ONCE,	// -E predicate id
	I_INVOKE_TAIL_MULTI,	// -E predicate id

	I_PUSH_CHOICE,		// -- number of args, routine id
	I_POP_CHOICE,		// -- number of args
	I_CUT_CHOICE,		// --
	I_SAVE_CHOICE,		// -- dest
	I_RESTORE_CHOICE,	// -- value

	I_SELECT,		// -E number of labels, first label, optional select id, subop = select type
	I_JUMP,			// -E routine id

	I_PRINT_WORDS,		// -- up to three word ids
	I_PRINT_VAL,		// -- value
	I_FOR_WORDS,		// -- subop = 1 for inc, 0 for dec

	I_UNIFY,		// F- value 1, value 2

	I_LESSTHAN,		// R- value 1, value 2
	I_COMPUTE_V,		// F- value 1, value 2, dest value, subop = BI_*
	I_COMPUTE_R,		// R- value 1, value 2, dest ref, subop = BI_*
	I_BUILTIN,		// R- optional value 1, optional value 2, predicate id

	I_QUIT,			// -E
	I_RESTART,		// -E
	I_SAVE,			// FE
	I_SAVE_UNDO,		// FE
	I_RESTORE,		// --
	I_UNDO,			// F-
	I_GET_INPUT,		// FE
	I_GET_RAW_INPUT,	// FE
	I_GET_KEY,		// FE

	I_PREPARE_INDEX,	// -- value known to be bound
	I_CHECK_INDEX,		// -- simple value, routine id

	I_COLLECT_BEGIN,	// --
	I_COLLECT_PUSH,		// -- value
	I_COLLECT_END,		// F- dest value
	I_WORDCHECK_BEGIN,	// --
	I_WORDCHECK_END,	// F- dest value

	I_IF_BOUND,		// -- value, routine
	I_IF_NIL,		// -- value, routine
	I_IF_MATCH,		// -- value, value, routine (must be simple values)

	I_GET_GFLAG,		// R- global flag number
	I_GET_GVAR_R,		// R- global var number, dest ref
	I_GET_GVAR_V,		// F- global var number, dest value
	I_GET_OFLAG,		// R- object flag number, object
	I_GET_OVAR_R,		// R- object var number, object, dest ref
	I_GET_OVAR_V,		// F- object var number, object, dest value
	I_SET_GFLAG,		// -- global flag number, subop = set/clear
	I_SET_GVAR,		// -- global var number, value or none
	I_SET_OFLAG,		// -- object flag number, object, subop = set/clear
	I_SET_OVAR,		// -- object var number, object, value or none
	I_CLRALL_OFLAG,		// -- object flag number
	I_CLRALL_OVAR,		// -- object var number
	I_FIRST_OFLAG,		// R- object flag number, dest ref
	I_NEXT_OFLAG_PUSH,	// -- object flag number, object, routine
	I_FIRST_CHILD,		// R- object, dest ref
	I_NEXT_CHILD_PUSH,	// -- object, routine
	I_NEXT_OBJ_PUSH,	// -- object, routine

	I_PUSH_STOP,		// -- routine id
	I_STOP,			// -E
	I_POP_STOP,		// --

	I_BEGIN_STATUS,		// -- value (height)
	I_END_STATUS,		// --
	I_WIN_WIDTH,		// -- dest

	I_BREAKPOINT,		// -E
	I_TRACEPOINT,		// -- file number, line number, predicate id, subop = kind

	N_OPCODES
};

enum {
	TR_ENTER,
	TR_QUERY,
	TR_MQUERY,
	TR_QDONE,
	TR_NOW,
	TR_NOTNOW,
	TR_REPORT,
	TR_LINE,
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
	VAL_NIL,
	VAL_PAIR,
	VAL_REF,
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
	OPER_STR,
	OPER_FILE,
	OPER_WORD
};

struct cinstr {
	uint8_t			op;
	uint8_t			subop;
	value_t			oper[3];
};

struct comp_routine {
	struct cinstr		*instr;
	uint16_t		ninstr;
	uint16_t		clause_id;	// so the debugger can print variable names
};

void comp_init();
void comp_dump_routine(struct program *prg, struct clause *cl, struct comp_routine *r);
void comp_dump_predicate(struct program *prg, struct predname *predname);
void comp_predicate(struct program *prg, struct predname *predname);
void comp_builtins(struct program *prg);
void comp_program(struct program *prg);
void comp_cleanup(void);
