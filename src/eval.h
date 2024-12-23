
#define EVAL_MULTI 0xffff

typedef struct prgpoint {
	struct predicate	*pred;
	uint16_t		routine;
} prgpoint_t;

struct env {
	value_t			*vars;
	value_t			*tracevars;
	prgpoint_t		cont;
	uint16_t		env;
	uint16_t		nvar;
	uint16_t		simple;
	uint16_t		level;
	uint16_t		ntracevar;
};

struct choice {
	uint16_t		env;		// env index
	uint16_t		envtop;
	uint16_t		trail;
	uint16_t		top;
	uint16_t		simple;
	value_t			arg[MAXPARAM + 1];
	value_t			orig_arg0;
	prgpoint_t		cont;
	prgpoint_t		nextcase;
};

struct eval_undo {
	struct arena		arena;
	struct env		*envstack;
	struct choice		*choicestack;
	value_t			*auxstack;
	uint16_t		*trailstack;
	value_t			*heap;
	uint8_t			*select;
	int			nselect;
	long			randomseed;
	prgpoint_t		cont;
	int			choice;
	int			env;
	uint16_t		aux;
	uint16_t		trail;
	uint16_t		top;
	uint16_t		stopchoice;
	uint16_t		stopaux;
	value_t			arg0; // where to put the 1 for $ComingBack
};

struct eval_state {
	struct program		*program;

	struct eval_dyn_cb	*dyn_callbacks;
	void			*dyn_callback_data;

	struct eval_undo	*undostack;
	int			nalloc_undo;
	int			nundo;

	struct env		*envstack;
	struct choice		*choicestack;
	value_t			*auxstack;
	uint16_t		*trailstack;	// heap index
	value_t			*heap;
	value_t			*temp;
	value_t			arg[MAXPARAM + 1];
	value_t			orig_arg0;	// for tracing an indexed argument
	uint16_t		nalloc_env;
	uint16_t		nalloc_choice;
	uint16_t		nalloc_aux;
	uint16_t		nalloc_trail;
	uint16_t		nalloc_heap;
	uint16_t		nalloc_temp;
	long			randomseed;

	prgpoint_t		resume;		// between calls to eval_run

	prgpoint_t		cont;
	int			env;
	int			choice;
	uint16_t		aux;
	uint16_t		trail;
	uint16_t		top;		// heap index
	uint16_t		stopchoice;	// choice index
	uint16_t		stopaux;	// aux index
	uint16_t		simple;		// choice index or EVAL_MULTI
	value_t			index;

	uint8_t			forwords;
	uint8_t			trace;
	uint8_t			divstyle;
};

struct eval_dyn_cb {
	value_t	(*get_globalvar)(struct eval_state *es, void *userdata, int dyn_id);
	int	(*set_globalvar)(struct eval_state *es, void *userdata, int dyn_id, value_t val);
	int	(*get_globalflag)(struct eval_state *es, void *userdata, int dyn_id);
	void	(*set_globalflag)(struct eval_state *es, void *userdata, int dyn_id, int val);
	int	(*get_objflag)(struct eval_state *es, void *userdata, int dyn_id, int obj_id);
	void	(*set_objflag)(struct eval_state *es, void *userdata, int dyn_id, int obj_id, int val);
	value_t	(*get_objvar)(struct eval_state *es, void *userdata, int dyn_id, int obj_id);
	int	(*set_objvar)(struct eval_state *es, void *userdata, int dyn_id, int obj_id, value_t val);
	int	(*get_first_child)(struct eval_state *es, void *userdata, int obj_id);
	int	(*get_next_child)(struct eval_state *es, void *userdata, int obj_id);
	int	(*get_first_oflag)(struct eval_state *es, void *userdata, int dyn_id);
	int	(*get_next_oflag)(struct eval_state *es, void *userdata, int dyn_id, int obj_id);
	void	(*clrall_objflag)(struct eval_state *es, void *userdata, int dyn_id);
	int	(*clrall_objvar)(struct eval_state *es, void *userdata, int dyn_id);
	void	(*dump_state)(struct eval_state *es, void *userdata);
	void	(*push_undo)(void *userdata);
	void	(*pop_undo)(struct eval_state *es, void *userdata);
};

enum {
	ESTATUS_FAILURE = 0,
	ESTATUS_ERR_HEAP = 1,
	ESTATUS_ERR_AUX = 2,
	ESTATUS_ERR_OBJ = 3,
	ESTATUS_ERR_SIMPLE = 4,
	ESTATUS_ERR_DYN = 5,
	ESTATUS_SUCCESS,
	ESTATUS_QUIT,
	ESTATUS_RESTART,
	ESTATUS_SAVE,
	ESTATUS_RESTORE,
	ESTATUS_SUSPENDED,
	ESTATUS_DEBUGGER,
	ESTATUS_GET_INPUT,
	ESTATUS_GET_RAW_INPUT,
	ESTATUS_GET_KEY,
	ESTATUS_RESUME,		// used internally by the debugger
	ESTATUS_UNDO		// used internally by the debugger
};

void ensure_fixed_values(struct eval_state *es, struct program *prg, struct predname *predname);
void pp_value(struct eval_state *es, value_t v, int with_at, int with_plus);
void init_evalstate(struct eval_state *es, struct program *prg);
void free_evalstate(struct eval_state *es);
void eval_reinitialize(struct eval_state *es);
value_t eval_makevar(struct eval_state *es);
value_t eval_makepair(value_t head, value_t tail, struct eval_state *es);
value_t eval_gethead(value_t v, struct eval_state *es);
value_t eval_gettail(value_t v, struct eval_state *es);
int eval_initial(struct eval_state *es, struct predname *predname, value_t *args);
int eval_program_entry(struct eval_state *es, struct predname *predname, value_t *args);
int eval_resume(struct eval_state *es, value_t arg);
int eval_injected_query(struct eval_state *es, struct predname *predname);
void eval_interrupt(); // called from the signal handler
