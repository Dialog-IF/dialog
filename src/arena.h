struct arena_part {
	struct arena_part	*next;
	int			size;
	int			pos;
	char			data[1];
};

struct arena {
	struct arena_part	*part;
	int			nominal_size;
};

void arena_init(struct arena *arena, int nominal_size);
void arena_free(struct arena *arena);
void *arena_alloc(struct arena *arena, int size);
void *arena_calloc(struct arena *arena, int size);
char *arena_strndup(struct arena *arena, char *str, int n);
char *arena_strdup(struct arena *arena, char *str);
