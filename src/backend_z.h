
void prepare_dictionary_z(struct program *prg);
void prepare_wordseps_z(const uint8_t *wordseps);

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
