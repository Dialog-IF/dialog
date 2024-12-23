#define MAXPARAM 12
#define MAXWORDMAP 10

struct word {
	struct word		*next_in_hash;
	char			*name;
	unsigned int		flags:2;
	unsigned int		word_id:30;
	uint16_t		obj_id;
	uint16_t		dict_id;
};

#define WORDF_DICT			1
#define WORDF_TAG			2

enum {
	AN_BLOCK,
	AN_NEG_BLOCK,
	AN_RULE,
	AN_NEG_RULE,
	AN_OR,

	AN_BAREWORD,
	AN_DICTWORD,
	AN_TAG,
	AN_VARIABLE,
	AN_INTEGER,
	AN_PAIR,
	AN_EMPTY_LIST,

	AN_IF,
	AN_NOW,
	AN_JUST,
	AN_EXHAUST,
	AN_FIRSTRESULT,
	AN_COLLECT,
	AN_COLLECT_WORDS,
	AN_DETERMINE_OBJECT,
	AN_SELECT,
	AN_STOPPABLE,
	AN_STATUSBAR,
};

enum {
	SEL_STOPPING,
	SEL_RANDOM,
	SEL_P_RANDOM,
	SEL_T_RANDOM,
	SEL_T_P_RANDOM,
	SEL_CYCLING
};

enum {
	RULE_SIMPLE,
	RULE_MULTI
};

enum {
	BI_LESSTHAN = 1,
	BI_GREATERTHAN,
	BI_PLUS,
	BI_MINUS,
	BI_TIMES,
	BI_DIVIDED,
	BI_MODULO,
	BI_RANDOM,

	BI_FAIL,
	BI_STOP,
	BI_REPEAT,

	BI_NUMBER,
	BI_LIST,
	BI_EMPTY,
	BI_NONEMPTY,
	BI_WORD,
	BI_UNBOUND,

	BI_QUIT,
	BI_RESTART,
	BI_BREAKPOINT,
	BI_SAVE,
	BI_RESTORE,
	BI_SAVE_UNDO,
	BI_UNDO,
	BI_SCRIPT_ON,
	BI_SCRIPT_OFF,

	BI_TRACE_ON,
	BI_TRACE_OFF,
	BI_NOSPACE,
	BI_SPACE,
	BI_SPACE_N,
	BI_LINE,
	BI_PAR,
	BI_PAR_N,
	BI_ROMAN,
	BI_BOLD,
	BI_ITALIC,
	BI_REVERSE,
	BI_FIXED,
	BI_UPPER,
	BI_CLEAR,
	BI_CLEAR_ALL,
	BI_WINDOWWIDTH,
	BI_CURSORTO,

	BI_OBJECT,
	BI_GETINPUT,
	BI_GETRAWINPUT,
	BI_GETKEY,
	BI_HASPARENT,

	BI_UNIFY,
	BI_IS_ONE_OF,
	BI_SPLIT,

	BI_SERIALNUMBER,
	BI_COMPILERVERSION,
	BI_MEMSTATS,

	BI_WORDREP_RETURN,
	BI_WORDREP_SPACE,
	BI_WORDREP_BACKSPACE,
	BI_WORDREP_UP,
	BI_WORDREP_DOWN,
	BI_WORDREP_LEFT,
	BI_WORDREP_RIGHT,

	BI_HAVE_UNDO,

	BI_PROGRAM_ENTRY,
	BI_ERROR_ENTRY,

	BI_STORY_IFID,
	BI_STORY_TITLE,
	BI_STORY_AUTHOR,
	BI_STORY_NOUN,
	BI_STORY_BLURB,
	BI_STORY_RELEASE,

	BI_ENDINGS,

	BI_INJECTED_QUERY,
	BI_BREAKPOINT_AGAIN,
	BI_BREAK_GETKEY,
	BI_BREAK_FAIL,

	NBUILTIN
};

struct astnode {
	uint8_t			kind;
	uint8_t			subkind;
	uint16_t		nchild;
	line_t			line;
	struct astnode		**children;
	struct astnode		*next_in_body;

	// todo: these should be in a union
	struct word		*word;
	int			value;
	struct predname		*predicate;

	uint8_t			unbound;	// set if this expression can contain unbound variable(s) at runtime
};

struct clause {
	struct predname		*predicate;
	struct arena		*arena;
	struct astnode		**params;
	struct astnode		*body;
	line_t			line;
	int			negated;
	struct clause		*next_in_source;
	void			*backend;
	struct word		**varnames;
	uint16_t		nvar;
	uint16_t		clause_id;
	uint8_t			max_call_arity;
	char			*structure;
};

struct predname {
	uint16_t		pred_id;
	uint16_t		arity;
	uint16_t		nword;
	uint16_t		nameflags;
	struct word		**words;	// a null word indicates a parameter
	char			*printed_name;	// for debug printouts
	struct predicate	*pred;		// current implementation
	struct predicate	*old_pred;	// previous implementation (for select recovery)
	uint8_t			*fixedvalues;	// for PREDF_FIXED_FLAG, indexed by obj_id
	uint16_t		nfixedvalue;
	uint16_t		special;
	uint16_t		builtin;
	uint16_t		dyn_id;		// global flags, per-object flags, per-object variables
	uint16_t		dyn_var_id;	// global variables
	int			total_refcount;
};

