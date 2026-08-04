#ifndef CHIPSDB_ARENA_H
#define CHIPSDB_ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef struct Arena {
	uint8_t *base;
	size_t cap;
	size_t pos;
} Arena;

typedef struct ArenaFrame {
	Arena *arena;
	size_t pos;
} ArenaFrame;

void arena_init(Arena *arena, void *backing_memory, size_t backing_size);
void arena_reset(Arena *arena);

void *arena_alloc(Arena *arena, size_t size, size_t align);

ArenaFrame arena_begin_frame(Arena *arena);
void arena_end_frame(ArenaFrame *frame);

#endif
