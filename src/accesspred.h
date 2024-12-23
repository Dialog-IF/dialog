
struct astnode *expand_macro_body(struct astnode *an, struct clause *def, struct astnode **bindings, int instance, line_t line, struct program *prg, struct arena *arena);
struct astnode *expand_macros(struct astnode *an, struct program *prg, struct arena *arena);
