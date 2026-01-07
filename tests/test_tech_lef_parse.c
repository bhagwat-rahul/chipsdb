#include <stdio.h>
#include <stdlib.h>

#include "arena.h"
#include "tech_db.h"

int main(void)
{
	Arena arena;
	void *mem;

	TechDb tech;
	DbResult r;

	mem = malloc(64 * 1024 * 1024);
	if (!mem) {
		fprintf(stderr, "oom\n");
		return 1;
	}
	arena_init(&arena, mem, 64 * 1024 * 1024);

	tech_db_init(&tech, &arena);
	r = tech_db_alloc(&tech, 256, 256);
	if (r != DB_OK) {
		fprintf(stderr, "tech alloc failed\n");
		return 1;
	}

	r = tech_db_parse_tech_lef_file(&tech, "data/min_tech.lef");
	if (r != DB_OK) {
		fprintf(stderr, "tech lef parse failed\n");
		return 1;
	}

	if (tech.dbu_per_micron != 1000) {
		fprintf(stderr, "expected dbu_per_micron=1000 got=%d\n", tech.dbu_per_micron);
		return 1;
	}
	if (tech.layer_count != 2) {
		fprintf(stderr, "expected 2 layers got=%u\n", tech.layer_count);
		return 1;
	}
	if (tech.track_count != 2) {
		fprintf(stderr, "expected 2 tracks got=%u\n", tech.track_count);
		return 1;
	}

	/* M1: pitch 0.20 -> 200 dbu */
	if (tech.layer_pitch[0] != 200) {
		fprintf(stderr, "expected layer0 pitch=200 got=%d\n", tech.layer_pitch[0]);
		return 1;
	}

	free(mem);
	return 0;
}
