
void prepare_dictionary_z_preserve(struct program *prg);
void prepare_dictionary_z_replace(struct program *prg);

void backend_z(
	char *filename,
	char *format,
	char *coverfname,
	char *coveralt,
	int heapsize,
	int auxsize,
	int ltssize,
	int strip,
	struct program *prg,
	struct arena *arena);
