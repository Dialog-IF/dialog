#define MAXPARAM 12
#define MAXWORDMAP 10

struct word {
	struct word		*next_in_hash;
	char			*name;
	unsigned int		flags:3;
	unsigned int		word_id:29;
	uint16_t		obj_id;
	uint16_t		dict_id;
};

#define WORDF_DICT			1
#define WORDF_TAG			2
#define WORDF_OUTPUT			4

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
	AN_ACCUMULATE,
	AN_DETERMINE_OBJECT,
	AN_SELECT,
	AN_STOPPABLE,
	AN_STATUSBAR,
	AN_OUTPUTBOX,
	AN_LINK_SELF,
	AN_LINK,
	AN_LINK_RES,
	AN_LOG,

	AN_REPORT_RULE
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
	BOX_DIV,
	BOX_SPAN,
	BOX_STATUS
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
	BI_UNKNOWN_WORD,
	BI_BOUND,
	BI_FULLY_BOUND,

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
	BI_UNSTYLE,
	BI_ROMAN,
	BI_BOLD,
	BI_ITALIC,
	BI_REVERSE,
	BI_FIXED,
	BI_UPPER,
	BI_CLEAR,
	BI_CLEAR_ALL,
	BI_CLEAR_LINKS,
	BI_PROGRESS_BAR,

	BI_OBJECT,
	BI_GETINPUT,
	BI_GETRAWINPUT,
	BI_GETKEY,
	BI_HASPARENT,

	BI_UNIFY,
	BI_IS_ONE_OF,
	BI_SPLIT,
	BI_APPEND,

	BI_SPLIT_WORD,
	BI_JOIN_WORDS,

	BI_SERIALNUMBER,
	BI_COMPILERVERSION,
	BI_MEMSTATS,

	BI_HAVE_UNDO,
	BI_HAVE_LINK,
	BI_HAVE_QUIT,
	BI_CAN_EMBED,
	BI_CAN_EMBED_INTERNAL,

	BI_PROGRAM_ENTRY,
	BI_ERROR_ENTRY,

	BI_QUERY,
	BI_QUERY_ARG,
	BI_INVOKE_CLOSURE,

	BI_STORY_IFID,
	BI_STORY_TITLE,
	BI_STORY_AUTHOR,
	BI_STORY_NOUN,
	BI_STORY_BLURB,
	BI_STORY_RELEASE,

	BI_ENDINGS,
	BI_STYLEDEF,
	BI_RESOURCEDEF,
	BI_RESOLVERESOURCE,
	BI_EMBEDRESOURCE,
	BI_EMBED_INTERNAL,

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

struct clause_code {
	struct clause_code	*next;
	int16_t			ndrop_arg0;
	int16_t			ndrop_body;
	int			routine_id:31;
	int			ignore_arg0:1;
};

