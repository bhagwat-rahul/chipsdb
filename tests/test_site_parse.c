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
	r = tech_db_alloc(&tech, 256, 256, 256);
	if (r != DB_OK) {
		fprintf(stderr, "tech alloc failed\n");
		return 1;
	}

	r = tech_db_parse_tech_lef_file(&tech, "data/min_tech.lef");
	if (r != DB_OK) {
		fprintf(stderr, "tech lef parse failed\n");
		return 1;
	}

	if (tech.site_count != 1) {
		fprintf(stderr, "expected 1 site got=%u\n", tech.site_count);
		return 1;
	}
	if (!tech.site_name[0]) {
		fprintf(stderr, "site name null\n");
		return 1;
	}
	if (tech.site_w[0] != 190 || tech.site_h[0] != 1400) {
		fprintf(stderr, "expected site 190x1400 got=%dx%d\n",
			tech.site_w[0], tech.site_h[0]);
		return 1;
	}

	free(mem);
	return 0;
}
