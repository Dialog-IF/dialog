#include <stdlib.h>
#include <string.h>

#include "arena.h"

#define DEBUG_ALLOCATIONS 0

void arena_free(struct arena *arena) {
	struct arena_part *p, *nextp;

	for(p = arena->part; p; p = nextp) {
		nextp = p->next;
		free(p);
	}
	arena->part = 0;
}

void arena_init(struct arena *arena, int nominal_size) {
	arena->nominal_size = nominal_size;
	arena->part = malloc(sizeof(struct arena_part) - 1 + nominal_size);
	arena->part->size = nominal_size;
	arena->part->pos = 0;
	arena->part->next = 0;
}

void *arena_alloc(struct arena *arena, int size) {
	char *ptr;
	struct arena_part *p;

#if DEBUG_ALLOCATIONS
	p = malloc(sizeof(struct arena_part) - 1 + size);
	p->size = size;
	p->pos = size;
	p->next = arena->part;
	arena->part = p;
	ptr = p->data;
#else
	size = (size + 3) & ~3;
	if(size <= arena->part->size - arena->part->pos) {
		ptr = arena->part->data + arena->part->pos;
		arena->part->pos += size;
	} else {
		p = malloc(sizeof(struct arena_part) - 1 + arena->nominal_size + size);
		p->size = arena->nominal_size + size;
		p->pos = size;
		p->next = arena->part;
		arena->part = p;
		ptr = p->data;
	}
#endif

	return ptr;
}

void *arena_calloc(struct arena *arena, int size) {
	void *ptr;

	ptr = arena_alloc(arena, size);
	memset(ptr, 0, size);
	return ptr;
}

char *arena_strndup(struct arena *arena, char *str, int n) {
	char *buf = arena_alloc(arena, n + 1);

	memcpy(buf, str, n);
	buf[n] = 0;

	return buf;
}

char *arena_strdup(struct arena *arena, char *str) {
	return arena_strndup(arena, str, strlen(str));
}
