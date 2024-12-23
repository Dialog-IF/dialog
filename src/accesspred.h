
int accesspred_valid_def(struct clause *cl, struct program *prg);
struct clause *accesspred_find_def(struct predicate *pred, struct astnode **params);
void accesspred_bind_vars(struct clause *def, struct astnode **params, struct astnode **table, struct program *prg, struct arena *arena);
struct astnode *expand_macro_body(struct astnode *an, struct clause *def, struct astnode **bindings, int instance, line_t line, struct program *prg, struct clause *container);
struct astnode *expand_macros(struct astnode *an, struct program *prg, struct clause *container);
