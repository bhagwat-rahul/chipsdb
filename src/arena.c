#include "arena.h"

static size_t
align_up(size_t x, size_t align)
{
	size_t mask = align - 1;
	return (x + mask) & ~mask;
}

void
arena_init(Arena *arena, void *backing_memory, size_t backing_size)
{
	arena->base = (uint8_t *)backing_memory;
	arena->cap = backing_size;
	arena->pos = 0;
}

void
arena_reset(Arena *arena)
{
	arena->pos = 0;
}

void *
arena_alloc(Arena *arena, size_t size, size_t align)
{
	size_t aligned_pos;
	size_t new_pos;

	if (align == 0) {
		align = 1;
	}
	if ((align & (align - 1)) != 0) {
		return 0;
	}

	aligned_pos = align_up(arena->pos, align);
	new_pos = aligned_pos + size;
	if (new_pos > arena->cap) {
		return 0;
	}

	arena->pos = new_pos;
	return arena->base + aligned_pos;
}

ArenaFrame
arena_begin_frame(Arena *arena)
{
	ArenaFrame f;
	f.arena = arena;
	f.pos = arena->pos;
	return f;
}

void
arena_end_frame(ArenaFrame *frame)
{
	frame->arena->pos = frame->pos;
}
