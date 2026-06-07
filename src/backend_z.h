
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

#define ZSCII_EXTEND 0 // Default: add new characters, don't replace any existing ones
#define ZSCII_DONT_EXTEND 1 // Don't add any new characters
#define ZSCII_REPLACE 2 // Purge the existing stock and replace them all
