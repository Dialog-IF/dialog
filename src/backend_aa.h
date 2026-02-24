
void prepare_dictionary_aa(struct program *prg);
void prepare_wordseps_aa(const uint8_t *wordseps);

void backend_aa(
	char *filename,
	char *format,
	char *coverfname,
	char *coveralt,
	int heapsize,
	int auxsize,
	int ltssize,
	int strip,
	struct program *prg,
	struct arena *arena,
	char *resdir);

int aa_get_max_temp();
