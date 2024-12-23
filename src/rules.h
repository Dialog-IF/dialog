
typedef uint32_t line_t;

#define MKLINE(file, line) (((file) << 24) | (line))
#define FILEPART(line) (sourcefile[(line) >> 24])
#define LINEPART(line) ((line) & 0xffffff)

extern char **sourcefile;
extern int nsourcefile;
extern int verbose;

struct word {
	struct word		*next_in_hash;
	char			*name;
	uint8_t			flags;
	uint16_t		obj_id;
	uint16_t		dict_id;
};

#define WORDF_DICT			1
#define WORDF_TAG			2

struct worldobj {
	struct astnode		*astnode;
	void			*backend;
};

enum {
	// char-valued tokens: [|](){}*~
	TOK_BAREWORD = 128,
	TOK_DICTWORD,
	TOK_TAG,
	TOK_VARIABLE,
	TOK_INTEGER,
	TOK_STARPAREN
};

struct token {
	uint8_t			kind;
	struct word		*word;
	int			value;
};

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
	AN_COLLECT_WORDS_CHECK,
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

struct astnode {
	uint8_t			kind;
	uint8_t			subkind;
	uint16_t		nchild;
	struct word		*word;
	int			value;
	struct predicate	*predicate;
	struct astnode		**children;
	struct astnode		*next_in_body;
	line_t			line;
	uint8_t			unbound;	// set if this expression can contain unbound variable(s) at runtime
};

struct clause {
	struct predicate	*predicate;
	struct astnode		**params;
	struct astnode		*body;
	line_t			line;
	int			negated;
	struct clause		*next_in_source;
	void			*backend;
};

enum {
	SP_NOW = 1,
	SP_JUST,
	SP_EXHAUST,

	SP_IF,
	SP_THEN,
	SP_ELSEIF,
	SP_ELSE,
	SP_ENDIF,

	SP_SELECT,
	SP_STOPPING,
	SP_RANDOM,
	SP_P_RANDOM,
	SP_T_RANDOM,
	SP_T_P_RANDOM,
	SP_CYCLING,

	SP_COLLECT,
	SP_COLLECT_WORDS,
	SP_INTO,
	SP_AND_CHECK,

	SP_STOPPABLE,
	SP_STATUSBAR,

	SP_OR,

	SP_GLOBAL_VAR,
	SP_GLOBAL_VAR_2,
	SP_GENERATE,
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
	BI_NUMBER,
	BI_LIST,
	BI_EMPTY,
	BI_NONEMPTY,
	BI_WORD,
	BI_UNBOUND,

	BI_QUIT,
	BI_RESTART,
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
	//BI_MEMINFO,
	BI_MEMSTATS,

	BI_WORDREP_RETURN,
	BI_WORDREP_SPACE,
	BI_WORDREP_BACKSPACE,
	BI_WORDREP_UP,
	BI_WORDREP_DOWN,
	BI_WORDREP_LEFT,
	BI_WORDREP_RIGHT,

	BI_PROGRAM_ENTRY,
	BI_ERROR_ENTRY,

	BI_STORY_IFID,
	BI_STORY_TITLE,
	BI_STORY_AUTHOR,
	BI_STORY_NOUN,
	BI_STORY_BLURB,
	BI_STORY_RELEASE,

	NBUILTIN
};

struct predlist {
	struct predlist		*next;
	struct predicate	*pred;
};

struct dynamic {
	uint8_t			initial_global_flag;	// for dynamic predicates of arity 0
	struct astnode		*initial_global_value;	// for global variables
	uint8_t			*initial_flag;		// for fixed flags & dynamic predicates of arity 1, indexed by worldobj number
	struct astnode		**initial_value;	// for dynamic predicates of arity 2, indexed by worldobj number
	int			first_param_use;
	struct astnode		*first_param_value;
	uint8_t			linkage_flags;
	line_t			dyn_due_to_line;
	line_t			linkage_due_to_line;
	int			global_bufsize;
	int			fixed_flag_count;	// number of objects having the flag set
};

#define LINKF_SET	0x01
#define LINKF_RESET	0x02
#define LINKF_LIST	0x04
#define LINKF_CLEAR	0x08

struct predicate {
	int			nword;
	struct word		**words;	// a null word indicates a parameter
	int			arity;
	int			nclause;
	struct clause		**clauses;
	struct clause		*macrodef;
	int			flags;
	int			special;
	int			builtin;
	void			*backend;
	uint32_t		unbound_in, unbound_out;
	struct clause		**unbound_in_due_to;
	struct predlist		*callers;
	struct dynamic		*dynamic;
	line_t			simple_due_to_line;
	line_t			invoked_at_line;
	char			*printed_name;	// for debug printouts
};

#define FPUSE_ONE_TAG		0x01
#define FPUSE_MANY_TAGS		0x02
#define FPUSE_OTHER		0x04

#define FPUSE_QUERY_VAR		0x10

#define PREDF_MACRO			0x0001
#define PREDF_DYNAMIC			0x0002
#define PREDF_INVOKED_NORMALLY		0x0004
#define PREDF_INVOKED_SHALLOW_WORDS	0x0008
#define PREDF_INVOKED_DEEP_WORDS	0x0010
#define PREDF_IN_QUEUE			0x0020
#define PREDF_VISITED			0x0040
#define PREDF_FIXED_FLAG		0x0080
#define PREDF_FAIL			0x0100
#define PREDF_SUCCEEDS			0x0200
#define PREDF_GLOBAL_VAR		0x0400
#define PREDF_META			0x0800
#define PREDF_INVOKED_MULTI		0x1000
#define PREDF_INVOKED_SIMPLE		0x2000
#define PREDF_OUTPUT			0x4000
#define PREDF_DEFINABLE_BI		0x8000

#define PREDF_INVOKED_FOR_WORDS (PREDF_INVOKED_SHALLOW_WORDS | PREDF_INVOKED_DEEP_WORDS)
#define PREDF_INVOKED (PREDF_INVOKED_NORMALLY | PREDF_INVOKED_FOR_WORDS)

extern struct worldobj **worldobjs;
extern int nworldobj;
extern struct predicate **predicates;
extern int npredicate;

struct astnode *mkast(int kind);
struct word *find_word(char *name);
struct predicate *find_predicate(int nword, struct word **words);
struct predicate *find_builtin(int id);
int contains_just(struct astnode *an);
int body_fails(struct astnode *an);
int body_succeeds(struct astnode *an);
void pp_expr(struct astnode *an);
void pp_body(struct astnode *an);
void pp_predname(struct predicate *pred);
void pp_clause(struct clause *cl);
void backend_add_dict(struct word *w);
int frontend(int nfile, char **fname);