#define DYN_NONE		0xffff
#define DYN_HASPARENT		0		// this is an objvar id

#define PREDNF_OUTPUT			0x0001
#define PREDNF_META			0x0002
#define PREDNF_DEFINABLE_BI		0x0004

struct predicate {
	struct clause		**clauses;
	struct clause		*macrodef;
	uint32_t		flags;
	uint16_t		nclause;
	uint16_t		unbound_in;
	uint16_t		unbound_out;
	uint16_t		nselectclause;
	uint16_t		*selectclauses;
	uint16_t		nwordmap;
	struct wordmap		*wordmaps;
	void			*backend;
	struct clause		**unbound_in_due_to;
	struct predlist		*callers;
	struct dynamic		*dynamic;
	line_t			invoked_at_line;
	int			refcount;
	struct comp_routine	*routines;
	struct arena		arena;
	int			nroutine;
	int			normal_entry;
	int			initial_value_entry;
	struct predname		*predname;
};

#define PREDF_MACRO			0x00000001
#define PREDF_DYNAMIC			0x00000002
#define PREDF_INVOKED_BY_PROGRAM	0x00000004
#define PREDF_INVOKED_FOR_WORDS		0x00000008
#define PREDF_INVOKED_BY_DEBUGGER	0x00000010
#define PREDF_VISITED			0x00000020
#define PREDF_FIXED_FLAG		0x00000080
#define PREDF_FAIL			0x00000100
#define PREDF_SUCCEEDS			0x00000200
#define PREDF_GLOBAL_VAR		0x00000400
#define PREDF_IN_QUEUE			0x00000800
#define PREDF_INVOKED_MULTI		0x00001000
#define PREDF_INVOKED_SIMPLE		0x00002000
#define PREDF_CONTAINS_JUST		0x00004000
#define PREDF_STOP			0x00008000
#define PREDF_DEFINED			0x00010000

#define PREDF_INVOKED_NORMALLY (PREDF_INVOKED_BY_PROGRAM | PREDF_INVOKED_BY_DEBUGGER)
#define PREDF_INVOKED (PREDF_INVOKED_NORMALLY | PREDF_INVOKED_FOR_WORDS)

struct wordmap_tally {
	uint16_t		onumtable[MAXWORDMAP];
	uint16_t		count;
};

struct wordmap {
	int			nmap;
	uint16_t		*dict_ids;
	struct wordmap_tally	*objects;
};

struct endings_point {
	int			nway;
	struct endings_way	**ways;
};

struct endings_way {
	uint16_t		letter;
	uint16_t		final;	// bool
	struct endings_point	more;
};

#define WORDBUCKETS 1024

typedef void (*program_ticker_t)();

struct program {
	struct arena		arena;
	struct word		*wordhash[WORDBUCKETS];
	int			nextfresh;
	struct predname		**predicates;
	struct word		**allwords;;
	struct word		**worldobjnames;
	struct word		**dictwordnames;
	struct predname		**globalflagpred;
	struct predname		**globalvarpred;
	struct predname		**objflagpred;
	struct predname		**objvarpred;
	uint8_t			*select;
	uint32_t		optflags;
	int			did_warn_about_repeat; // prevent multiple warnings
	int			nword;
	int			npredicate;
	int			nworldobj;
	int			ndictword;
	int			nglobalflag;
	int			nglobalvar;
	int			nobjflag;
	int			nobjvar;
	int			nselect;
	int			nalloc_dictword;
	int			nalloc_word;
	int			nalloc_select;
	struct endings_point	endings_root;
	struct arena		endings_arena;
	int			totallines;
	int			errorflag;
	program_ticker_t	eval_ticker;
	int			nwordmappred;
};

#define OPTF_BOUND_PARAMS	0x00000001

typedef void (*word_visitor_t)(struct word *);

struct program *new_program(void);
struct astnode *mkast(int kind, int nchild, struct arena *arena, line_t line);
struct astnode *deepcopy_astnode(struct astnode *an, struct arena *arena, line_t line);
struct word *find_word(struct program *prg, char *name);
struct word *find_word_nocreate(struct program *prg, char *name);
struct word *fresh_word(struct program *prg);
void pred_clear(struct predname *predname);
struct predname *find_predicate(struct program *prg, int nword, struct word **words);
struct predname *find_builtin(struct program *prg, int id);
void pp_expr(struct astnode *an);
void pp_body(struct astnode *an);
void pp_clause(struct clause *cl);
void pp_predicate(struct predname *predname, struct program *prg);
int contains_just(struct astnode *an);
void free_program(struct program *prg);
void create_worldobj(struct program *prg, struct word *w);
void pred_claim(struct predicate *pred);
void pred_release(struct predicate *pred);
