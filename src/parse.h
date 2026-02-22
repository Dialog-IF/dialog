
enum {
	// char-valued tokens: [|](){}*~/
	TOK_BAREWORD = 128,
	TOK_DICTWORD,
	TOK_TAG,
	TOK_VARIABLE,
	TOK_INTEGER,
	TOK_STARPAREN,
	TOK_NOSPACE
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
	SP_ACCUMULATE,
	SP_INTO,
	SP_DETERMINE_OBJECT,
	SP_FROM_WORDS,
	SP_MATCHING_ALL_OF,

	SP_STOPPABLE,
	SP_DIV,
	SP_SPAN,
	SP_STATUSBAR,
	SP_STATUSBAR_OVERRIDE,
	SP_INLINE_STATUSBAR,
	SP_LINK_SELF,
	SP_LINK,
	SP_LINK_RES,
	SP_LOG,

	SP_OR,

	SP_GLOBAL_VAR,
	SP_GENERATE,
	SP_INTERFACE,
};

struct lexer {
	struct program		*program;
	FILE			*file;
	const uint8_t		*string;
	uint8_t			ungetbackslash;
	uint8_t			ungetcbuf;
	uint8_t			pending_nospace;
	uint8_t			kind;	// TOK_*
	struct word		*word;
	int			value;
	struct arena		temp_arena;
	uint32_t		wordcount;
	uint32_t		totallines;
	uint32_t		totalwords;
	int			errorflag;
	int			lib_file;
	uint8_t		unicode_escape[5]; // Used to implement \x1234 and \X123456
};

int parse_file(struct lexer *lexer, int filenum, struct clause ***clause_dest_ptr);
struct astnode *parse_injected_query(struct lexer *lexer, struct predicate *pred);
