
#define DF_ON		1
#define DF_CHANGED	2

struct dyn_var {
	value_t			*rendered;
	uint16_t		size;
	uint16_t		nalloc;
	uint8_t			changed;
};

struct dyn_flag {
	uint16_t		next;
	uint16_t		prev;
	uint8_t			value;
};

struct dyn_obj {
	struct dyn_flag		*flag;
	struct dyn_var		*var;
	uint16_t		sibling;
	uint16_t		child;
};

struct dyn_undo {
	struct arena		arena;
	uint8_t			*gflag;
	struct dyn_var		*gvar;
	struct dyn_obj		*obj;
	uint16_t		*first_in_oflag;
	int			ngflag;
	int			ngvar;
	int			nobj;
	int			nobjflag;
	int			nobjvar;
	int			ninput;
};

struct dyn_state {
	uint8_t			*gflag;
	struct dyn_var		*gvar;
	struct dyn_obj		*obj;
	uint16_t		*first_in_oflag;
	int			ngflag;
	int			ngvar;
	int			nobj;
	int			nobjflag;
	int			nobjvar;

	char			**inputlog;
	int			ninput;
	int			nalloc_input;

	struct dyn_undo		*undo;
	int			nalloc_undo;
	int			nundo;
	int			did_prune_undo;
};
