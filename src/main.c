#include <stdio.h>
#include <stdlib.h>
#include "arena.h"
#include "lib_db.h"

int main(int argc, char **argv)
{
	Arena arena;
	void *mem;

	if (argc != 2) {
		fprintf(stderr, "usage:\n");
		fprintf(stderr, "  chipsdb <lib.lef>\n");
		return 2;
	}

	mem = malloc(64 * 1024 * 1024);
	if (!mem) {
		fprintf(stderr, "oom\n");
		return 1;
	}
	arena_init(&arena, mem, 64 * 1024 * 1024);

	{
		LibDb lib;
		DbResult r;

		lib_db_init(&lib, &arena);
		r = lib_db_alloc(&lib, 1024, 16384);
		if (r != DB_OK) {
			fprintf(stderr, "lib alloc failed\n");
			free(mem);
			return 1;
		}

		r = lib_db_parse_lef_file(&lib, argv[1]);
		if (r != DB_OK) {
			fprintf(stderr, "lef parse failed\n");
			free(mem);
			return 1;
		}

		printf("LEF parsed: masters=%u pins=%u dbu_per_micron=%d\n",
			lib.master_count,
			lib.pin_count,
			lib.dbu_per_micron);
	}

	free(mem);
	return 0;
}