struct clause {
	struct predname		*predicate;
	struct arena		*arena;
	struct astnode		**params;
	struct astnode		*body;
	struct clause		*next_in_source;
	void			*backend;
	struct word		**varnames;
	char			*structure;
	struct clause_code	*entrypoints;
	line_t			line;
	uint16_t		nvar;
	uint16_t		next_temp;
	uint16_t		clause_id;
	uint16_t		macro_instance;
	uint8_t			max_call_arity;
	uint8_t			negated;
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

#define PREDNF_META		0x0001
#define PREDNF_DEFINABLE_BI	0x0002

struct predicate {
	struct clause		**clauses;
	struct clause		**macrodefs;
	uint32_t		flags;
	uint16_t		nclause;
	uint16_t		nmacrodef;
	uint16_t		unbound_in;
	uint16_t		unbound_out;
	uint16_t		nselectclause;
	uint16_t		*selectclauses;
	uint16_t		nwordmap;
	uint16_t		nroutine;
	struct wordmap		*wordmaps;
	void			*backend;
	struct clause		**unbound_in_due_to;
	struct clause		**unbound_out_due_to;
	struct predlist		*callers;
	struct dynamic		*dynamic;
	line_t			invoked_at_line;
	int			refcount;
	struct comp_routine	*routines;
	struct arena		arena;
	int			normal_entry;
	int			initial_value_entry;
	struct predname		*predname;
	struct clause		*iface_decl;
	uint16_t		iface_bound_in;
	uint16_t		iface_bound_out;
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
#define PREDF_DYN_LINKAGE		0x00020000
#define PREDF_INVOKED_NORMALLY		0x00040000
#define PREDF_NEEDS_LABEL		0x00080000
#define PREDF_MIGHT_STOP		0x00100000
#define PREDF_MAY_INLINE		0x00200000

#define PREDF_INVOKED (PREDF_INVOKED_NORMALLY | PREDF_INVOKED_FOR_WORDS | PREDF_INVOKED_BY_PROGRAM | PREDF_INVOKED_BY_DEBUGGER)

struct wordmap_tally {
	uint16_t		key;	// dict_id or backend-specific value, 0xffff = any
	uint16_t		count;
	uint16_t		onumtable[MAXWORDMAP];
};

struct wordmap {
	int			nmap;
	struct wordmap_tally	*map;
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

struct boxclassline {
	struct boxclassline	*next;
	char			*data;
};

struct boxclass {
	uint16_t		width, height;
	uint16_t		margintop, marginbottom;
	uint16_t		flags;
	uint16_t		style;	// STYLE_*
	struct word		*class;
	struct boxclassline	*css_lines;
};

#define BOXF_RELWIDTH		0x0001
#define BOXF_RELHEIGHT		0x0002
#define BOXF_FLOATLEFT		0x0004
#define BOXF_FLOATRIGHT		0x0008

struct extresource {
	char			*url;
	char			*stem;
	char			*path;
	char			*options;
	char			*alt;
	line_t			line;
};

#define WORDBUCKETS 1024

typedef void (*program_ticker_t)();

struct program {
	struct arena		arena;
	struct word		*wordhash[WORDBUCKETS];
	int			nextfresh;
	struct predname		**predicates;
	struct word		**allwords;
	struct word		**worldobjnames;
	struct word		**dictwordnames;
	struct boxclass		*boxclasses;
	uint16_t		*dictmap; // from dict_id to backend-specific tagged word
	struct predname		**globalflagpred;
	struct predname		**globalvarpred;
	struct predname		**objflagpred;
	struct predname		**objvarpred;
	struct astnode		**closurebodies;
	struct extresource	*resources;
	uint8_t			*select;
	uint32_t		optflags;
	int			did_warn_about_repeat; // prevent multiple warnings
	int			nword;
	int			npredicate;
	int			nworldobj;
	int			ndictword;
	int			nboxclass;
	int			nglobalflag;
	int			nglobalvar;
	int			nobjflag;
	int			nobjvar;
	int			nclosurebody;
	int			nselect;
	int			nalloc_dictword;
	int			nalloc_word;
	int			nresource;
	int			nalloc_select;
	struct endings_point	endings_root;
	struct arena		endings_arena;
	int			totallines;
	int			errorflag;
	program_ticker_t	eval_ticker;
	int			nwordmappred;
	struct word		**clausevars;
	uint8_t			*clausevarcounts;
	int			nclausevar;
	int			nalloc_var;
	char			*meta_author;
	char			*meta_title;
	char			*meta_noun;
	char			*meta_blurb;
	char			*meta_ifid;
	char			*meta_serial;
	char			*meta_reldate;
	int			meta_release;
	uint16_t		max_temp;
};

#define OPTF_BOUND_PARAMS	0x00000001
#define OPTF_TAIL_CALLS		0x00000002
#define OPTF_NO_TRACE		0x00000004
#define OPTF_ENV_FRAMES		0x00000008
#define OPTF_SIMPLE_SELECT	0x00000010
#define OPTF_NO_LINKS		0x00000020
#define OPTF_NO_LOG		0x00000040
#define OPTF_INLINE		0x00000080

typedef void (*word_visitor_t)(struct word *);

struct program *new_program(void);
struct astnode *mkast(int kind, int nchild, struct arena *arena, line_t line);
struct astnode *deepcopy_astnode(struct astnode *an, struct arena *arena, line_t line);
struct clause *mkclause(struct predicate *pred);
int astnode_equals(struct astnode *a, struct astnode *b);
struct word *find_word(struct program *prg, char *name);
struct word *find_word_nocreate(struct program *prg, char *name);
void ensure_dict_word(struct program *prg, struct word *w);
struct word *fresh_word(struct program *prg);
int find_boxclass(struct program *prg, struct word *w);
void pred_clear(struct predname *predname);
struct predname *find_predicate(struct program *prg, int nword, struct word **words);
struct predname *find_builtin(struct program *prg, int id);
int find_closurebody(struct program *prg, struct astnode *an, int *did_create);
void analyse_clause(struct program *prg, struct clause *cl, int report_singletons);
int findvar(struct clause *cl, struct word *w);
void add_clause(struct clause *cl, struct predicate *pred);
void pp_expr(struct astnode *an);
void pp_body(struct astnode *an);
void pp_clause(struct clause *cl);
void pp_predicate(struct predname *predname, struct program *prg);
int contains_just(struct astnode *an);
void free_program(struct program *prg);
void create_worldobj(struct program *prg, struct word *w);
void pred_claim(struct predicate *pred);
void pred_release(struct predicate *pred);
