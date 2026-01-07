#include <stdio.h>
#include <stdlib.h>

#include "arena.h"
#include "lib_db.h"

int main(void)
{
	Arena arena;
	void *mem;

	LibDb lib;
	DbResult r;

	mem = malloc(64 * 1024 * 1024);
	if (!mem) {
		fprintf(stderr, "oom\n");
		return 1;
	}
	arena_init(&arena, mem, 64 * 1024 * 1024);

	lib_db_init(&lib, &arena);
	r = lib_db_alloc(&lib, 1024, 16384);
	if (r != DB_OK) {
		fprintf(stderr, "lib alloc failed\n");
		return 1;
	}

	r = lib_db_parse_lef_file(&lib, "data/min.lef");
	if (r != DB_OK) {
		fprintf(stderr, "lef parse failed\n");
		return 1;
	}

	if (lib.master_count != 1) {
		fprintf(stderr, "expected 1 master, got %u\n", lib.master_count);
		return 1;
	}
	if (lib.pin_count != 2) {
		fprintf(stderr, "expected 2 pins, got %u\n", lib.pin_count);
		return 1;
	}
	if (lib.dbu_per_micron != 1000) {
		fprintf(stderr, "expected dbu_per_micron=1000, got %d\n", lib.dbu_per_micron);
		return 1;
	}
	if (!lib.master_name[0]) {
		fprintf(stderr, "master name null\n");
		return 1;
	}
	if (lib.master_w[0] != 1000 || lib.master_h[0] != 2000) {
		fprintf(stderr, "expected size 1000x2000, got %dx%d\n",
			lib.master_w[0], lib.master_h[0]);
		return 1;
	}

	free(mem);
	return 0;
}
