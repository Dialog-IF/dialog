
struct dynamic {
	uint8_t			linkage_flags;
	line_t			linkage_due_to_line;
};

#define LINKF_SET	0x01
#define LINKF_RESET	0x02
#define LINKF_LIST	0x04
#define LINKF_CLEAR	0x08

typedef void (*dictmap_callback_t)(struct program *prg);

int body_fails(struct astnode *an);
int body_succeeds(struct astnode *an);
void frontend_add_builtins(struct program *prg);
int frontend(struct program *prg, int nfile, char **fname, dictmap_callback_t dictmap_callback);
int frontend_inject_query(struct program *prg, struct predname *predname, struct predname *tailpred, struct word *prompt, const uint8_t *str);
